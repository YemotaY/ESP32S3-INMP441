#include "server/stt.h"

#include <string.h>

static void push_pending(stt_stub_t *s, const char *text)
{
    size_t next = (s->pend_tail + 1) % STT_STUB_MAX_PENDING;
    if (next == s->pend_head) {
        return;   /* ring full: drop (bounded, no malloc) */
    }
    size_t n = strlen(text);
    if (n >= STT_STUB_TRANSCRIPT_MAX) {
        n = STT_STUB_TRANSCRIPT_MAX - 1;
    }
    memcpy(s->pending[s->pend_tail], text, n);
    s->pending[s->pend_tail][n] = '\0';
    s->pend_tail = next;
}

static void end_utterance(stt_stub_t *s)
{
    if (s->speech_run >= s->min_speech_samples && s->next_script < s->script_count) {
        push_pending(s, s->scripts[s->next_script++]);
    }
    s->in_speech = 0;
    s->speech_run = 0;
    s->silence_run = 0;
}

static void stub_reset(void *ctx)
{
    stt_stub_t *s = ctx;
    s->next_script = 0;
    s->in_speech = 0;
    s->speech_run = 0;
    s->silence_run = 0;
    s->pend_head = s->pend_tail = 0;
}

static void stub_feed(void *ctx, const int16_t *pcm, size_t n)
{
    stt_stub_t *s = ctx;
    for (size_t i = 0; i < n; i++) {
        int32_t v = pcm[i];
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

static int stub_poll(void *ctx, char *out, size_t cap)
{
    stt_stub_t *s = ctx;
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
    s->pend_head = (s->pend_head + 1) % STT_STUB_MAX_PENDING;
    return 1;
}

void stt_stub_init(stt_stub_t *s, const char *const *scripts, size_t count,
                   uint32_t energy_thresh, size_t hang_samples, size_t min_speech_samples)
{
    memset(s, 0, sizeof(*s));
    s->scripts = scripts;
    s->script_count = count;
    s->energy_thresh = energy_thresh;
    s->hang_samples = hang_samples ? hang_samples : 1;
    s->min_speech_samples = min_speech_samples;
}

stt_backend_t stt_stub_backend(stt_stub_t *s)
{
    stt_backend_t b = {
        .reset = stub_reset,
        .feed = stub_feed,
        .poll = stub_poll,
        .ctx = s,
    };
    return b;
}
