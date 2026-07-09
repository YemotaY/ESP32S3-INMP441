/* whisper.cpp STT backend implementation. See server/stt_whisper.h.
 *
 * Compiled only when SIMONSAYS_WITH_WHISPER is defined (the CMake option finds the
 * whisper.cpp headers/libs). The whole translation unit is guarded so serverlib always
 * builds even on machines without whisper installed.
 */
#include "server/stt_whisper.h"

#include <stdlib.h>
#include <string.h>

#ifdef SIMONSAYS_WITH_WHISPER

#include <whisper.h>

static void push_pending(stt_whisper_t *s, const char *text)
{
    /* Trim leading whitespace whisper often prepends. */
    while (*text == ' ' || *text == '\t' || *text == '\n') {
        text++;
    }
    if (*text == '\0') {
        return;
    }
    size_t next = (s->pend_tail + 1) % STT_WHISPER_MAX_PENDING;
    if (next == s->pend_head) {
        return;   /* ring full: drop, stay bounded */
    }
    size_t n = strlen(text);
    if (n >= STT_WHISPER_TRANSCRIPT_MAX) {
        n = STT_WHISPER_TRANSCRIPT_MAX - 1;
    }
    memcpy(s->pending[s->pend_tail], text, n);
    s->pending[s->pend_tail][n] = '\0';
    s->pend_tail = next;
}

static void transcribe(stt_whisper_t *s)
{
    if (s->acc_len == 0 || s->wctx == NULL) {
        return;
    }
    struct whisper_context *ctx = s->wctx;
    struct whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.print_progress   = false;
    p.print_realtime   = false;
    p.print_timestamps = false;
    p.single_segment   = false;
    p.no_context       = true;
    p.translate        = false;
    p.n_threads        = s->n_threads > 0 ? s->n_threads : 1;
    if (s->language) {
        p.language = s->language;
    }

    if (whisper_full(ctx, p, s->acc, (int)s->acc_len) == 0) {
        int n_seg = whisper_full_n_segments(ctx);
        char joined[STT_WHISPER_TRANSCRIPT_MAX];
        size_t used = 0;
        joined[0] = '\0';
        for (int i = 0; i < n_seg; i++) {
            const char *seg = whisper_full_get_segment_text(ctx, i);
            if (!seg) {
                continue;
            }
            size_t sl = strlen(seg);
            if (used + sl >= sizeof(joined)) {
                sl = sizeof(joined) - 1 - used;
            }
            memcpy(joined + used, seg, sl);
            used += sl;
            joined[used] = '\0';
            if (used >= sizeof(joined) - 1) {
                break;
            }
        }
        push_pending(s, joined);
    }
}

static void reset_utterance(stt_whisper_t *s)
{
    s->acc_len = 0;
    s->in_speech = 0;
    s->speech_run = 0;
    s->silence_run = 0;
}

static void end_utterance(stt_whisper_t *s)
{
    if (s->speech_run >= s->min_speech_samples) {
        transcribe(s);
    }
    reset_utterance(s);
}

static void acc_push(stt_whisper_t *s, float v)
{
    if (s->acc_len >= s->max_samples) {
        return;   /* utterance too long: stop accumulating (will flush on hang) */
    }
    if (s->acc_len == s->acc_cap) {
        size_t cap = s->acc_cap ? s->acc_cap * 2 : 4096;
        if (cap > s->max_samples) {
            cap = s->max_samples;
        }
        float *na = realloc(s->acc, cap * sizeof(float));
        if (!na) {
            return;
        }
        s->acc = na;
        s->acc_cap = cap;
    }
    s->acc[s->acc_len++] = v;
}

static void whisper_reset(void *ctx)
{
    stt_whisper_t *s = ctx;
    reset_utterance(s);
    s->pend_head = s->pend_tail = 0;
}

static void whisper_feed(void *ctx, const int16_t *pcm, size_t n)
{
    stt_whisper_t *s = ctx;
    for (size_t i = 0; i < n; i++) {
        int32_t v = pcm[i];
        acc_push(s, (float)v / 32768.0f);
        uint32_t mag = (uint32_t)(v < 0 ? -v : v);
        if (mag > s->energy_thresh) {
            s->in_speech = 1;
            s->speech_run++;
            s->silence_run = 0;
        } else if (s->in_speech) {
            s->silence_run++;
            if (s->silence_run >= s->hang_samples) {
                end_utterance(s);
            }
        }
    }
}

static int whisper_poll(void *ctx, char *out, size_t cap)
{
    stt_whisper_t *s = ctx;
    if (s->pend_head == s->pend_tail) {
        return 0;
    }
    const char *src = s->pending[s->pend_head];
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, src, n);
    out[n] = '\0';
    s->pend_head = (s->pend_head + 1) % STT_WHISPER_MAX_PENDING;
    return 1;
}

int stt_whisper_init(stt_whisper_t *s, const char *model_path,
                     int sample_rate, uint32_t energy_thresh,
                     size_t hang_samples, size_t min_speech_samples)
{
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 0 ? sample_rate : 16000;
    s->n_threads = 4;
    s->language = "en";
    s->energy_thresh = energy_thresh;
    s->hang_samples = hang_samples ? hang_samples : 1;
    s->min_speech_samples = min_speech_samples;
    s->max_samples = (size_t)s->sample_rate * 30;   /* whisper window cap: 30 s */

    struct whisper_context_params cp = whisper_context_default_params();
    s->wctx = whisper_init_from_file_with_params(model_path, cp);
    return s->wctx ? 0 : -1;
}

stt_backend_t stt_whisper_backend(stt_whisper_t *s)
{
    stt_backend_t b = {
        .reset = whisper_reset,
        .feed = whisper_feed,
        .poll = whisper_poll,
        .ctx = s,
    };
    return b;
}

void stt_whisper_free(stt_whisper_t *s)
{
    if (s->wctx) {
        whisper_free(s->wctx);
        s->wctx = NULL;
    }
    free(s->acc);
    s->acc = NULL;
    s->acc_len = s->acc_cap = 0;
}

#else  /* !SIMONSAYS_WITH_WHISPER */

/* Stubs so the symbol exists; init always fails, signalling the caller to fall back. */
int stt_whisper_init(stt_whisper_t *s, const char *model_path,
                     int sample_rate, uint32_t energy_thresh,
                     size_t hang_samples, size_t min_speech_samples)
{
    (void)model_path; (void)sample_rate; (void)energy_thresh;
    (void)hang_samples; (void)min_speech_samples;
    if (s) {
        memset(s, 0, sizeof(*s));
    }
    return -1;
}

stt_backend_t stt_whisper_backend(stt_whisper_t *s)
{
    (void)s;
    stt_backend_t b = {0};
    return b;
}

void stt_whisper_free(stt_whisper_t *s) { (void)s; }

#endif /* SIMONSAYS_WITH_WHISPER */
