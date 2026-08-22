#ifndef OPTIONS_H
#define OPTIONS_H

#include "types.h"

typedef struct
{
    int debug;
    char addr[STATIC_STRING_SIZE];
    int port;
    sample_format_t format;
    sample_endianness_t endianness;
    int optind;
} options_t;

int options_parse(int argc, char **argv, options_t *opts);
void options_usage(void);

#endif /* OPTIONS_H */
