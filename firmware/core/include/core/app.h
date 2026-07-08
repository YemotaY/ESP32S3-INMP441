/* Session runner: glues the pure FSM to the power controller and to injected
 * hooks that perform the real work (wake-word confirm, connect, stream).
 *
 * On device the hooks call I2S/KWS/Wi-Fi/HTTP code; in host tests they return
 * scripted events. This lets the full wake -> ... -> sleep control flow be
 * verified without any hardware.
 */
#ifndef CORE_APP_H
#define CORE_APP_H

#include <stdint.h>
#include "core/fsm.h"
#include "core/power.h"

/* Each hook performs its step and returns the resulting FSM event.
 *   run_kws -> FSM_EV_KWS_CONFIRMED | FSM_EV_KWS_REJECTED | FSM_EV_ERROR
 *   connect -> FSM_EV_CONNECTED     | FSM_EV_CONNECT_FAILED | FSM_EV_ERROR
 *   stream  -> FSM_EV_SERVER_CLOSED | FSM_EV_SESSION_TIMEOUT | FSM_EV_ERROR
 * A NULL hook is treated as a safe negative result. */
typedef struct {
    fsm_event_t (*run_kws)(void *user);
    fsm_event_t (*connect)(void *user);
    fsm_event_t (*stream)(void *user);
    void *user;
} app_hooks_t;

typedef struct {
    fsm_t         fsm;
    power_ctrl_t *power;
    app_hooks_t   hooks;
    uint32_t      cycles; /* completed wake->sleep cycles */
} app_t;

void app_init(app_t *a, power_ctrl_t *power, const app_hooks_t *hooks);

/* Run exactly one wake->...->sleep cycle. Returns the wake cause processed.
 * In REAL power mode the final power_enter_deep_sleep does not return. */
wake_cause_t app_run_cycle(app_t *a);

/* Run up to `max_cycles` cycles (host dry-run / stay-awake loops).
 * In REAL mode a single cycle runs and the device sleeps. */
void app_run(app_t *a, uint32_t max_cycles);

#endif /* CORE_APP_H */
