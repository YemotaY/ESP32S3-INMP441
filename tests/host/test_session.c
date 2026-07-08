#include "server/session.h"
#include "server/config.h"
#include "server/http_ingest.h"
#include "core/net/stream_proto.h"
#include "test.h"

#include <string.h>

/* Append `n` little-endian int16 samples of constant amplitude `amp` to buf. */
static void append_pcm(uint8_t *buf, size_t *len, int n, int16_t amp)
{
    for (int i = 0; i < n; i++) {
        uint16_t u = (uint16_t)amp;
        buf[(*len)++] = (uint8_t)(u & 0xFF);
        buf[(*len)++] = (uint8_t)(u >> 8);
    }
}

static const intent_entry_t entries[] = {
    { 0, "light_on", "light on" },
    { 1, "stop",     "stop" },
};
static const intent_table_t table = { .entries = entries, .count = 2 };

static session_cfg_t make_cfg(void)
{
    session_cfg_t cfg = {
        .intents = &table,
        .vad = { .energy_thresh = 500 },
        .debounce = { .debounce_ms = 1000, .max_session_ms = 15000, .silence_ms = 4000 },
    };
    return cfg;
}

/* One utterance: 200 speech samples then 100 silence (ends the utterance for the stub). */
static size_t make_utterance(uint8_t *buf, int16_t amp)
{
    size_t len = 0;
    append_pcm(buf, &len, 200, amp);
    append_pcm(buf, &len, 100, 0);
    return len;
}

static void test_command_then_debounce_cut(void)
{
    static const char *scripts[] = { "turn the light on" };
    stt_stub_t stub;
    stt_stub_init(&stub, scripts, 1, 500, 50, 20);

    session_t s;
    session_cfg_t cfg = make_cfg();
    session_init(&s, &cfg, stt_stub_backend(&stub), 0);

    uint8_t pcm[2048];
    size_t len = make_utterance(pcm, 4000);
    session_on_pcm(&s, pcm, len, 100);

    CHECK_EQ_INT(s.commands, 1);
    CHECK_EQ_INT(s.last_intent, 0);
    CHECK_STR_EQ(s.last_transcript, "turn the light on");

    CHECK(!session_tick(&s, 100));
    CHECK(!session_tick(&s, 1000));
    CHECK(session_tick(&s, 1100));
    CHECK_EQ_INT(session_cut_reason(&s), CUT_DEBOUNCE);
}

static void test_non_command_silence_cut(void)
{
    static const char *scripts[] = { "hello there world" };
    stt_stub_t stub;
    stt_stub_init(&stub, scripts, 1, 500, 50, 20);

    session_t s;
    session_cfg_t cfg = make_cfg();
    session_init(&s, &cfg, stt_stub_backend(&stub), 0);

    uint8_t pcm[2048];
    size_t len = make_utterance(pcm, 4000);
    session_on_pcm(&s, pcm, len, 100);

    CHECK_EQ_INT(s.commands, 0);                 /* transcript matched no intent */
    CHECK_STR_EQ(s.last_transcript, "hello there world");
    CHECK(!session_tick(&s, 4000));              /* 3900ms since speech@100 */
    CHECK(session_tick(&s, 4100));               /* 4000ms of silence -> cut */
    CHECK_EQ_INT(session_cut_reason(&s), CUT_SILENCE);
}

/* Full data path: client framing -> http_ingest -> session. */
typedef struct {
    session_t *sess;
    uint32_t   now;
} ingest_ctx_t;

static void ingest_payload(void *user, const uint8_t *data, size_t len)
{
    ingest_ctx_t *c = user;
    session_on_pcm(c->sess, data, len, c->now);
}

static void test_full_path_client_to_session(void)
{
    static const char *scripts[] = { "light on" };
    stt_stub_t stub;
    stt_stub_init(&stub, scripts, 1, 500, 50, 20);

    session_t s;
    session_cfg_t cfg = make_cfg();
    session_init(&s, &cfg, stt_stub_backend(&stub), 0);

    /* Build the exact bytes the device would send. */
    uint8_t pcm[2048];
    size_t plen = make_utterance(pcm, 4000);

    uint8_t req[4096];
    stream_proto_cfg_t pcfg = { .path = "/v1/stream", .host = "h",
                                .session_id = "s", .wake_conf_milli = 900 };
    size_t pos = stream_proto_build_request_header((char *)req, sizeof(req), &pcfg);
    for (size_t off = 0; off < plen; off += 128) {
        size_t n = plen - off < 128 ? plen - off : 128;
        pos += stream_proto_encode_chunk((char *)req + pos, sizeof(req) - pos,
                                         pcm + off, n);
    }
    pos += stream_proto_encode_final((char *)req + pos, sizeof(req) - pos);

    ingest_ctx_t ctx = { .sess = &s, .now = 100 };
    http_ingest_t ing;
    http_ingest_init(&ing, ingest_payload, NULL, &ctx);
    long consumed = http_ingest_feed(&ing, req, pos);

    CHECK_EQ_INT(consumed, (long)pos);
    CHECK(ing.complete);
    CHECK_EQ_INT(s.commands, 1);
    CHECK_EQ_INT(s.last_intent, 0);
    CHECK(session_tick(&s, 1200));
    CHECK_EQ_INT(session_cut_reason(&s), CUT_DEBOUNCE);
}

static void test_default_config_intents(void)
{
    const intent_table_t *t = server_default_intents();
    const char *name = NULL;
    CHECK_EQ_INT(intent_match(t, "please turn the light on", &name), INTENT_LIGHT_ON);
    CHECK_STR_EQ(name, "light_on");
    CHECK_EQ_INT(intent_match(t, "stop it", NULL), INTENT_STOP);
}

TEST_MAIN_BEGIN("session")
    RUN(test_command_then_debounce_cut);
    RUN(test_non_command_silence_cut);
    RUN(test_full_path_client_to_session);
    RUN(test_default_config_intents);
TEST_MAIN_END()
