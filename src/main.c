#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "options.h"
#include "types.h"

volatile int do_exit = 0;

static void sighandler(int signum)
{
    (void)signum;
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
}

int main(int argc, char **argv)
{
    struct sigaction sigact;
    options_t opts;

    if (options_parse(argc, argv, &opts) != 0)
        return EXIT_FAILURE;

    if (opts.debug)
        fprintf(stderr, "hello world debug\n");
    else
        fprintf(stderr, "hello world\n");

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    while (!do_exit)
        usleep(100000);

    fprintf(stderr, "Exiting...\n");
    return 0;
}
