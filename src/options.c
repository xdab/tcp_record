#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "options.h"

void options_usage(void)
{
    fprintf(stderr,
            "tcp_record - TCP audio stream recorder\n"
            "\n"
            "Usage: tcp_record [-options]\n"
            "\t[-d enable debug output]\n"
            "\t[-h display this help]\n"
            "\n");
    exit(1);
}

int options_parse(int argc, char **argv, options_t *opts)
{
    int opt;

    memset(opts, 0, sizeof(options_t));

    while ((opt = getopt(argc, argv, "dh")) != -1)
    {
        switch (opt)
        {
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
