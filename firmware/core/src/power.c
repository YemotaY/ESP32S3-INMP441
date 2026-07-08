#include "core/power.h"

#include <stddef.h>

void power_init(power_ctrl_t *pc, power_mode_t mode, const power_backend_t *backend)
{
    pc->mode = mode;
    if (backend != NULL) {
        pc->backend = *backend;
    } else {
        pc->backend.enter_deep_sleep = NULL;
        pc->backend.wake_cause = NULL;
        pc->backend.ctx = NULL;
    }
    pc->sleep_count = 0;
    pc->false_wake_count = 0;
}

wake_cause_t power_wake_cause(power_ctrl_t *pc)
{
    if (pc->backend.wake_cause != NULL) {
        return pc->backend.wake_cause(pc);
    }
    /* No backend: treat every cycle as a cold boot (useful in STAY_AWAKE dev). */
    return WAKE_CAUSE_POWER_ON;
}

bool power_enter_deep_sleep(power_ctrl_t *pc)
{
    if (pc->mode == POWER_MODE_STAY_AWAKE) {
        return true; /* never sleep during initial development */
    }

    pc->sleep_count++;

    if (pc->backend.enter_deep_sleep != NULL) {
        pc->backend.enter_deep_sleep(pc);
        /* REAL backend does not return; DRYRUN backend does. */
    }
    return true;
}

void power_note_false_wake(power_ctrl_t *pc)
{
    pc->false_wake_count++;
}

const char *power_wake_cause_str(wake_cause_t c)
{
    switch (c) {
    case WAKE_CAUSE_UNKNOWN:       return "unknown";
    case WAKE_CAUSE_POWER_ON:      return "power_on";
    case WAKE_CAUSE_SOUND_TRIGGER: return "sound_trigger";
    case WAKE_CAUSE_BUTTON:        return "button";
    case WAKE_CAUSE_TIMER:         return "timer";
    default:                       return "?";
    }
}

const char *power_mode_str(power_mode_t m)
{
    switch (m) {
    case POWER_MODE_STAY_AWAKE: return "stay_awake";
    case POWER_MODE_DRYRUN:     return "dryrun";
    case POWER_MODE_REAL:       return "real";
    default:                    return "?";
    }
}
