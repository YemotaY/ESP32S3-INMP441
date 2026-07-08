#include "server/config.h"

static const intent_entry_t k_intents[] = {
    { INTENT_LIGHT_ON,  "light_on",  "light on" },
    { INTENT_LIGHT_OFF, "light_off", "light off" },
    { INTENT_STOP,      "stop",      "stop" },
};

static const intent_table_t k_table = {
    .entries = k_intents,
    .count = sizeof(k_intents) / sizeof(k_intents[0]),
};

const intent_table_t *server_default_intents(void)
{
    return &k_table;
}

session_cfg_t server_default_session_cfg(void)
{
    session_cfg_t cfg = {
        .intents = &k_table,
        .vad = { .energy_thresh = 500 },
        .debounce = {
            .debounce_ms = 1000,
            .max_session_ms = 15000,
            .silence_ms = 4000,
        },
    };
    return cfg;
}
