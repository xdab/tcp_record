#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
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
    uint8_t buf[RECV_BUF_SIZE + MAX_BPS];
    int len;    /* valid bytes in buf */
    int pos;    /* start of next unconsumed sample */
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
    s->len = 0;
    s->pos = 0;

    fprintf(stderr, "TCP: connected to %s:%d\n", addr, port);
    return s;
}

net_state_t *net_open_file(const char *path, sample_format_t fmt,
                           sample_endianness_t end)
{
    net_state_t *s = calloc(1, sizeof(net_state_t));
    if (!s)
        return NULL;

    s->sockfd = open(path, O_RDONLY);
    if (s->sockfd < 0)
    {
        fprintf(stderr, "File: open(%s) failed: %s\n", path, strerror(errno));
        free(s);
        return NULL;
    }

    s->format = fmt;
    s->endianness = end;
    s->bps = sample_bytes_per_sample(fmt);
    s->len = 0;
    s->pos = 0;

    fprintf(stderr, "File: reading %s\n", path);
    return s;
}

int net_recv_samples(net_state_t *s, float *out, int max_samples)
{
    int written = 0;

    while (written < max_samples)
    {
        /* consume complete samples */
        if (s->len - s->pos >= s->bps)
        {
            out[written++] = convert_sample(s->buf + s->pos, s->format, s->endianness);
            s->pos += s->bps;
            continue;
        }

        /* compact: drop consumed bytes, keep the partial sample tail */
        if (s->pos > 0)
        {
            memmove(s->buf, s->buf + s->pos, s->len - s->pos);
            s->len -= s->pos;
            s->pos = 0;
        }

        ssize_t n = read(s->sockfd, s->buf + s->len, sizeof(s->buf) - s->len);
        if (n <= 0)
        {
            if (n == 0)
                return written > 0 ? written : 0;
            if (errno == EINTR)
                continue;
            return -1;
        }
        s->len += (int)n;
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
