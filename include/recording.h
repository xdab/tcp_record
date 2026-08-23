#ifndef RECORDING_H
#define RECORDING_H

#include "options.h"
#include "wav.h"

wav_t *start_recording(const options_t *opts, char *temp_path, char *final_path);
void stop_recording(wav_t **wav, const char *temp_path, const char *final_path);
void discard_recording(wav_t **wav, const char *path);

#endif /* RECORDING_H */
