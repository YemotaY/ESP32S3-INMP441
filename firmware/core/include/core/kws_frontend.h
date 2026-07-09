/* Wake-word front-end: raw PCM -> log-mel feature map -> int8 -> DS-CNN decision.
 *
 * Ties the shared log-mel front-end (dsp/melspec) to the trained int8 DS-CNN
 * (kws_model) so the same pipeline runs on the device and in host tests. The model's
 * in_h/in_w set the feature-map geometry: in_h stacked frames x in_w mel bins.
 *
 * The feature framing (25 ms window / 10 ms hop, pre-emphasis 0.97) mirrors the Python
 * training features (kws-framework/kwslib/features.py) so trained weights transfer.
 */
#ifndef CORE_KWS_FRONTEND_H
#define CORE_KWS_FRONTEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/dsp/melspec.h"
#include "core/kws_model.h"

/* Framing constants (16 kHz): 400-sample window, 160-sample hop. */
#define KWS_FE_FRAME_LEN  400
#define KWS_FE_FRAME_STEP 160
#define KWS_FE_MAX_FRAMES 32   /* upper bound on model in_h */

typedef struct {
    const kws_model_t *model;
    melspec_t          mel;
    int                n_frames;    /* == model->in_h */
    int                n_mels;      /* == model->in_w */
    /* Samples needed to produce n_frames frames. */
    size_t             need_samples;
} kws_frontend_t;

/* Configure the front-end for `model` at `sample_rate` Hz.
 * Returns false if the model geometry exceeds the melspec/frontend limits. */
bool kws_frontend_init(kws_frontend_t *fe, const kws_model_t *model, int sample_rate);

/* How many PCM samples kws_frontend_run() consumes. */
size_t kws_frontend_need_samples(const kws_frontend_t *fe);

/* Run the full pipeline over `pcm` (>= need_samples int16 samples).
 * Writes the wake decision to `out`. Returns false on size/inference error. */
bool kws_frontend_run(kws_frontend_t *fe, const int16_t *pcm, size_t n,
                      kws_result_t *out);

#endif /* CORE_KWS_FRONTEND_H */
