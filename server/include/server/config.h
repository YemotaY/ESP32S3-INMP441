/* Default SimonSays server configuration: the built-in command set and session tuning.
 * Shared by the socket server and the e2e loopback test so they agree on intents. */
#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "server/intent.h"
#include "server/session.h"

/* Built-in demo intents (extend for your command set). */
enum {
    INTENT_LIGHT_ON = 0,
    INTENT_LIGHT_OFF,
    INTENT_STOP,
};

/* Returns the default intent table (static storage). */
const intent_table_t *server_default_intents(void);

/* Returns a default session config wired to the default intents. */
session_cfg_t server_default_session_cfg(void);

#endif /* SERVER_CONFIG_H */
