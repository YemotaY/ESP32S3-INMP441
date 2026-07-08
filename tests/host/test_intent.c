#include "server/intent.h"
#include "test.h"

static const intent_entry_t entries[] = {
    { 10, "light_on",  "light on" },
    { 11, "light_off", "light off" },
    { 12, "stop",      "stop" },
    { 13, "play_music","play music" },
};
static const intent_table_t table = {
    .entries = entries, .count = sizeof(entries) / sizeof(entries[0]),
};

static void test_single_keyword(void)
{
    const char *name = NULL;
    CHECK_EQ_INT(intent_match(&table, "please stop now", &name), 12);
    CHECK_STR_EQ(name, "stop");
}

static void test_multi_keyword_all_required(void)
{
    const char *name = NULL;
    CHECK_EQ_INT(intent_match(&table, "turn the light on please", &name), 10);
    CHECK_STR_EQ(name, "light_on");
    /* "light" alone (no "on"/"off") should not match light_on or light_off */
    CHECK_EQ_INT(intent_match(&table, "the light is nice", NULL), INTENT_NONE);
}

static void test_case_insensitive(void)
{
    CHECK_EQ_INT(intent_match(&table, "LIGHT OFF", NULL), 11);
    CHECK_EQ_INT(intent_match(&table, "Play Some MUSIC", NULL), 13);
}

static void test_no_match(void)
{
    const char *name = "x";
    CHECK_EQ_INT(intent_match(&table, "what time is it", &name), INTENT_NONE);
    CHECK(name == NULL);
}

static void test_first_match_wins(void)
{
    /* Contains both "light on" and "stop"; light_on is listed first. */
    CHECK_EQ_INT(intent_match(&table, "light on and then stop", NULL), 10);
}

static void test_null_safe(void)
{
    CHECK_EQ_INT(intent_match(NULL, "x", NULL), INTENT_NONE);
    CHECK_EQ_INT(intent_match(&table, NULL, NULL), INTENT_NONE);
}

TEST_MAIN_BEGIN("intent")
    RUN(test_single_keyword);
    RUN(test_multi_keyword_all_required);
    RUN(test_case_insensitive);
    RUN(test_no_match);
    RUN(test_first_match_wins);
    RUN(test_null_safe);
TEST_MAIN_END()
