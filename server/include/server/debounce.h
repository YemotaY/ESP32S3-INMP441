/* Debounce / connection-cut logic -- the heart of the server state machine.
 *
 * After the server recognises a command it waits out a trailing-silence "debounce"
 * window; if a new command arrives the window resets, otherwise the connection is cut and
 * the device goes back to sleep. Hard caps on total session length and speechless silence
 * bound the session. Time is passed in explicitly (monotonic milliseconds) so the logic
 * is deterministic and host-tested in tests/host/test_debounce.c.
 */
#ifndef SERVER_DEBOUNCE_H
#define SERVER_DEBOUNCE_H

#include <stdint.h>

typedef struct {
    uint32_t debounce_ms;     /* trailing silence after the last command before cutting */
    uint32_t max_session_ms;  /* hard cap on total session duration */
    uint32_t silence_ms;      /* cut if no speech at all for this long */
} debounce_cfg_t;

typedef enum {
    DEBOUNCE_LISTENING = 0,   /* no command recognised yet */
    DEBOUNCE_WAITING,         /* >=1 command seen; waiting out the debounce window */
    DEBOUNCE_CUT,             /* terminal: connection should be closed */
} debounce_state_t;

/* Reason a session was cut (for logging / the dashboard). */
typedef enum {
    CUT_NONE = 0,
    CUT_DEBOUNCE,             /* trailing silence after a command */
    CUT_MAX_SESSION,          /* hit the hard session cap */
    CUT_SILENCE,              /* no speech for silence_ms */
} cut_reason_t;

typedef struct {
    debounce_cfg_t   cfg;
    debounce_state_t state;
    cut_reason_t     reason;
    uint32_t start_ms;
    uint32_t last_speech_ms;
    uint32_t last_cmd_ms;
    int      commands;
} debounce_t;

void debounce_init(debounce_t *d, const debounce_cfg_t *cfg, uint32_t now_ms);

/* Report voice activity (VAD hit) at `now_ms`. */
void debounce_note_speech(debounce_t *d, uint32_t now_ms);

/* Report a recognised command at `now_ms` (also counts as speech). */
void debounce_note_command(debounce_t *d, uint32_t now_ms);

/* Evaluate the timers at `now_ms`. Returns 1 if the connection should be cut now,
 * latching DEBOUNCE_CUT and recording `reason`. */
int debounce_should_cut(debounce_t *d, uint32_t now_ms);

const char *cut_reason_str(cut_reason_t r);

#endif /* SERVER_DEBOUNCE_H */
