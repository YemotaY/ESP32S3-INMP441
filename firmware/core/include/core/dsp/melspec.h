/* Log-Mel spectrogram front-end (float), shared verbatim by firmware and host.
 *
 * Per-frame pipeline: pre-emphasis -> Hann window -> zero-pad to fft_size -> FFT ->
 * power spectrum -> triangular mel filterbank (HTK formula) -> log.
 *
 * Buffers are fixed-size (no malloc) so the same code runs on the ESP32-S3. Bump the
 * MELSPEC_MAX_* limits if a larger configuration is needed.
 */
#ifndef CORE_DSP_MELSPEC_H
#define CORE_DSP_MELSPEC_H

#include <stdbool.h>

#define MELSPEC_MAX_FFT   512
#define MELSPEC_MAX_MELS   40
#define MELSPEC_MAX_FRAME 512
#define MELSPEC_MAX_BINS  (MELSPEC_MAX_FFT / 2 + 1)

typedef struct {
    int   sample_rate;
    int   frame_len;    /* samples per analysis frame */
    int   fft_size;     /* >= frame_len, power of two */
    int   n_mels;
    float fmin_hz;
    float fmax_hz;
    float preemph;      /* pre-emphasis coefficient, e.g. 0.97; 0 to disable */
} melspec_cfg_t;

typedef struct {
    melspec_cfg_t cfg;
    int   n_bins;                               /* fft_size/2 + 1 */
    float window[MELSPEC_MAX_FRAME];            /* Hann window */
    float mel_fb[MELSPEC_MAX_MELS][MELSPEC_MAX_BINS]; /* triangular weights */
} melspec_t;

/* Validate cfg and precompute window + filterbank. Returns false on bad cfg. */
bool melspec_init(melspec_t *m, const melspec_cfg_t *cfg);

/* Compute log-mel for one frame of `frame_len` float samples (range ~[-1,1]).
 * Writes `n_mels` values to out. */
void melspec_frame(const melspec_t *m, const float *frame, float *out);

/* Convenience: Hz<->Mel (HTK). Exposed for tests. */
float melspec_hz_to_mel(float hz);
float melspec_mel_to_hz(float mel);

#endif /* CORE_DSP_MELSPEC_H */
