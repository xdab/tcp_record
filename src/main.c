#define _XOPEN_SOURCE 700

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "calibration.h"
#include "format.h"
#include "network.h"
#include "options.h"
#include "squelch.h"
#include "types.h"
#include "wav.h"

#define SAMPLE_BUF_SIZE 2048

volatile int do_exit = 0;

static void sighandler(int signum)
{
    (void)signum;
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
}

static int write_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    while (len > 0)
    {
        ssize_t n = write(fd, p, len);
        if (n < 0)
            return -1;
        p += n;
        len -= n;
    }
    return 0;
}

static wav_t *start_recording(const options_t *opts, char *path_out)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char path[512];

    if (opts->rec_dir[0] != '\0')
        snprintf(path, sizeof(path), "%s/", opts->rec_dir);
    else
        path[0] = '\0';

    int end = strlen(path);
    snprintf(path + end, sizeof(path) - end, "%s_%d_", opts->label, opts->port);
    end = strlen(path);
    strftime(path + end, sizeof(path) - end, "%Y%m%d_%H%M%S", tm);
    end = strlen(path);
    snprintf(path + end, sizeof(path) - end, ".wav");

    if (path_out)
        snprintf(path_out, 512, "%s", path);

    wav_t *wav = wav_open(path, opts->sample_rate, 1, 16);
    if (wav)
        fprintf(stderr, "Squelch opened -> recording to %s\n", path);
    else
        fprintf(stderr, "Squelch opened -> failed to open %s\n", path);
    return wav;
}

static void stop_recording(wav_t **wav)
{
    if (!*wav)
        return;
    wav_close(*wav);
    *wav = NULL;
    fprintf(stderr, "Squelch closed -> recording stopped\n");
}

static void discard_recording(wav_t **wav, const char *path)
{
    if (!*wav)
        return;
    wav_close(*wav);
    *wav = NULL;
    if (path[0] != '\0')
        unlink(path);
}

static void handle_calibration(cal_state_t *cal, squelch_state_t *sql, int n)
{
    float env = sql->envelope;
    if (sql->env_norm > 0.0f)
        env *= sql->env_norm;
    cal_accumulate(cal, env, n);
    if (cal_should_print(cal))
        cal_print(cal);
}

int main(int argc, char **argv)
{
    struct sigaction sigact;
    options_t opts;

    if (options_parse(argc, argv, &opts) != 0)
        return EXIT_FAILURE;

    if (opts.addr[0] == '\0' || opts.port == 0)
    {
        fprintf(stderr, "Error: address and port are required.\n");
        options_usage();
    }

    if (opts.debug)
        fprintf(stderr, "Connecting to %s:%d (format=%s, %s-endian)...\n",
                opts.addr, opts.port, sample_format_name(opts.format),
                opts.endianness == ENDIAN_BE ? "big" : "little");

    net_state_t *net = net_connect(opts.addr, opts.port, opts.format, opts.endianness);
    if (!net)
        return EXIT_FAILURE;

    squelch_state_t sql;
    int sql_enabled = (opts.squelch_level > 0) || opts.calibrate;
    if (sql_enabled)
    {
        float open_th = (float)opts.squelch_level / 1000.0f;
        float close_th = opts.squelch_close_level > 0
                             ? (float)opts.squelch_close_level / 1000.0f
                             : open_th;
        squelch_init(&sql, opts.squelch_mode, opts.sample_rate, open_th, close_th,
                     opts.signal_bw);
        float norm = sql.env_norm > 0.0f ? 1.0f / sql.env_norm : 0.0f;
        if (opts.calibrate)
            fprintf(stderr, "Calibrate: running (open=%.4f, close=%.4f, bw=%d Hz, norm=%.2f)\n",
                    open_th, close_th, opts.signal_bw, norm);
        else
            fprintf(stderr, "Squelch: enabled (open=%.4f, close=%.4f, bw=%d Hz, norm=%.2f)\n",
                    open_th, close_th, opts.signal_bw, norm);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    float samples[SAMPLE_BUF_SIZE];
    int16_t outbuf[SAMPLE_BUF_SIZE];
    wav_t *wav = NULL;
    char rec_path[512] = {0};
    int recording_samples = 0;
    int write_skip = 0;

    if (opts.auto_record && opts.rec_dir[0] != '\0')
        fprintf(stderr, "Auto-record: WAVs will be saved to %s\n", opts.rec_dir);
    else if (opts.auto_record)
        fprintf(stderr, "Auto-record: WAVs will be saved to cwd\n");

    cal_state_t cal;
    if (opts.calibrate)
        cal_init(&cal, opts.sample_rate);

    while (!do_exit)
    {
        int n = net_recv_samples(net, samples, SAMPLE_BUF_SIZE);
        if (n <= 0)
        {
            if (n == 0)
                fprintf(stderr, "TCP: connection closed by server.\n");
            else
                fprintf(stderr, "TCP: recv error\n");
            break;
        }

        if (sql_enabled)
        {
            int was_open = sql.open;
            squelch_process(&sql, samples, samples, n);

            if (opts.calibrate)
                handle_calibration(&cal, &sql, n);
            else if (sql.open != was_open)
            {
                if (sql.open)
                {
                    recording_samples = 0;
                    write_skip = sql.open_idx;
                    wav = start_recording(&opts, rec_path);
                }
                else
                {
                    float duration = (float)recording_samples / (float)opts.sample_rate;
                    if (opts.min_duration > 0.0f && duration < opts.min_duration)
                    {
                        discard_recording(&wav, rec_path);
                        fprintf(stderr, "Squelch closed -> discarded (%.2fs < %.2fs)\n",
                                duration, opts.min_duration);
                    }
                    else
                        stop_recording(&wav);
                }
            }
        }

        float_to_s16le(samples, outbuf, n);
        if (wav)
        {
            int write_n = n - write_skip;
            if (write_n > 0)
            {
                wav_write(wav, outbuf + write_skip, write_n * sizeof(int16_t));
                recording_samples += write_n;
            }
            write_skip = 0;
        }
        if (opts.stdout_output && write_all(STDOUT_FILENO, outbuf, n * sizeof(int16_t)) < 0)
        {
            if (!do_exit)
                fprintf(stderr, "Write error (broken pipe?)\n");
            break;
        }
    }

    if (wav)
    {
        float duration = (float)recording_samples / (float)opts.sample_rate;
        if (opts.min_duration > 0.0f && duration < opts.min_duration)
        {
            discard_recording(&wav, rec_path);
            fprintf(stderr, "Exiting -> discarded short recording (%.2fs < %.2fs)\n",
                    duration, opts.min_duration);
        }
        else
            stop_recording(&wav);
    }
    net_close(net);
    fprintf(stderr, "Exiting...\n");
    return 0;
}
