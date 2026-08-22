#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "format.h"
#include "options.h"

static const struct option long_options[] = {
    {"addr", required_argument, NULL, 'a'},
    {"port", required_argument, NULL, 'p'},
    {"format", required_argument, NULL, 'f'},
    {"rate", required_argument, NULL, 'r'},
    {"sql", required_argument, NULL, 's'},
    {"sql-close", required_argument, NULL, 'S'},
    {"cal", no_argument, NULL, 'c'},
    {"stdout", no_argument, NULL, 'o'},
    {"record", no_argument, NULL, 'P'},
    {"recdir", required_argument, NULL, 'D'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

static int parse_format(const char *arg, sample_format_t *fmt, sample_endianness_t *end)
{
    if (strncmp(arg, "s8", 2) == 0)
    {
        *fmt = SAMPLE_S8;
        return 0;
    }
    if (strncmp(arg, "u8", 2) == 0)
    {
        *fmt = SAMPLE_U8;
        return 0;
    }
    if (strncmp(arg, "s16", 3) == 0)
    {
        *fmt = SAMPLE_S16;
        if (arg[3] == '\0')
            return 0;
        if (strcmp(arg + 3, "le") == 0)
        {
            *end = ENDIAN_LE;
            return 0;
        }
        if (strcmp(arg + 3, "be") == 0)
        {
            *end = ENDIAN_BE;
            return 0;
        }
    }
    if (strncmp(arg, "u16", 3) == 0)
    {
        *fmt = SAMPLE_U16;
        if (arg[3] == '\0')
            return 0;
        if (strcmp(arg + 3, "le") == 0)
        {
            *end = ENDIAN_LE;
            return 0;
        }
        if (strcmp(arg + 3, "be") == 0)
        {
            *end = ENDIAN_BE;
            return 0;
        }
    }
    if (strncmp(arg, "f32", 3) == 0)
    {
        *fmt = SAMPLE_F32;
        if (arg[3] == '\0')
            return 0;
        if (strcmp(arg + 3, "le") == 0)
        {
            *end = ENDIAN_LE;
            return 0;
        }
        if (strcmp(arg + 3, "be") == 0)
        {
            *end = ENDIAN_BE;
            return 0;
        }
    }

    fprintf(stderr, "Unknown format: %s (use s8, u8, s16, u16, f32 with optional le/be suffix)\n", arg);
    return -1;
}

void options_usage(void)
{
    fprintf(stderr,
            "tcp_record - TCP audio stream recorder\n"
            "\n"
            "Usage: tcp_record -a <addr> -p <port> -f <format> [-options]\n"
            "\t-a, --addr <addr>    server address (IP or hostname)\n"
            "\t-p, --port <port>    server port\n"
            "\t-f, --format <fmt>   sample format: s8, u8, s16, u16, f32\n"
            "\t                     multi-byte formats accept le/be suffix (e.g. s16le)\n"
            "\t                     default endianness: big-endian (network byte order)\n"
            "\t-r, --rate <hz>      sample rate (default: 48000)\n"
            "\t-s, --sql <level>    squelch open threshold (0=off, default: 0)\n"
            "\t-S, --sql-close <l>  squelch close threshold (optional, default: same as -s)\n"
            "\t-c, --cal            calibrate: print envelope histogram\n"
            "\t-P, --record         auto-record: write WAV files on squelch open\n"
            "\t-D, --recdir <dir>   directory for recorded WAVs (default: cwd)\n"
            "\t-o, --stdout         output s16le samples to stdout\n"
            "\t-d                   enable debug output\n"
            "\t-h, --help           display this help\n"
            "\n");
    exit(1);
}

int options_parse(int argc, char **argv, options_t *opts)
{
    int opt;

    memset(opts, 0, sizeof(options_t));
    opts->endianness = ENDIAN_BE;
    opts->format = SAMPLE_S16;
    opts->squelch_mode = SQUELCH_FM;
    opts->sample_rate = 48000;

    while ((opt = getopt_long(argc, argv, "a:p:f:r:s:S:coPD:dh", long_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'a':
            strncpy(opts->addr, optarg, sizeof(opts->addr) - 1);
            opts->addr[sizeof(opts->addr) - 1] = '\0';
            break;
        case 'p':
            opts->port = atoi(optarg);
            break;
        case 'f':
            if (parse_format(optarg, &opts->format, &opts->endianness) < 0)
                return -1;
            break;
        case 'r':
            opts->sample_rate = atoi(optarg);
            break;
        case 's':
            opts->squelch_level = (int)(atof(optarg) * 1000);
            break;
        case 'S':
            opts->squelch_close_level = (int)(atof(optarg) * 1000);
            break;
        case 'c':
            opts->calibrate = 1;
            break;
        case 'o':
            opts->stdout_output = 1;
            break;
        case 'P':
            opts->auto_record = 1;
            break;
        case 'D':
            strncpy(opts->rec_dir, optarg, sizeof(opts->rec_dir) - 1);
            opts->rec_dir[sizeof(opts->rec_dir) - 1] = '\0';
            break;
        case 'd':
            opts->debug = 1;
            break;
        case 'h':
        default:
            options_usage();
            break;
        }
    }

    opts->optind = optind;
    return 0;
}
