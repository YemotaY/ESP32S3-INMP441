#include "wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_u32(FILE *f, uint32_t *v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int read_u16(FILE *f, uint16_t *v)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return -1;
    *v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return 0;
}

int wav_read(const char *path, wav_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char riff[4], wave[4];
    uint32_t chunk_size;
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return -2; }
    if (read_u32(f, &chunk_size) != 0) { fclose(f); return -2; }
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return -2; }

    uint16_t audio_fmt = 0, channels = 0, bits = 0;
    uint32_t sample_rate = 0;
    int have_fmt = 0;

    for (;;) {
        char id[4];
        uint32_t sz;
        if (fread(id, 1, 4, f) != 4) break;
        if (read_u32(f, &sz) != 0) break;

        if (memcmp(id, "fmt ", 4) == 0) {
            uint16_t block_align; uint32_t byte_rate;
            if (read_u16(f, &audio_fmt) != 0) { fclose(f); return -3; }
            if (read_u16(f, &channels) != 0) { fclose(f); return -3; }
            if (read_u32(f, &sample_rate) != 0) { fclose(f); return -3; }
            if (read_u32(f, &byte_rate) != 0) { fclose(f); return -3; }
            if (read_u16(f, &block_align) != 0) { fclose(f); return -3; }
            if (read_u16(f, &bits) != 0) { fclose(f); return -3; }
            (void)byte_rate; (void)block_align;
            if (sz > 16) fseek(f, (long)(sz - 16), SEEK_CUR);
            have_fmt = 1;
        } else if (memcmp(id, "data", 4) == 0) {
            if (!have_fmt || audio_fmt != 1 || bits != 16 || channels == 0) {
                fclose(f); return -4;
            }
            uint32_t n_samples = sz / 2; /* int16 count, interleaved */
            int16_t *buf = (int16_t *)malloc(sz ? sz : 2);
            if (!buf) { fclose(f); return -5; }
            if (fread(buf, 1, sz, f) != sz) { free(buf); fclose(f); return -6; }

            out->sample_rate = (int)sample_rate;
            out->channels = (int)channels;
            out->num_frames = n_samples / channels;
            out->samples = buf;
            fclose(f);
            return 0;
        } else {
            /* Skip unknown chunk (word-aligned). */
            fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
        }
    }
    fclose(f);
    return -7;
}

static int write_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

static int write_u16(FILE *f, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    return fwrite(b, 1, 2, f) == 2 ? 0 : -1;
}

int wav_write_mono(const char *path, const int16_t *samples, uint32_t n, int sample_rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    uint32_t data_bytes = n * 2;
    uint32_t byte_rate = (uint32_t)sample_rate * 2;

    fwrite("RIFF", 1, 4, f);
    write_u32(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    write_u32(f, 16);
    write_u16(f, 1);              /* PCM */
    write_u16(f, 1);              /* mono */
    write_u32(f, (uint32_t)sample_rate);
    write_u32(f, byte_rate);
    write_u16(f, 2);             /* block align */
    write_u16(f, 16);            /* bits */
    fwrite("data", 1, 4, f);
    write_u32(f, data_bytes);
    fwrite(samples, 2, n, f);

    fclose(f);
    return 0;
}

void wav_free(wav_t *w)
{
    if (w && w->samples) {
        free(w->samples);
        w->samples = NULL;
    }
}
