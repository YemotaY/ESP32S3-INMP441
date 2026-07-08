/* wav_simulate: run the device log-mel front-end over a WAV file on the host.
 *
 * Prints frame/feature stats and a coarse ASCII heatmap of the log-mel features,
 * proving the exact firmware DSP pipeline end-to-end on real audio before flashing.
 *
 * Usage: wav_simulate <file.wav> [n_mels]
 */
#include "wav.h"
#include "core/dsp/melspec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.wav> [n_mels]\n", argv[0]);
        return 2;
    }
    int n_mels = (argc >= 3) ? atoi(argv[2]) : 40;
    if (n_mels < 1 || n_mels > MELSPEC_MAX_MELS) n_mels = 40;

    wav_t w;
    int rc = wav_read(argv[1], &w);
    if (rc != 0) {
        fprintf(stderr, "wav_read failed: %d\n", rc);
        return 1;
    }

    const int frame_len = 400;   /* 25 ms @ 16k */
    const int frame_step = 160;  /* 10 ms hop  */
    const int fft_size = 512;

    melspec_cfg_t cfg = {
        .sample_rate = w.sample_rate,
        .frame_len = frame_len,
        .fft_size = fft_size,
        .n_mels = n_mels,
        .fmin_hz = 20.0f,
        .fmax_hz = (float)w.sample_rate / 2.0f,
        .preemph = 0.97f,
    };
    melspec_t m;
    if (!melspec_init(&m, &cfg)) {
        fprintf(stderr, "melspec_init failed (check sample rate / config)\n");
        wav_free(&w);
        return 1;
    }

    printf("file=%s sr=%d ch=%d frames=%u\n",
           argv[1], w.sample_rate, w.channels, w.num_frames);

    /* Convert to mono float. */
    float *mono = (float *)malloc(sizeof(float) * w.num_frames);
    for (uint32_t i = 0; i < w.num_frames; i++) {
        long acc = 0;
        for (int c = 0; c < w.channels; c++) {
            acc += w.samples[i * w.channels + c];
        }
        mono[i] = (float)acc / (float)w.channels / 32768.0f;
    }

    const char *ramp = " .:-=+*#%@";
    float frame[MELSPEC_MAX_FRAME];
    float feat[MELSPEC_MAX_MELS];
    int n_frames = 0;
    float gmin = 1e30f, gmax = -1e30f;

    for (uint32_t start = 0; start + frame_len <= w.num_frames; start += frame_step) {
        for (int i = 0; i < frame_len; i++) frame[i] = mono[start + i];
        melspec_frame(&m, frame, feat);
        for (int k = 0; k < n_mels; k++) {
            if (feat[k] < gmin) gmin = feat[k];
            if (feat[k] > gmax) gmax = feat[k];
        }
        n_frames++;
    }

    printf("log-mel: n_frames=%d n_mels=%d range=[%.3f, %.3f]\n",
           n_frames, n_mels, gmin, gmax);

    /* Second pass: print a coarse heatmap (mels as rows, frames as columns). */
    float span = (gmax > gmin) ? (gmax - gmin) : 1.0f;
    int col = 0;
    for (int k = n_mels - 1; k >= 0; k--) {
        printf("m%02d |", k);
        col = 0;
        for (uint32_t start = 0;
             start + frame_len <= w.num_frames && col < 100;
             start += frame_step) {
            for (int i = 0; i < frame_len; i++) frame[i] = mono[start + i];
            melspec_frame(&m, frame, feat);
            float norm = (feat[k] - gmin) / span;
            int idx = (int)(norm * 9.0f);
            if (idx < 0) idx = 0;
            if (idx > 9) idx = 9;
            putchar(ramp[idx]);
            col++;
        }
        putchar('\n');
    }

    free(mono);
    wav_free(&w);
    return 0;
}
