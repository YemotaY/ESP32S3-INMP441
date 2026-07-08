/* Minimal RIFF/WAVE reader for 16-bit PCM (mono/stereo). Host tooling only. */
#ifndef WAV_H
#define WAV_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int      sample_rate;
    int      channels;
    uint32_t num_frames;   /* samples per channel */
    int16_t *samples;      /* interleaved int16, length num_frames*channels; caller frees */
} wav_t;

/* Load a 16-bit PCM WAV. Returns 0 on success, negative on error. */
int wav_read(const char *path, wav_t *out);

/* Write a mono 16-bit PCM WAV. Returns 0 on success. */
int wav_write_mono(const char *path, const int16_t *samples, uint32_t n, int sample_rate);

void wav_free(wav_t *w);

#endif /* WAV_H */
