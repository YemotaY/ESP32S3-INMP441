#include "core/power.h"
#include "mock_power.h"
#include "test.h"

static void test_stay_awake_never_sleeps(void)
{
    mock_power_t m;
    power_ctrl_t pc;
    mock_power_init(&m, &pc, POWER_MODE_STAY_AWAKE);

    CHECK(power_enter_deep_sleep(&pc));
    CHECK(power_enter_deep_sleep(&pc));
    CHECK_EQ_INT(m.sleeps, 0);       /* backend never invoked */
    CHECK_EQ_INT(pc.sleep_count, 0); /* nothing counted */
}

static void test_dryrun_sleeps_and_delivers_wake(void)
{
    mock_power_t m;
    power_ctrl_t pc;
    mock_power_init(&m, &pc, POWER_MODE_DRYRUN);
    mock_power_push(&m, WAKE_CAUSE_POWER_ON);
    mock_power_push(&m, WAKE_CAUSE_SOUND_TRIGGER);

    CHECK_EQ_INT(power_wake_cause(&pc), WAKE_CAUSE_POWER_ON);
    CHECK(power_enter_deep_sleep(&pc)); /* returns in dry-run */
    CHECK_EQ_INT(power_wake_cause(&pc), WAKE_CAUSE_SOUND_TRIGGER);
    CHECK(power_enter_deep_sleep(&pc));

    CHECK_EQ_INT(m.sleeps, 2);
    CHECK_EQ_INT(pc.sleep_count, 2);
    /* Queue exhausted -> unknown. */
    CHECK_EQ_INT(power_wake_cause(&pc), WAKE_CAUSE_UNKNOWN);
}

static void test_no_backend_defaults_power_on(void)
{
    power_ctrl_t pc;
    power_init(&pc, POWER_MODE_STAY_AWAKE, NULL);
    CHECK_EQ_INT(power_wake_cause(&pc), WAKE_CAUSE_POWER_ON);
}

static void test_false_wake_counter(void)
{
    power_ctrl_t pc;
    power_init(&pc, POWER_MODE_DRYRUN, NULL);
    CHECK_EQ_INT(pc.false_wake_count, 0);
    power_note_false_wake(&pc);
    power_note_false_wake(&pc);
    CHECK_EQ_INT(pc.false_wake_count, 2);
}

static void test_string_helpers(void)
{
    CHECK_STR_EQ(power_mode_str(POWER_MODE_STAY_AWAKE), "stay_awake");
    CHECK_STR_EQ(power_mode_str(POWER_MODE_DRYRUN), "dryrun");
    CHECK_STR_EQ(power_mode_str(POWER_MODE_REAL), "real");
    CHECK_STR_EQ(power_wake_cause_str(WAKE_CAUSE_SOUND_TRIGGER), "sound_trigger");
    CHECK_STR_EQ(power_wake_cause_str(WAKE_CAUSE_BUTTON), "button");
}

TEST_MAIN_BEGIN("power")
    RUN(test_stay_awake_never_sleeps);
    RUN(test_dryrun_sleeps_and_delivers_wake);
    RUN(test_no_backend_defaults_power_on);
    RUN(test_false_wake_counter);
    RUN(test_string_helpers);
TEST_MAIN_END()
