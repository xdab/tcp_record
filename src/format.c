#include <string.h>

#include "format.h"

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SYSTEM_BIG_ENDIAN 1
#else
#define SYSTEM_BIG_ENDIAN 0
#endif

int sample_bytes_per_sample(sample_format_t fmt)
{
    switch (fmt)
    {
    case SAMPLE_S8:
    case SAMPLE_U8:
        return 1;
    case SAMPLE_S16:
    case SAMPLE_U16:
        return 2;
    case SAMPLE_F32:
        return 4;
    }
    return 1;
}

const char *sample_format_name(sample_format_t fmt)
{
    static const char *names[] = {"s8", "u8", "s16", "u16", "f32"};
    if (fmt >= SAMPLE_S8 && fmt <= SAMPLE_F32)
        return names[fmt];
    return "unknown";
}

static inline int needs_swap(sample_endianness_t data_end)
{
    int data_big = (data_end == ENDIAN_BE);
    return data_big != SYSTEM_BIG_ENDIAN;
}

float convert_sample(const uint8_t *b, sample_format_t fmt, sample_endianness_t data_end)
{
    int swap = needs_swap(data_end);

    switch (fmt)
    {
    case SAMPLE_S8:
        return (float)*(int8_t *)b / 128.0f;
    case SAMPLE_U8:
        return ((float)*b - 128.0f) / 128.0f;
    case SAMPLE_S16:
    {
        uint16_t raw;
        if (swap)
            raw = (uint16_t)b[0] << 8 | b[1];
        else
            raw = (uint16_t)b[1] << 8 | b[0];
        return (float)*(int16_t *)&raw / 32768.0f;
    }
    case SAMPLE_U16:
    {
        uint16_t raw;
        if (swap)
            raw = (uint16_t)b[0] << 8 | b[1];
        else
            raw = (uint16_t)b[1] << 8 | b[0];
        return ((float)raw - 32768.0f) / 32768.0f;
    }
    case SAMPLE_F32:
    {
        float v;
        uint8_t *vp = (uint8_t *)&v;
        if (swap)
        {
            vp[0] = b[3];
            vp[1] = b[2];
            vp[2] = b[1];
            vp[3] = b[0];
        }
        else
        {
            memcpy(&v, b, 4);
        }
        return v;
    }
    }
    return 0.0f;
}

void float_to_s16le(const float *in, int16_t *out, int len)
{
    for (int i = 0; i < len; i++)
    {
        float v = in[i] * 32767.0f;
        if (v > 32767.0f)
            v = 32767.0f;
        else if (v < -32768.0f)
            v = -32768.0f;
        out[i] = (int16_t)v;
    }
}
