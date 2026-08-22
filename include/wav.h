#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <stdio.h>

typedef struct
{
    FILE *fp;
    uint32_t data_size;
} wav_t;

wav_t *wav_open(const char *path, int sample_rate, int channels, int bits);
int wav_write(wav_t *w, const void *buf, size_t samples);
void wav_close(wav_t *w);

#endif /* WAV_H */
