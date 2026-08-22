#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "format.h"
#include "network.h"

#define RECV_BUF_SIZE 4096
#define MAX_BPS 4

struct net_state
{
    int sockfd;
    sample_format_t format;
    sample_endianness_t endianness;
    int bps;
    uint8_t residual[MAX_BPS];
    int residual_len;
    uint8_t rbuf[RECV_BUF_SIZE];
};

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

net_state_t *net_connect(const char *addr, int port, sample_format_t fmt, sample_endianness_t end)
{
    net_state_t *s = calloc(1, sizeof(net_state_t));
    if (!s)
        return NULL;

    s->sockfd = tcp_connect(addr, port);
    if (s->sockfd < 0)
    {
        free(s);
        return NULL;
    }

    s->format = fmt;
    s->endianness = end;
    s->bps = sample_bytes_per_sample(fmt);
    s->residual_len = 0;

    fprintf(stderr, "TCP: connected to %s:%d\n", addr, port);
    return s;
}

int net_recv_samples(net_state_t *s, float *out, int max_samples)
{
    int written = 0;

    while (written < max_samples)
    {
        /* have a complete sample in residual? */
        if (s->residual_len >= s->bps)
        {
            out[written++] = convert_sample(s->residual, s->format, s->endianness);
            /* shift remaining residual bytes */
            int remain = s->residual_len - s->bps;
            if (remain > 0)
                memmove(s->residual, s->residual + s->bps, remain);
            s->residual_len = remain;
            continue;
        }

        /* residual is empty, block on recv */
        ssize_t n = recv(s->sockfd, s->rbuf, RECV_BUF_SIZE, 0);
        if (n <= 0)
        {
            if (n == 0)
                return written > 0 ? written : 0;
            if (errno == EINTR)
                continue;
            return -1;
        }

        int total = s->residual_len + (int)n;
        int full_samples = total / s->bps;
        int leftover = total % s->bps;

        int to_copy = full_samples;
        if (to_copy > max_samples - written)
            to_copy = max_samples - written;

        for (int i = 0; i < to_copy; i++)
        {
            uint8_t sample_buf[MAX_BPS];
            int abs_off = i * s->bps;

            if (abs_off < s->residual_len)
            {
                memcpy(sample_buf, s->residual + abs_off, s->bps);
            }
            else
            {
                int rb_idx = abs_off - s->residual_len;
                if (rb_idx + s->bps <= (int)n)
                {
                    memcpy(sample_buf, s->rbuf + rb_idx, s->bps);
                }
                else
                {
                    int from_res = s->residual_len - abs_off;
                    if (from_res < 0)
                        from_res = 0;
                    if (from_res > 0)
                        memcpy(sample_buf, s->residual + abs_off, from_res);
                    memcpy(sample_buf + from_res, s->rbuf, s->bps - from_res);
                }
            }

            out[written++] = convert_sample(sample_buf, s->format, s->endianness);
        }

        /* carry leftover bytes */
        if (leftover > 0)
            memcpy(s->residual, s->rbuf + (int)n - leftover, leftover);
        s->residual_len = leftover;

        /* if we couldn't fit all decoded samples, stop here */
        if (full_samples > to_copy)
            break;
    }

    return written;
}

void net_close(net_state_t *s)
{
    if (!s)
        return;
    if (s->sockfd >= 0)
        close(s->sockfd);
    free(s);
}
