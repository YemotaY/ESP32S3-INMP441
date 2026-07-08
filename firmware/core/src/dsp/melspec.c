#include "core/dsp/melspec.h"
#include "core/dsp/fft.h"

#include <math.h>
#include <string.h>

float melspec_hz_to_mel(float hz)
{
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

float melspec_mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

bool melspec_init(melspec_t *m, const melspec_cfg_t *cfg)
{
    if (cfg->frame_len <= 0 || cfg->frame_len > MELSPEC_MAX_FRAME) return false;
    if (cfg->fft_size < cfg->frame_len || cfg->fft_size > MELSPEC_MAX_FFT) return false;
    if (!fft_is_pow2(cfg->fft_size)) return false;
    if (cfg->n_mels <= 0 || cfg->n_mels > MELSPEC_MAX_MELS) return false;
    if (cfg->sample_rate <= 0) return false;
    if (cfg->fmax_hz <= cfg->fmin_hz) return false;

    m->cfg = *cfg;
    m->n_bins = cfg->fft_size / 2 + 1;

    /* Hann window. */
    for (int i = 0; i < cfg->frame_len; i++) {
        m->window[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i /
                                          (float)(cfg->frame_len - 1));
    }

    /* Triangular mel filterbank (HTK). n_mels+2 edge points in mel space. */
    memset(m->mel_fb, 0, sizeof(m->mel_fb));
    float mel_min = melspec_hz_to_mel(cfg->fmin_hz);
    float mel_max = melspec_hz_to_mel(cfg->fmax_hz);
    int npts = cfg->n_mels + 2;

    float bin_hz[MELSPEC_MAX_MELS + 2];
    for (int p = 0; p < npts; p++) {
        float mel = mel_min + (mel_max - mel_min) * (float)p / (float)(npts - 1);
        bin_hz[p] = melspec_mel_to_hz(mel);
    }

    for (int mch = 0; mch < cfg->n_mels; mch++) {
        float left = bin_hz[mch];
        float center = bin_hz[mch + 1];
        float right = bin_hz[mch + 2];
        for (int k = 0; k < m->n_bins; k++) {
            float f = (float)k * (float)cfg->sample_rate / (float)cfg->fft_size;
            float w = 0.0f;
            if (f >= left && f <= center && center > left) {
                w = (f - left) / (center - left);
            } else if (f > center && f <= right && right > center) {
                w = (right - f) / (right - center);
            }
            m->mel_fb[mch][k] = w;
        }
    }
    return true;
}

void melspec_frame(const melspec_t *m, const float *frame, float *out)
{
    const melspec_cfg_t *cfg = &m->cfg;
    float re[MELSPEC_MAX_FFT];
    float im[MELSPEC_MAX_FFT];

    /* Pre-emphasis + window into re[], zero-pad the rest. */
    float prev = 0.0f;
    for (int i = 0; i < cfg->frame_len; i++) {
        float s = frame[i] - cfg->preemph * prev;
        prev = frame[i];
        re[i] = s * m->window[i];
        im[i] = 0.0f;
    }
    for (int i = cfg->frame_len; i < cfg->fft_size; i++) {
        re[i] = 0.0f;
        im[i] = 0.0f;
    }

    fft_radix2(re, im, cfg->fft_size, false);

    /* Power spectrum for bins 0..n_bins-1. */
    float power[MELSPEC_MAX_BINS];
    for (int k = 0; k < m->n_bins; k++) {
        power[k] = re[k] * re[k] + im[k] * im[k];
    }

    /* Mel projection + log. */
    const float eps = 1e-10f;
    for (int mch = 0; mch < cfg->n_mels; mch++) {
        float e = 0.0f;
        for (int k = 0; k < m->n_bins; k++) {
            e += m->mel_fb[mch][k] * power[k];
        }
        out[mch] = logf(e + eps);
    }
}
