#ifndef OPTIONS_H
#define OPTIONS_H

#include "types.h"

typedef struct
{
    int debug;
    int optind;
} options_t;

int options_parse(int argc, char **argv, options_t *opts);
void options_usage(void);

#endif /* OPTIONS_H */
