#include "server/debounce.h"

void debounce_init(debounce_t *d, const debounce_cfg_t *cfg, uint32_t now_ms)
{
    d->cfg = *cfg;
    d->state = DEBOUNCE_LISTENING;
    d->reason = CUT_NONE;
    d->start_ms = now_ms;
    d->last_speech_ms = now_ms;
    d->last_cmd_ms = now_ms;
    d->commands = 0;
}

void debounce_note_speech(debounce_t *d, uint32_t now_ms)
{
    if (d->state == DEBOUNCE_CUT) {
        return;
    }
    d->last_speech_ms = now_ms;
}

void debounce_note_command(debounce_t *d, uint32_t now_ms)
{
    if (d->state == DEBOUNCE_CUT) {
        return;
    }
    d->last_speech_ms = now_ms;
    d->last_cmd_ms = now_ms;
    d->commands++;
    d->state = DEBOUNCE_WAITING;
}

/* Unsigned elapsed time, robust to monotonic-clock wraparound. */
static uint32_t elapsed(uint32_t now, uint32_t since)
{
    return now - since;
}

int debounce_should_cut(debounce_t *d, uint32_t now_ms)
{
    if (d->state == DEBOUNCE_CUT) {
        return 1;
    }

    if (d->cfg.max_session_ms && elapsed(now_ms, d->start_ms) >= d->cfg.max_session_ms) {
        d->reason = CUT_MAX_SESSION;
        d->state = DEBOUNCE_CUT;
        return 1;
    }

    if (d->state == DEBOUNCE_WAITING && d->cfg.debounce_ms &&
        elapsed(now_ms, d->last_cmd_ms) >= d->cfg.debounce_ms) {
        d->reason = CUT_DEBOUNCE;
        d->state = DEBOUNCE_CUT;
        return 1;
    }

    if (d->cfg.silence_ms && elapsed(now_ms, d->last_speech_ms) >= d->cfg.silence_ms) {
        d->reason = CUT_SILENCE;
        d->state = DEBOUNCE_CUT;
        return 1;
    }

    return 0;
}

const char *cut_reason_str(cut_reason_t r)
{
    switch (r) {
    case CUT_NONE:        return "none";
    case CUT_DEBOUNCE:    return "debounce";
    case CUT_MAX_SESSION: return "max_session";
    case CUT_SILENCE:     return "silence";
    default:              return "?";
    }
}
