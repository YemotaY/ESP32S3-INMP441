#include "core/fsm.h"
#include "test.h"

static void test_init(void)
{
    fsm_t f;
    fsm_init(&f);
    CHECK_EQ_INT(f.state, FSM_ST_BOOT);
    CHECK_EQ_INT(f.confirm_count, 0);
    CHECK_EQ_INT(f.reject_count, 0);
    CHECK_EQ_INT(f.session_count, 0);
    CHECK_EQ_INT(f.error_count, 0);
}

static void test_sound_wake_starts_kws(void)
{
    fsm_t f;
    fsm_init(&f);
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER);
    CHECK_EQ_INT(a, FSM_ACT_START_KWS);
    CHECK_EQ_INT(f.state, FSM_ST_CONFIRM_KWS);
    CHECK_EQ_INT(f.last_wake_cause, WAKE_CAUSE_SOUND_TRIGGER);
}

static void test_power_on_starts_kws(void)
{
    fsm_t f;
    fsm_init(&f);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_POWER_ON), FSM_ACT_START_KWS);
    CHECK_EQ_INT(f.state, FSM_ST_CONFIRM_KWS);
}

static void test_button_skips_kws(void)
{
    fsm_t f;
    fsm_init(&f);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_BUTTON), FSM_ACT_START_CONNECT);
    CHECK_EQ_INT(f.state, FSM_ST_CONNECT);
}

static void test_timer_wake_sleeps(void)
{
    fsm_t f;
    fsm_init(&f);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_TIMER), FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.state, FSM_ST_SLEEP);
}

static void test_kws_rejected_sleeps(void)
{
    fsm_t f;
    fsm_init(&f);
    fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER);
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_KWS_REJECTED, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(a, FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.state, FSM_ST_SLEEP);
    CHECK_EQ_INT(f.reject_count, 1);
}

static void test_full_happy_path(void)
{
    fsm_t f;
    fsm_init(&f);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER), FSM_ACT_START_KWS);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_KWS_CONFIRMED, WAKE_CAUSE_UNKNOWN), FSM_ACT_START_CONNECT);
    CHECK_EQ_INT(f.confirm_count, 1);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_CONNECTED, WAKE_CAUSE_UNKNOWN), FSM_ACT_START_STREAM);
    CHECK_EQ_INT(f.state, FSM_ST_STREAM);
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_SERVER_CLOSED, WAKE_CAUSE_UNKNOWN), FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.state, FSM_ST_SLEEP);
    CHECK_EQ_INT(f.session_count, 1);
}

static void test_connect_failed_errors(void)
{
    fsm_t f;
    fsm_init(&f);
    fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER);
    fsm_dispatch(&f, FSM_EV_KWS_CONFIRMED, WAKE_CAUSE_UNKNOWN);
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_CONNECT_FAILED, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(a, FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.state, FSM_ST_ERROR);
    CHECK_EQ_INT(f.error_count, 1);
}

static void test_stream_timeout_sleeps(void)
{
    fsm_t f;
    fsm_init(&f);
    fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_BUTTON);
    fsm_dispatch(&f, FSM_EV_CONNECTED, WAKE_CAUSE_UNKNOWN);
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_SESSION_TIMEOUT, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(a, FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.session_count, 1);
}

static void test_error_event_from_any_state(void)
{
    fsm_t f;
    fsm_init(&f);
    fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER); /* in CONFIRM_KWS */
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_ERROR, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(a, FSM_ACT_ENTER_SLEEP);
    CHECK_EQ_INT(f.state, FSM_ST_ERROR);
    CHECK_EQ_INT(f.error_count, 1);
}

static void test_unhandled_event_is_noop(void)
{
    fsm_t f;
    fsm_init(&f);
    /* CONNECTED while still in BOOT is not valid -> no-op, stays in BOOT. */
    fsm_action_t a = fsm_dispatch(&f, FSM_EV_CONNECTED, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(a, FSM_ACT_NONE);
    CHECK_EQ_INT(f.state, FSM_ST_BOOT);
}

static void test_sleep_then_rewake(void)
{
    fsm_t f;
    fsm_init(&f);
    /* One full cycle to SLEEP. */
    fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER);
    fsm_dispatch(&f, FSM_EV_KWS_REJECTED, WAKE_CAUSE_UNKNOWN);
    CHECK_EQ_INT(f.state, FSM_ST_SLEEP);
    /* Waking from SLEEP behaves like BOOT. */
    CHECK_EQ_INT(fsm_dispatch(&f, FSM_EV_WOKE, WAKE_CAUSE_SOUND_TRIGGER), FSM_ACT_START_KWS);
    CHECK_EQ_INT(f.state, FSM_ST_CONFIRM_KWS);
}

static void test_string_helpers(void)
{
    CHECK_STR_EQ(fsm_state_str(FSM_ST_STREAM), "STREAM");
    CHECK_STR_EQ(fsm_event_str(FSM_EV_SERVER_CLOSED), "SERVER_CLOSED");
    CHECK_STR_EQ(fsm_action_str(FSM_ACT_ENTER_SLEEP), "ENTER_SLEEP");
}

TEST_MAIN_BEGIN("fsm")
    RUN(test_init);
    RUN(test_sound_wake_starts_kws);
    RUN(test_power_on_starts_kws);
    RUN(test_button_skips_kws);
    RUN(test_timer_wake_sleeps);
    RUN(test_kws_rejected_sleeps);
    RUN(test_full_happy_path);
    RUN(test_connect_failed_errors);
    RUN(test_stream_timeout_sleeps);
    RUN(test_error_event_from_any_state);
    RUN(test_unhandled_event_is_noop);
    RUN(test_sleep_then_rewake);
    RUN(test_string_helpers);
TEST_MAIN_END()
