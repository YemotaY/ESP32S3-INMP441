/* whisper.cpp speech-to-text backend behind the generic stt_backend_t.
 *
 * Real recognition for the deployment: PCM is segmented into utterances with the same
 * energy VAD the stub uses, and each completed utterance is transcribed with whisper.cpp
 * (whisper_full over a float32 window). Finalised transcripts are drained via poll(),
 * exactly like the stub, so session.c is agnostic to which backend is linked.
 *
 * Optional dependency: this file only compiles into serverlib when the build is
 * configured with -DSIMONSAYS_WITH_WHISPER=ON and whisper.cpp headers/libs are found.
 * Without it the server falls back to the deterministic scripted stub.
 */
#ifndef SERVER_STT_WHISPER_H
#define SERVER_STT_WHISPER_H

#include <stddef.h>
#include <stdint.h>

#include "server/stt.h"

#define STT_WHISPER_TRANSCRIPT_MAX 256
#define STT_WHISPER_MAX_PENDING    4

typedef struct {
    void *wctx;                 /* struct whisper_context * (opaque here) */

    int   sample_rate;          /* PCM sample rate, e.g. 16000 */
    int   n_threads;            /* whisper inference threads */
    const char *language;       /* e.g. "en"; NULL = auto */

    /* Energy-VAD utterance segmentation (mirrors the stub). */
    uint32_t energy_thresh;
    size_t   hang_samples;      /* trailing silence that ends an utterance */
    size_t   min_speech_samples;/* minimum speech to count as an utterance */

    int      in_speech;
    size_t   speech_run;
    size_t   silence_run;

    /* Growable float32 accumulator for the current utterance. */
    float   *acc;
    size_t   acc_len;
    size_t   acc_cap;
    size_t   max_samples;       /* hard cap on utterance length */

    /* Pending finalised transcripts (ring). */
    char     pending[STT_WHISPER_MAX_PENDING][STT_WHISPER_TRANSCRIPT_MAX];
    size_t   pend_head, pend_tail;
} stt_whisper_t;

/* Load a ggml/gguf whisper model from `model_path`.
 * Returns 0 on success, non-zero if the model failed to load. */
int stt_whisper_init(stt_whisper_t *s, const char *model_path,
                     int sample_rate, uint32_t energy_thresh,
                     size_t hang_samples, size_t min_speech_samples);

/* Wrap the loaded backend in the generic interface. */
stt_backend_t stt_whisper_backend(stt_whisper_t *s);

/* Release the whisper context and the sample accumulator. */
void stt_whisper_free(stt_whisper_t *s);

#endif /* SERVER_STT_WHISPER_H */
