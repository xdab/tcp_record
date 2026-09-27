#define _XOPEN_SOURCE 700

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "recording.h"

#define TEMP_HEX_LEN 16

static void random_hex(char *buf, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        for (size_t i = 0; i < len; i++)
            buf[i] = '0';
        return;
    }
    uint8_t bytes[len / 2];
    ssize_t rd = read(fd, bytes, len / 2);
    close(fd);
    if (rd < (ssize_t)(len / 2))
    {
        for (size_t i = 0; i < len; i++)
            buf[i] = '0';
        return;
    }
    for (size_t i = 0; i < len / 2; i++)
        snprintf(buf + i * 2, 3, "%02x", bytes[i]);
}

wav_t *start_recording(const options_t *opts, char *temp_path, char *final_path)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char dir[512];

    if (opts->rec_dir[0] != '\0')
        snprintf(dir, sizeof(dir), "%s/", opts->rec_dir);
    else
        dir[0] = '\0';

    /* build final path */
    int end = strlen(dir);
    snprintf(final_path, 512, "%s%s_%d_", dir, opts->label, opts->port);
    end = strlen(final_path);
    strftime(final_path + end, 512 - end, "%Y%m%d_%H%M%S", tm);
    end = strlen(final_path);
    snprintf(final_path + end, 512 - end, ".wav");

    /* build temp path */
    char hex[TEMP_HEX_LEN + 1];
    random_hex(hex, TEMP_HEX_LEN);
    hex[TEMP_HEX_LEN] = '\0';
    snprintf(temp_path, 512, "%sTEMP_%s.wav", dir, hex);

    wav_t *wav = wav_open(temp_path, opts->sample_rate, 1, 16);
    if (wav)
        fprintf(stderr, "Squelch opened -> recording to %s\n", temp_path);
    else
        fprintf(stderr, "Squelch opened -> failed to open %s\n", temp_path);
    return wav;
}

/* avoid silently clobbering an existing recording: append _N before .wav */
static void unique_target(char *target, size_t sz, const char *final_path)
{
    size_t stem = strlen(final_path) - 4; /* strip ".wav" */

    snprintf(target, sz, "%s", final_path);
    for (int i = 1; access(target, F_OK) == 0 && i < 1000; i++)
        snprintf(target, sz, "%.*s_%d.wav", (int)stem, final_path, i);
}

void stop_recording(wav_t **wav, const char *temp_path, const char *final_path)
{
    char target[512];

    if (!*wav)
        return;
    wav_close(*wav);
    *wav = NULL;
    unique_target(target, sizeof(target), final_path);
    if (rename(temp_path, target) == 0)
        fprintf(stderr, "Squelch closed -> saved to %s\n", target);
    else
        fprintf(stderr, "Squelch closed -> failed to rename %s to %s\n",
                temp_path, target);
}

void discard_recording(wav_t **wav, const char *path)
{
    if (!*wav)
        return;
    wav_close(*wav);
    *wav = NULL;
    if (path[0] != '\0')
        unlink(path);
}
