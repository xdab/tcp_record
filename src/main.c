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
#include "recording.h"
#include "squelch.h"
#include "tsql.h"
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

static void handle_calibration(cal_state_t *cal, const float *env, int n)
{
    for (int i = 0; i < n; i++)
        cal_accumulate(cal, env[i], 1);
}

static void handle_gate_transition(int open, int skip, options_t *opts,
                                   wav_t **wav, char *temp_path,
                                   char *final_path, int *recording_samples,
                                   int *write_skip)
{
    *write_skip = skip;
    if (!open)
    {
        float duration = (float)*recording_samples / (float)opts->sample_rate;
        if (*wav && opts->min_duration > 0.0f && duration < opts->min_duration)
        {
            discard_recording(wav, temp_path);
            fprintf(stderr, "Squelch closed -> discarded (%.2fs < %.2fs)\n",
                    duration, opts->min_duration);
            return;
        }
        stop_recording(wav, temp_path, final_path);
        return;
    }
    *recording_samples = 0;
    *wav = start_recording(opts, temp_path, final_path);
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

    int tsql_enabled = opts.tsql_tone > 0.0f;
    tsql_state_t tsql;
    if (tsql_enabled)
    {
        tsql_init(&tsql, opts.tsql_tone, opts.sample_rate,
                  opts.tsql_level_dbfs, opts.tsql_delay_ms);
        fprintf(stderr, "TSQL: %.1f Hz gate (level=%.0f dBFS, delay=%.0f ms)\n",
                opts.tsql_tone, opts.tsql_level_dbfs, opts.tsql_delay_ms);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    float samples[SAMPLE_BUF_SIZE];
    float env_buf[SAMPLE_BUF_SIZE];
    int16_t outbuf[SAMPLE_BUF_SIZE];
    wav_t *wav = NULL;
    char temp_path[512] = {0};
    char final_path[512] = {0};
    int recording_samples = 0;
    int write_skip = 0;

    if (opts.auto_record && opts.rec_dir[0] != '\0')
        fprintf(stderr, "Auto-record: WAVs will be saved to %s\n", opts.rec_dir);
    else if (opts.auto_record)
        fprintf(stderr, "Auto-record: WAVs will be saved to cwd\n");

    cal_state_t cal;
    if (opts.calibrate)
        cal_init(&cal, opts.sample_rate);

    int gate_open = 1;
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

        int prev_sql_open = sql_enabled ? sql.open : 1;
        int prev_tsql_open = tsql_enabled ? tsql.open : 1;

        if (tsql_enabled)
            tsql_process(&tsql, samples, n);

        if (sql_enabled)
        {
            squelch_process(&sql, samples, samples, n,
                            opts.calibrate ? env_buf : NULL);
            if (opts.calibrate)
            {
                handle_calibration(&cal, env_buf, n);
                if (cal_should_print(&cal))
                {
                    cal_print(&cal);
                    if (tsql_enabled)
                        fprintf(stderr,
                                "TSQL %.1f Hz: %.1f dBFS (gate %s)\n",
                                opts.tsql_tone, tsql.tick_dbfs,
                                tsql.open ? "open" : "closed");
                }
            }
        }

        int gate = (!sql_enabled || sql.open) && (!tsql_enabled || tsql.open);
        if (!opts.calibrate && gate != gate_open)
        {
            int skip = 0;
            if (gate)
            {
                int sql_rose = sql_enabled && sql.open && !prev_sql_open;
                int tsql_rose = tsql_enabled && tsql.open && !prev_tsql_open;
                if (sql_rose && tsql_rose)
                    skip = sql.open_idx > tsql.open_idx ? sql.open_idx
                                                        : tsql.open_idx;
                else if (sql_rose)
                    skip = sql.open_idx;
                else if (tsql_rose)
                    skip = tsql.open_idx;
            }
            handle_gate_transition(gate, skip, &opts, &wav, temp_path,
                                   final_path, &recording_samples,
                                   &write_skip);
        }
        gate_open = gate;

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
            discard_recording(&wav, temp_path);
            fprintf(stderr, "Exiting -> discarded short recording (%.2fs < %.2fs)\n",
                    duration, opts.min_duration);
        }
        else
            stop_recording(&wav, temp_path, final_path);
    }
    net_close(net);
    fprintf(stderr, "Exiting...\n");
    return 0;
}
