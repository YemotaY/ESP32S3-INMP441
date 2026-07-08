/* Application state machine (pure reducer).
 *
 * fsm_dispatch maps (state, event, wake-cause) -> (new state, action). It performs
 * NO I/O: the caller (device or host test) executes the returned action. This keeps
 * the control flow fully deterministic and unit-testable.
 *
 * Flow: BOOT/SLEEP --woke--> CONFIRM_KWS --confirmed--> CONNECT --connected-->
 *       STREAM --server_closed/timeout--> SLEEP.  Rejections/failures -> SLEEP/ERROR.
 */
#ifndef CORE_FSM_H
#define CORE_FSM_H

#include <stdint.h>
#include "core/power.h"

typedef enum {
    FSM_ST_BOOT = 0,
    FSM_ST_CONFIRM_KWS,
    FSM_ST_CONNECT,
    FSM_ST_STREAM,
    FSM_ST_SLEEP,
    FSM_ST_ERROR,
} fsm_state_t;

typedef enum {
    FSM_EV_NONE = 0,
    FSM_EV_WOKE,             /* uses wake cause */
    FSM_EV_KWS_CONFIRMED,
    FSM_EV_KWS_REJECTED,
    FSM_EV_CONNECTED,
    FSM_EV_CONNECT_FAILED,
    FSM_EV_SERVER_CLOSED,
    FSM_EV_SESSION_TIMEOUT,
    FSM_EV_ERROR,
} fsm_event_t;

typedef enum {
    FSM_ACT_NONE = 0,
    FSM_ACT_START_KWS,
    FSM_ACT_START_CONNECT,
    FSM_ACT_START_STREAM,
    FSM_ACT_ENTER_SLEEP,
} fsm_action_t;

typedef struct {
    fsm_state_t  state;
    wake_cause_t last_wake_cause;
    uint32_t     confirm_count;  /* KWS confirmations */
    uint32_t     reject_count;   /* KWS rejections */
    uint32_t     session_count;  /* completed streaming sessions */
    uint32_t     error_count;    /* errors / connect failures */
} fsm_t;

void fsm_init(fsm_t *f);

/* Apply an event; update state and counters; return the action to perform.
 * `cause` is only meaningful for FSM_EV_WOKE. */
fsm_action_t fsm_dispatch(fsm_t *f, fsm_event_t ev, wake_cause_t cause);

const char *fsm_state_str(fsm_state_t s);
const char *fsm_event_str(fsm_event_t e);
const char *fsm_action_str(fsm_action_t a);

#endif /* CORE_FSM_H */
