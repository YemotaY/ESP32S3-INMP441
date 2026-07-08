#include "core/app.h"
#include "mock_power.h"
#include "test.h"

/* Scripted hooks: each call returns the next event in its sequence. */
typedef struct {
    fsm_event_t kws[8];    int kws_i;
    fsm_event_t conn[8];   int conn_i;
    fsm_event_t strm[8];   int strm_i;
} script_t;

static fsm_event_t script_kws(void *u)  { script_t *s = u; return s->kws[s->kws_i++]; }
static fsm_event_t script_conn(void *u) { script_t *s = u; return s->conn[s->conn_i++]; }
static fsm_event_t script_strm(void *u) { script_t *s = u; return s->strm[s->strm_i++]; }

/* Full mission over 5 dry-run cycles exercising every branch. */
static void test_dryrun_full_mission(void)
{
    mock_power_t m;
    power_ctrl_t pc;
    mock_power_init(&m, &pc, POWER_MODE_DRYRUN);
    /* Cycle wake causes. */
    mock_power_push(&m, WAKE_CAUSE_POWER_ON);      /* 1: KWS reject      */
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER); /* 2: full stream     */
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER); /* 3: connect failure */
    mock_power_push(&m, WAKE_CAUSE_BUTTON);        /* 4: manual + timeout*/
    mock_power_push(&m, WAKE_CAUSE_TIMER);         /* 5: sleep at once   */

    script_t s = {
        .kws  = { FSM_EV_KWS_REJECTED, FSM_EV_KWS_CONFIRMED, FSM_EV_KWS_CONFIRMED },
        .conn = { FSM_EV_CONNECTED, FSM_EV_CONNECT_FAILED, FSM_EV_CONNECTED },
        .strm = { FSM_EV_SERVER_CLOSED, FSM_EV_SESSION_TIMEOUT },
    };

    app_hooks_t hooks = {
        .run_kws = script_kws,
        .connect = script_conn,
        .stream = script_strm,
        .user = &s,
    };

    app_t app;
    app_init(&app, &pc, &hooks);
    app_run(&app, 5);

    CHECK_EQ_INT(app.cycles, 5);
    CHECK_EQ_INT(m.sleeps, 5);            /* slept once per cycle */
    CHECK_EQ_INT(pc.sleep_count, 5);
    CHECK_EQ_INT(app.fsm.confirm_count, 2);
    CHECK_EQ_INT(app.fsm.reject_count, 1);
    CHECK_EQ_INT(app.fsm.session_count, 2); /* server_closed + timeout */
    CHECK_EQ_INT(app.fsm.error_count, 1);   /* connect failure */
    CHECK_EQ_INT(pc.false_wake_count, 1);   /* one KWS reject */

    /* All scripted events were consumed exactly. */
    CHECK_EQ_INT(s.kws_i, 3);
    CHECK_EQ_INT(s.conn_i, 3);
    CHECK_EQ_INT(s.strm_i, 2);
}

/* STAY_AWAKE mode processes cycles but never sleeps. */
static void test_stay_awake_processes_without_sleeping(void)
{
    mock_power_t m;
    power_ctrl_t pc;
    mock_power_init(&m, &pc, POWER_MODE_STAY_AWAKE);
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER);
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER);
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER);

    script_t s = {
        .kws  = { FSM_EV_KWS_CONFIRMED, FSM_EV_KWS_CONFIRMED, FSM_EV_KWS_CONFIRMED },
        .conn = { FSM_EV_CONNECTED, FSM_EV_CONNECTED, FSM_EV_CONNECTED },
        .strm = { FSM_EV_SERVER_CLOSED, FSM_EV_SERVER_CLOSED, FSM_EV_SERVER_CLOSED },
    };
    app_hooks_t hooks = { script_kws, script_conn, script_strm, &s };

    app_t app;
    app_init(&app, &pc, &hooks);
    app_run(&app, 3);

    CHECK_EQ_INT(app.cycles, 3);
    CHECK_EQ_INT(m.sleeps, 0);        /* stay-awake: backend never slept */
    CHECK_EQ_INT(pc.sleep_count, 0);
    CHECK_EQ_INT(app.fsm.session_count, 3);
}

/* NULL hooks fall back to safe negatives (reject / connect-fail). */
static void test_null_hooks_reject(void)
{
    mock_power_t m;
    power_ctrl_t pc;
    mock_power_init(&m, &pc, POWER_MODE_DRYRUN);
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER);

    app_hooks_t hooks = { NULL, NULL, NULL, NULL };
    app_t app;
    app_init(&app, &pc, &hooks);
    app_run(&app, 1);

    CHECK_EQ_INT(app.cycles, 1);
    CHECK_EQ_INT(app.fsm.reject_count, 1);   /* KWS defaulted to reject */
    CHECK_EQ_INT(pc.false_wake_count, 1);
    CHECK_EQ_INT(app.fsm.confirm_count, 0);
}

TEST_MAIN_BEGIN("app")
    RUN(test_dryrun_full_mission);
    RUN(test_stay_awake_processes_without_sleeping);
    RUN(test_null_hooks_reject);
TEST_MAIN_END()
