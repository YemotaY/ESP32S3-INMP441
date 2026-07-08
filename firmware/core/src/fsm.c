#include "core/fsm.h"

void fsm_init(fsm_t *f)
{
    f->state = FSM_ST_BOOT;
    f->last_wake_cause = WAKE_CAUSE_UNKNOWN;
    f->confirm_count = 0;
    f->reject_count = 0;
    f->session_count = 0;
    f->error_count = 0;
}

static fsm_action_t on_wake(fsm_t *f, wake_cause_t cause)
{
    f->last_wake_cause = cause;
    switch (cause) {
    case WAKE_CAUSE_SOUND_TRIGGER:
    case WAKE_CAUSE_POWER_ON:
        f->state = FSM_ST_CONFIRM_KWS;
        return FSM_ACT_START_KWS;
    case WAKE_CAUSE_BUTTON:
        /* Manual trigger skips wake-word confirmation. */
        f->state = FSM_ST_CONNECT;
        return FSM_ACT_START_CONNECT;
    case WAKE_CAUSE_TIMER:
    case WAKE_CAUSE_UNKNOWN:
    default:
        f->state = FSM_ST_SLEEP;
        return FSM_ACT_ENTER_SLEEP;
    }
}

fsm_action_t fsm_dispatch(fsm_t *f, fsm_event_t ev, wake_cause_t cause)
{
    /* A hard error from any state routes to ERROR then sleep. */
    if (ev == FSM_EV_ERROR) {
        f->error_count++;
        f->state = FSM_ST_ERROR;
        return FSM_ACT_ENTER_SLEEP;
    }

    switch (f->state) {
    case FSM_ST_BOOT:
    case FSM_ST_SLEEP:
    case FSM_ST_ERROR:
        if (ev == FSM_EV_WOKE) {
            return on_wake(f, cause);
        }
        break;

    case FSM_ST_CONFIRM_KWS:
        if (ev == FSM_EV_KWS_CONFIRMED) {
            f->confirm_count++;
            f->state = FSM_ST_CONNECT;
            return FSM_ACT_START_CONNECT;
        }
        if (ev == FSM_EV_KWS_REJECTED) {
            f->reject_count++;
            f->state = FSM_ST_SLEEP;
            return FSM_ACT_ENTER_SLEEP;
        }
        break;

    case FSM_ST_CONNECT:
        if (ev == FSM_EV_CONNECTED) {
            f->state = FSM_ST_STREAM;
            return FSM_ACT_START_STREAM;
        }
        if (ev == FSM_EV_CONNECT_FAILED) {
            f->error_count++;
            f->state = FSM_ST_ERROR;
            return FSM_ACT_ENTER_SLEEP;
        }
        break;

    case FSM_ST_STREAM:
        if (ev == FSM_EV_SERVER_CLOSED || ev == FSM_EV_SESSION_TIMEOUT) {
            f->session_count++;
            f->state = FSM_ST_SLEEP;
            return FSM_ACT_ENTER_SLEEP;
        }
        break;
    }

    /* Unhandled event for the current state: remain, no action. */
    return FSM_ACT_NONE;
}

const char *fsm_state_str(fsm_state_t s)
{
    switch (s) {
    case FSM_ST_BOOT:        return "BOOT";
    case FSM_ST_CONFIRM_KWS: return "CONFIRM_KWS";
    case FSM_ST_CONNECT:     return "CONNECT";
    case FSM_ST_STREAM:      return "STREAM";
    case FSM_ST_SLEEP:       return "SLEEP";
    case FSM_ST_ERROR:       return "ERROR";
    default:                 return "?";
    }
}

const char *fsm_event_str(fsm_event_t e)
{
    switch (e) {
    case FSM_EV_NONE:            return "NONE";
    case FSM_EV_WOKE:            return "WOKE";
    case FSM_EV_KWS_CONFIRMED:   return "KWS_CONFIRMED";
    case FSM_EV_KWS_REJECTED:    return "KWS_REJECTED";
    case FSM_EV_CONNECTED:       return "CONNECTED";
    case FSM_EV_CONNECT_FAILED:  return "CONNECT_FAILED";
    case FSM_EV_SERVER_CLOSED:   return "SERVER_CLOSED";
    case FSM_EV_SESSION_TIMEOUT: return "SESSION_TIMEOUT";
    case FSM_EV_ERROR:           return "ERROR";
    default:                     return "?";
    }
}

const char *fsm_action_str(fsm_action_t a)
{
    switch (a) {
    case FSM_ACT_NONE:          return "NONE";
    case FSM_ACT_START_KWS:     return "START_KWS";
    case FSM_ACT_START_CONNECT: return "START_CONNECT";
    case FSM_ACT_START_STREAM:  return "START_STREAM";
    case FSM_ACT_ENTER_SLEEP:   return "ENTER_SLEEP";
    default:                    return "?";
    }
}
