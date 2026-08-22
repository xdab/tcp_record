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

typedef enum
{
    SAMPLE_S8,
    SAMPLE_U8,
    SAMPLE_S16,
    SAMPLE_U16,
    SAMPLE_F32
} sample_format_t;

typedef enum
{
    ENDIAN_BE,
    ENDIAN_LE
} sample_endianness_t;

#endif /* TYPES_H */
