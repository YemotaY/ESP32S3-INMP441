/* Speech-to-text backend interface + a deterministic stub.
 *
 * STT is pluggable: the real deployment links a whisper.cpp backend behind this same
 * interface (server/src/stt_whisper.c, not built by default -- needs the model). The stub
 * used for dev and tests segments utterances with energy VAD and emits a scripted
 * transcript per detected utterance, making the whole ingest->STT->intent->debounce loop
 * hermetic and reproducible (tests/host/test_session.c, the e2e loopback test).
 */
#ifndef SERVER_STT_H
#define SERVER_STT_H

#include <stddef.h>
#include <stdint.h>

/* Generic STT backend. Feed PCM incrementally; poll for finalised transcripts. */
typedef struct {
    void (*reset)(void *ctx);
    void (*feed)(void *ctx, const int16_t *pcm, size_t n);
    /* Returns 1 and writes a NUL-terminated transcript to `out` if one is ready. */
    int  (*poll)(void *ctx, char *out, size_t cap);
    void *ctx;
} stt_backend_t;

/* --- Deterministic stub backend --- */

#define STT_STUB_MAX_PENDING 4
#define STT_STUB_TRANSCRIPT_MAX 128

typedef struct {
    /* VAD-based utterance segmentation. */
    uint32_t energy_thresh;
    size_t   hang_samples;      /* trailing silence that ends an utterance */
    size_t   min_speech_samples;/* minimum speech to count as an utterance */

    /* Scripted transcripts, emitted in order as utterances complete. */
    const char *const *scripts;
    size_t   script_count;
    size_t   next_script;

    /* Internal VAD run state. */
    int      in_speech;
    size_t   speech_run;
    size_t   silence_run;

    /* Pending finalised transcripts (ring). */
    char     pending[STT_STUB_MAX_PENDING][STT_STUB_TRANSCRIPT_MAX];
    size_t   pend_head, pend_tail;
} stt_stub_t;

/* Initialise a stub over `scripts` (array of `count` NUL-terminated strings). */
void stt_stub_init(stt_stub_t *s, const char *const *scripts, size_t count,
                   uint32_t energy_thresh, size_t hang_samples, size_t min_speech_samples);

/* Wrap a stub in the generic backend interface. */
stt_backend_t stt_stub_backend(stt_stub_t *s);

#endif /* SERVER_STT_H */
