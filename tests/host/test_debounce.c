#include "server/debounce.h"
#include "test.h"

static debounce_cfg_t cfg(void)
{
    debounce_cfg_t c = { .debounce_ms = 1000, .max_session_ms = 10000, .silence_ms = 4000 };
    return c;
}

static void test_cut_after_debounce_window(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);

    debounce_note_command(&d, 500);          /* command at t=500 */
    CHECK(!debounce_should_cut(&d, 1000));   /* 500ms since command < 1000 */
    CHECK(!debounce_should_cut(&d, 1499));
    CHECK(debounce_should_cut(&d, 1500));    /* 1000ms trailing silence -> cut */
    CHECK_EQ_INT(d.reason, CUT_DEBOUNCE);
    CHECK_EQ_INT(d.state, DEBOUNCE_CUT);
    CHECK_EQ_INT(d.commands, 1);
}

static void test_new_command_resets_window(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);

    debounce_note_command(&d, 500);
    CHECK(!debounce_should_cut(&d, 1200));   /* would-be cut at 1500 */
    debounce_note_command(&d, 1300);         /* new command resets timer */
    CHECK(!debounce_should_cut(&d, 2000));   /* 700ms since 1300 */
    CHECK(debounce_should_cut(&d, 2300));    /* 1000ms since 1300 -> cut */
    CHECK_EQ_INT(d.reason, CUT_DEBOUNCE);
    CHECK_EQ_INT(d.commands, 2);
}

static void test_max_session_cut(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);

    /* Keep issuing commands so debounce never fires, but hit the session cap. */
    for (uint32_t t = 500; t < 10000; t += 500) {
        debounce_note_command(&d, t);
        CHECK(!debounce_should_cut(&d, t));
    }
    CHECK(debounce_should_cut(&d, 10000));
    CHECK_EQ_INT(d.reason, CUT_MAX_SESSION);
}

static void test_silence_cut_without_command(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);

    /* No commands, no speech: silence timeout at 4000ms. */
    CHECK(!debounce_should_cut(&d, 3999));
    CHECK(debounce_should_cut(&d, 4000));
    CHECK_EQ_INT(d.reason, CUT_SILENCE);
}

static void test_speech_delays_silence_cut(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);

    debounce_note_speech(&d, 3000);          /* refresh speech clock */
    CHECK(!debounce_should_cut(&d, 6000));   /* 3000ms since last speech < 4000 */
    CHECK(debounce_should_cut(&d, 7000));    /* 4000ms since last speech -> cut */
    CHECK_EQ_INT(d.reason, CUT_SILENCE);
}

static void test_cut_latches(void)
{
    debounce_t d;
    debounce_cfg_t c = cfg();
    debounce_init(&d, &c, 0);
    debounce_note_command(&d, 0);
    CHECK(debounce_should_cut(&d, 1000));
    /* Once cut, stays cut and ignores further input. */
    debounce_note_command(&d, 1100);
    CHECK(debounce_should_cut(&d, 1100));
    CHECK_EQ_INT(d.state, DEBOUNCE_CUT);
}

TEST_MAIN_BEGIN("debounce")
    RUN(test_cut_after_debounce_window);
    RUN(test_new_command_resets_window);
    RUN(test_max_session_cut);
    RUN(test_silence_cut_without_command);
    RUN(test_speech_delays_silence_cut);
    RUN(test_cut_latches);
TEST_MAIN_END()
