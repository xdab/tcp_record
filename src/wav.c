#include <stdlib.h>
#include <string.h>

#include "wav.h"

static void write_le32(FILE *fp, uint32_t v)
{
    fputc(v & 0xFF, fp);
    fputc((v >> 8) & 0xFF, fp);
    fputc((v >> 16) & 0xFF, fp);
    fputc((v >> 24) & 0xFF, fp);
}

static void write_le16(FILE *fp, uint16_t v)
{
    fputc(v & 0xFF, fp);
    fputc((v >> 8) & 0xFF, fp);
}

wav_t *wav_open(const char *path, int sample_rate, int channels, int bits)
{
    wav_t *w = calloc(1, sizeof(wav_t));
    if (!w)
        return NULL;

    w->fp = fopen(path, "wb");
    if (!w->fp)
    {
        free(w);
        return NULL;
    }

    int bytes_per_sample = bits / 8;
    int block_align = channels * bytes_per_sample;
    uint32_t byte_rate = sample_rate * block_align;

    /* RIFF header */
    fwrite("RIFF", 1, 4, w->fp);
    write_le32(w->fp, 0); /* file size - 8, patched at close */
    fwrite("WAVE", 1, 4, w->fp);

    /* fmt subchunk */
    fwrite("fmt ", 1, 4, w->fp);
    write_le32(w->fp, 16); /* subchunk size */
    write_le16(w->fp, 1);  /* PCM format */
    write_le16(w->fp, channels);
    write_le32(w->fp, sample_rate);
    write_le32(w->fp, byte_rate);
    write_le16(w->fp, block_align);
    write_le16(w->fp, bits);

    /* data subchunk */
    fwrite("data", 1, 4, w->fp);
    write_le32(w->fp, 0); /* data size, patched at close */

    return w;
}

int wav_write(wav_t *w, const void *buf, size_t samples)
{
    size_t written = fwrite(buf, 1, samples, w->fp);
    w->data_size += written;
    return (int)written;
}

void wav_close(wav_t *w)
{
    if (!w)
        return;

    /* patch data size */
    fseek(w->fp, 40, SEEK_SET);
    write_le32(w->fp, w->data_size);

    /* patch RIFF file size */
    fseek(w->fp, 4, SEEK_SET);
    write_le32(w->fp, w->data_size + 36);

    fclose(w->fp);
    free(w);
}
