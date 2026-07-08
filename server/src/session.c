#include "server/session.h"

#include <string.h>

#define DECODE_BLOCK 256   /* samples per stack decode block */

void session_init(session_t *s, const session_cfg_t *cfg,
                  stt_backend_t stt, uint32_t now_ms)
{
    memset(s, 0, sizeof(*s));
    s->cfg = *cfg;
    s->stt = stt;
    s->last_intent = INTENT_NONE;
    if (s->stt.reset) {
        s->stt.reset(s->stt.ctx);
    }
    debounce_init(&s->deb, &cfg->debounce, now_ms);
}

/* Decode `count` little-endian int16 samples from `bytes` into `out`. */
static void decode_le16(const uint8_t *bytes, int16_t *out, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        out[i] = (int16_t)((uint16_t)bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8));
    }
}

void session_on_pcm(session_t *s, const uint8_t *bytes, size_t nbytes, uint32_t now_ms)
{
    size_t total = nbytes / 2;   /* whole int16 samples */
    uint64_t energy_sum = 0;
    size_t   energy_n = 0;
    int16_t  block[DECODE_BLOCK];

    for (size_t off = 0; off < total; off += DECODE_BLOCK) {
        size_t n = total - off;
        if (n > DECODE_BLOCK) {
            n = DECODE_BLOCK;
        }
        decode_le16(bytes + 2 * off, block, n);
        energy_sum += (uint64_t)vad_frame_energy(block, n) * n;
        energy_n += n;
        if (s->stt.feed) {
            s->stt.feed(s->stt.ctx, block, n);
        }
    }

    if (energy_n > 0) {
        uint32_t mean = (uint32_t)(energy_sum / energy_n);
        if (mean > s->cfg.vad.energy_thresh) {
            debounce_note_speech(&s->deb, now_ms);
        }
    }

    /* Drain any finalised transcripts and match intents. */
    char transcript[STT_STUB_TRANSCRIPT_MAX];
    while (s->stt.poll && s->stt.poll(s->stt.ctx, transcript, sizeof(transcript))) {
        strncpy(s->last_transcript, transcript, sizeof(s->last_transcript) - 1);
        s->last_transcript[sizeof(s->last_transcript) - 1] = '\0';
        const char *name = NULL;
        int id = intent_match(s->cfg.intents, transcript, &name);
        if (id != INTENT_NONE) {
            s->last_intent = id;
            s->last_intent_name = name;
            s->commands++;
            debounce_note_command(&s->deb, now_ms);
        }
    }
}

int session_tick(session_t *s, uint32_t now_ms)
{
    return debounce_should_cut(&s->deb, now_ms);
}

cut_reason_t session_cut_reason(const session_t *s)
{
    return s->deb.reason;
}
