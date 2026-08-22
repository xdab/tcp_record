#define _XOPEN_SOURCE 700

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "calibration.h"
#include "format.h"
#include "network.h"
#include "options.h"
#include "squelch.h"
#include "types.h"

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
        float threshold = (float)opts.squelch_level / 1000.0f;
        squelch_init(&sql, opts.squelch_mode, opts.sample_rate, threshold);
        if (!opts.calibrate)
            fprintf(stderr, "Squelch: enabled (threshold=%.4f)\n", threshold);
        else
            fprintf(stderr, "Calibrate: running (threshold=%.4f)\n", threshold);
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    float samples[SAMPLE_BUF_SIZE];
    int16_t outbuf[SAMPLE_BUF_SIZE];
    int sql_block_count = 0;

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
                    fprintf(stderr, "Squelch %s (env=%.4f)\n",
                            sql.open ? "opened" : "closed", sql.envelope);
                if (++sql_block_count >= 12)
                {
                    fprintf(stderr, "sql: env=%.4f %s\n",
                            sql.envelope, sql.open ? "OPEN" : "CLOSED");
                    sql_block_count = 0;
                }
            }
        }

        float_to_s16le(samples, outbuf, n);
        if (write_all(STDOUT_FILENO, outbuf, n * sizeof(int16_t)) < 0)
        {
            if (!do_exit)
                fprintf(stderr, "Write error (broken pipe?)\n");
            break;
        }
    }

    net_close(net);
    fprintf(stderr, "Exiting...\n");
    return 0;
}
