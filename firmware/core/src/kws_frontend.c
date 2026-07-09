/* Wake-word front-end implementation. See core/kws_frontend.h. */
#include "core/kws_frontend.h"

#include <math.h>

bool kws_frontend_init(kws_frontend_t *fe, const kws_model_t *model, int sample_rate)
{
    if (!fe || !model) {
        return false;
    }
    if (model->in_h > KWS_FE_MAX_FRAMES || model->in_w > MELSPEC_MAX_MELS) {
        return false;
    }
    fe->model = model;
    fe->n_frames = model->in_h;
    fe->n_mels = model->in_w;
    fe->need_samples = (size_t)(model->in_h - 1) * KWS_FE_FRAME_STEP + KWS_FE_FRAME_LEN;

    melspec_cfg_t cfg = {
        .sample_rate = sample_rate,
        .frame_len = KWS_FE_FRAME_LEN,
        .fft_size = 512,
        .n_mels = model->in_w,
        .fmin_hz = 20.0f,
        .fmax_hz = (float)sample_rate / 2.0f,
        .preemph = 0.97f,
    };
    return melspec_init(&fe->mel, &cfg);
}

size_t kws_frontend_need_samples(const kws_frontend_t *fe)
{
    return fe->need_samples;
}

bool kws_frontend_run(kws_frontend_t *fe, const int16_t *pcm, size_t n,
                      kws_result_t *out)
{
    if (n < fe->need_samples) {
        return false;
    }
    const kws_model_t *m = fe->model;

    float frame[KWS_FE_FRAME_LEN];
    float mel[MELSPEC_MAX_MELS];
    int8_t feat[KWS_FE_MAX_FRAMES * MELSPEC_MAX_MELS];

    const float inv_scale = (m->in_scale != 0.0f) ? (1.0f / m->in_scale) : 0.0f;

    for (int t = 0; t < fe->n_frames; t++) {
        size_t start = (size_t)t * KWS_FE_FRAME_STEP;
        for (int i = 0; i < KWS_FE_FRAME_LEN; i++) {
            frame[i] = (float)pcm[start + i] / 32768.0f;
        }
        melspec_frame(&fe->mel, frame, mel);

        /* Quantise this frame's mel vector into the feature map row. */
        for (int f = 0; f < fe->n_mels; f++) {
            float q = roundf(mel[f] * inv_scale) + (float)m->in_zp;
            if (q > 127.0f) {
                q = 127.0f;
            } else if (q < -128.0f) {
                q = -128.0f;
            }
            feat[t * fe->n_mels + f] = (int8_t)q;
        }
    }

    return kws_model_run(m, feat, out);
}
