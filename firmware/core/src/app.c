#include "core/app.h"

#include <stddef.h>

void app_init(app_t *a, power_ctrl_t *power, const app_hooks_t *hooks)
{
    fsm_init(&a->fsm);
    a->power = power;
    a->hooks = *hooks;
    a->cycles = 0;
}

static fsm_event_t call_hook(fsm_event_t (*fn)(void *), void *user, fsm_event_t fallback)
{
    return fn != NULL ? fn(user) : fallback;
}

wake_cause_t app_run_cycle(app_t *a)
{
    wake_cause_t cause = power_wake_cause(a->power);
    fsm_action_t act = fsm_dispatch(&a->fsm, FSM_EV_WOKE, cause);

    for (;;) {
        fsm_event_t ev;
        switch (act) {
        case FSM_ACT_START_KWS:
            ev = call_hook(a->hooks.run_kws, a->hooks.user, FSM_EV_KWS_REJECTED);
            if (ev == FSM_EV_KWS_REJECTED) {
                power_note_false_wake(a->power);
            }
            act = fsm_dispatch(&a->fsm, ev, WAKE_CAUSE_UNKNOWN);
            break;

        case FSM_ACT_START_CONNECT:
            ev = call_hook(a->hooks.connect, a->hooks.user, FSM_EV_CONNECT_FAILED);
            act = fsm_dispatch(&a->fsm, ev, WAKE_CAUSE_UNKNOWN);
            break;

        case FSM_ACT_START_STREAM:
            ev = call_hook(a->hooks.stream, a->hooks.user, FSM_EV_SESSION_TIMEOUT);
            act = fsm_dispatch(&a->fsm, ev, WAKE_CAUSE_UNKNOWN);
            break;

        case FSM_ACT_ENTER_SLEEP:
        case FSM_ACT_NONE:
        default:
            /* End of cycle (or a defensive bail on an unexpected NONE). */
            a->cycles++;
            power_enter_deep_sleep(a->power);
            return cause;
        }
    }
}

void app_run(app_t *a, uint32_t max_cycles)
{
    for (uint32_t i = 0; i < max_cycles; i++) {
        app_run_cycle(a);
        if (a->power->mode == POWER_MODE_REAL) {
            break; /* device slept; control would not have returned anyway */
        }
    }
}
