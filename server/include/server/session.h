/* Server session: ingest PCM -> VAD + STT -> intent -> debounce -> cut decision.
 *
 * Time is passed in explicitly so the whole pipeline is deterministic and host-tested
 * (tests/host/test_session.c). The POSIX socket server (server/src/main.c) is a thin loop
 * that feeds bytes from http_ingest into session_on_pcm and closes the socket when
 * session_tick reports a cut.
 */
#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "server/intent.h"
#include "server/debounce.h"
#include "server/vad.h"
#include "server/stt.h"

typedef struct {
    const intent_table_t *intents;
    vad_cfg_t             vad;
    debounce_cfg_t        debounce;
} session_cfg_t;

typedef struct {
    session_cfg_t cfg;
    stt_backend_t stt;
    debounce_t    deb;

    char        last_transcript[STT_STUB_TRANSCRIPT_MAX];
    int         last_intent;
    const char *last_intent_name;
    int         commands;
} session_t;

void session_init(session_t *s, const session_cfg_t *cfg,
                  stt_backend_t stt, uint32_t now_ms);

/* Feed a PCM payload (raw little-endian int16 bytes) observed at `now_ms`. Runs VAD,
 * STT, and intent matching, updating the debounce timer. */
void session_on_pcm(session_t *s, const uint8_t *bytes, size_t nbytes, uint32_t now_ms);

/* Advance the timers. Returns 1 if the connection should be cut. */
int session_tick(session_t *s, uint32_t now_ms);

cut_reason_t session_cut_reason(const session_t *s);

#endif /* SERVER_SESSION_H */
