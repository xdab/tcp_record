#ifndef FORMAT_H
#define FORMAT_H

#include "types.h"
#include <stdint.h>

int sample_bytes_per_sample(sample_format_t fmt);
const char *sample_format_name(sample_format_t fmt);
float convert_sample(const uint8_t *buf, sample_format_t fmt, sample_endianness_t data_end);
void float_to_s16le(const float *in, int16_t *out, int len);

#endif /* FORMAT_H */
