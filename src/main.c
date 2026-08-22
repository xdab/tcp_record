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

#define SAMPLE_BUF_SIZE 4096

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
    {
        fprintf(stderr, "Connecting to %s:%d (format=%s, %s-endian)...\n",
                opts.addr, opts.port, sample_format_name(opts.format),
                opts.endianness == ENDIAN_BE ? "big" : "little");
    }

    net_state_t *net = net_connect(opts.addr, opts.port, opts.format, opts.endianness);
    if (!net)
        return EXIT_FAILURE;

    /* init squelch if threshold > 0 or calibrate mode */
    squelch_state_t sql;
    int sql_enabled = (opts.squelch_level > 0) || opts.calibrate;
    if (sql_enabled)
    {
        float open_th = (float)opts.squelch_level / 1000.0f;
        float close_th = opts.squelch_close_level > 0
                             ? (float)opts.squelch_close_level / 1000.0f
                             : open_th;
        squelch_init(&sql, opts.squelch_mode, opts.sample_rate, open_th, close_th);
        if (!opts.calibrate)
            fprintf(stderr, "Squelch: enabled (open=%.4f, close=%.4f)\n", open_th, close_th);
        else
            fprintf(stderr, "Calibrate: running (open=%.4f, close=%.4f)\n", open_th, close_th);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    float samples[SAMPLE_BUF_SIZE];
    int16_t outbuf[SAMPLE_BUF_SIZE];
    wav_t *wav = NULL;

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
            {
                cal_accumulate(&cal, sql.envelope, n);
                if (cal_should_print(&cal))
                    cal_print(&cal);
            }
            else
            {
                if (sql.open != was_open)
                {
                    if (sql.open)
                    {
                        /* squelch opened - start recording */
                        if (opts.auto_record)
                        {
                            time_t now = time(NULL);
                            struct tm *tm = localtime(&now);
                            char path[512];
                            if (opts.rec_dir[0] != '\0')
                                snprintf(path, sizeof(path), "%s/", opts.rec_dir);
                            else
                                path[0] = '\0';
                            int end = strlen(path);
                            strftime(path + end, sizeof(path) - end, "REC_%Y-%m-%d_%H-%M-%S", tm);
                            end = strlen(path);
                            snprintf(path + end, sizeof(path) - end, "_%d.wav", opts.port);
                            wav = wav_open(path, opts.sample_rate, 1, 16);
                            if (wav)
                                fprintf(stderr, "Squelch opened -> recording to %s\n", path);
                            else
                                fprintf(stderr, "Squelch opened -> failed to open %s\n", path);
                        }
                    }
                    else
                    {
                        /* squelch closed - stop recording */
                        if (wav)
                        {
                            wav_close(wav);
                            wav = NULL;
                            fprintf(stderr, "Squelch closed -> recording stopped\n");
                        }
                    }
                }
            }
        }

        float_to_s16le(samples, outbuf, n);
        if (wav)
            wav_write(wav, outbuf, n * sizeof(int16_t));
        if (opts.stdout_output && write_all(STDOUT_FILENO, outbuf, n * sizeof(int16_t)) < 0)
        {
            if (!do_exit)
                fprintf(stderr, "Write error (broken pipe?)\n");
            break;
        }
    }

    if (wav)
        wav_close(wav);
    net_close(net);
    fprintf(stderr, "Exiting...\n");
    return 0;
}
