#ifndef TYPES_H
#define TYPES_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>

#define STATIC_STRING_SIZE 256

extern volatile int do_exit;

#endif /* TYPES_H */
