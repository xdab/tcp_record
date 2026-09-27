#ifndef NETWORK_H
#define NETWORK_H

#include "types.h"

typedef struct net_state net_state_t;

net_state_t *net_connect(const char *addr, int port, sample_format_t fmt, sample_endianness_t end);
net_state_t *net_open_file(const char *path, sample_format_t fmt, sample_endianness_t end);
int net_recv_samples(net_state_t *s, float *out, int max_samples);
void net_close(net_state_t *s);

#endif /* NETWORK_H */
