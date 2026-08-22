#define _XOPEN_SOURCE 700

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "format.h"
#include "options.h"
#include "types.h"

#define RECV_BUF_SIZE 4096
#define MAX_BPS 4

volatile int do_exit = 0;

static void sighandler(int signum)
{
    (void)signum;
    fprintf(stderr, "Signal caught, exiting!\n");
    do_exit = 1;
}

static int tcp_connect(const char *addr, int port)
{
    struct addrinfo hints, *res, *rp;
    char port_str[16];
    int sockfd;

    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(addr, port_str, &hints, &res);
    if (gai != 0)
    {
        fprintf(stderr, "TCP: getaddrinfo() failed: %s\n", gai_strerror(gai));
        return -1;
    }

    for (rp = res; rp; rp = rp->ai_next)
    {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sockfd);
    }

    freeaddrinfo(res);

    if (!rp)
    {
        fprintf(stderr, "TCP: connect() to %s:%d failed: %s\n", addr, port, strerror(errno));
        return -1;
    }

    return sockfd;
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

    int bps = sample_bytes_per_sample(opts.format);

    if (opts.debug)
    {
        fprintf(stderr, "Connecting to %s:%d (format=%s, %s-endian)...\n",
                opts.addr, opts.port, sample_format_name(opts.format),
                opts.endianness == ENDIAN_BE ? "big" : "little");
    }

    int sockfd = tcp_connect(opts.addr, opts.port);
    if (sockfd < 0)
        return EXIT_FAILURE;

    fprintf(stderr, "TCP: connected to %s:%d\n", opts.addr, opts.port);

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    /* residual buffer holds bytes left over from incomplete samples */
    uint8_t residual[MAX_BPS];
    int residual_len = 0;
    uint8_t rbuf[RECV_BUF_SIZE];

    while (!do_exit)
    {
        ssize_t n = recv(sockfd, rbuf, RECV_BUF_SIZE, 0);
        if (n <= 0)
        {
            if (n == 0)
                fprintf(stderr, "TCP: connection closed by server.\n");
            else if (!do_exit)
                fprintf(stderr, "TCP: recv() failed: %s\n", strerror(errno));
            break;
        }

        fprintf(stderr, "Received %zd bytes", n);

        /* merge residual + new data into one contiguous stream */
        int total = residual_len + (int)n;
        int full_samples = total / bps;
        int leftover = total % bps;

        for (int i = 0; i < full_samples; i++)
        {
            uint8_t sample_buf[MAX_BPS];
            int abs_off = i * bps;

            if (abs_off < residual_len)
            {
                /* entirely in residual */
                memcpy(sample_buf, residual + abs_off, bps);
            }
            else
            {
                int rb_idx = abs_off - residual_len;
                if (rb_idx + bps <= (int)n)
                {
                    /* entirely in rbuf */
                    memcpy(sample_buf, rbuf + rb_idx, bps);
                }
                else
                {
                    /* spans residual/rbuf boundary */
                    int from_res = residual_len - abs_off;
                    if (from_res < 0)
                        from_res = 0;
                    if (from_res > 0)
                        memcpy(sample_buf, residual + abs_off, from_res);
                    memcpy(sample_buf + from_res, rbuf, bps - from_res);
                }
            }

            float sample = convert_sample(sample_buf, opts.format, opts.endianness);
            (void)sample;
        }

        fprintf(stderr, " (%d samples)\n", full_samples);

        /* carry leftover bytes into next recv */
        if (leftover > 0)
            memcpy(residual, rbuf + (int)n - leftover, leftover);
        residual_len = leftover;
    }

    close(sockfd);
    fprintf(stderr, "Exiting...\n");
    return 0;
}
