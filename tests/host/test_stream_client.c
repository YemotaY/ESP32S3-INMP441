#include "core/net/stream_client.h"
#include "core/net/stream_proto.h"
#include "test.h"

#include <string.h>

/* --- mock transport --- */
typedef struct {
    uint8_t sent[8192];
    size_t  sent_len;
    int     recv_calls;
    int     close_after;   /* return "closed" once recv_calls exceeds this; -1 = never */
    int     fail_send;
} mock_tr_t;

static long m_send(void *u, const void *b, size_t l)
{
    mock_tr_t *m = u;
    if (m->fail_send) {
        return -1;
    }
    if (m->sent_len + l <= sizeof(m->sent)) {
        memcpy(m->sent + m->sent_len, b, l);
    }
    m->sent_len += l;
    return (long)l;
}

static long m_recv(void *u, void *b, size_t l)
{
    mock_tr_t *m = u;
    (void)b; (void)l;
    m->recv_calls++;
    if (m->close_after >= 0 && m->recv_calls > m->close_after) {
        return 0;
    }
    return STREAM_WOULDBLOCK;
}

/* --- mock PCM source --- */
typedef struct {
    int    frames_left;
    size_t frame_bytes;
} mock_src_t;

static size_t m_read(void *u, void *b, size_t cap)
{
    mock_src_t *s = u;
    if (s->frames_left <= 0) {
        return 0;
    }
    size_t n = s->frame_bytes < cap ? s->frame_bytes : cap;
    memset(b, 0xAA, n);
    s->frames_left--;
    return n;
}

static stream_client_cfg_t make_cfg(uint32_t max_chunks)
{
    stream_client_cfg_t cfg = {
        .proto = { .path = "/v1/stream", .host = "h", .session_id = "s",
                   .wake_conf_milli = 900 },
        .frame_bytes = 8,
        .max_chunks = max_chunks,
    };
    return cfg;
}

static void test_server_closes_midstream(void)
{
    mock_tr_t tr = { .close_after = 2 };
    mock_src_t sr = { .frames_left = 100, .frame_bytes = 8 };
    stream_transport_t t = { .send = m_send, .recv = m_recv, .user = &tr };
    pcm_source_t s = { .read = m_read, .user = &sr };
    uint8_t scratch[256];

    stream_client_cfg_t cfg = make_cfg(100);
    fsm_event_t ev = stream_client_run(&cfg, &t, &s, scratch, sizeof(scratch));
    CHECK_EQ_INT(ev, FSM_EV_SERVER_CLOSED);
    CHECK(strncmp((char *)tr.sent, "POST /v1/stream", 15) == 0);
}

static void test_source_exhausts_then_close(void)
{
    mock_tr_t tr = { .close_after = 3 };
    mock_src_t sr = { .frames_left = 2, .frame_bytes = 8 };
    stream_transport_t t = { .send = m_send, .recv = m_recv, .user = &tr };
    pcm_source_t s = { .read = m_read, .user = &sr };
    uint8_t scratch[256];

    stream_client_cfg_t cfg = make_cfg(100);
    fsm_event_t ev = stream_client_run(&cfg, &t, &s, scratch, sizeof(scratch));
    CHECK_EQ_INT(ev, FSM_EV_SERVER_CLOSED);
    /* The terminating chunk must have been sent. */
    CHECK(tr.sent_len >= 5);
    CHECK(memcmp(tr.sent + tr.sent_len - 5, "0\r\n\r\n", 5) == 0);
}

static void test_session_timeout(void)
{
    mock_tr_t tr = { .close_after = -1 };   /* server never closes */
    mock_src_t sr = { .frames_left = 1, .frame_bytes = 8 };
    stream_transport_t t = { .send = m_send, .recv = m_recv, .user = &tr };
    pcm_source_t s = { .read = m_read, .user = &sr };
    uint8_t scratch[256];

    stream_client_cfg_t cfg = make_cfg(4);
    fsm_event_t ev = stream_client_run(&cfg, &t, &s, scratch, sizeof(scratch));
    CHECK_EQ_INT(ev, FSM_EV_SESSION_TIMEOUT);
}

static void test_send_failure(void)
{
    mock_tr_t tr = { .close_after = -1, .fail_send = 1 };
    mock_src_t sr = { .frames_left = 5, .frame_bytes = 8 };
    stream_transport_t t = { .send = m_send, .recv = m_recv, .user = &tr };
    pcm_source_t s = { .read = m_read, .user = &sr };
    uint8_t scratch[256];

    stream_client_cfg_t cfg = make_cfg(10);
    fsm_event_t ev = stream_client_run(&cfg, &t, &s, scratch, sizeof(scratch));
    CHECK_EQ_INT(ev, FSM_EV_ERROR);
}

static void test_scratch_too_small(void)
{
    mock_tr_t tr = { .close_after = -1 };
    mock_src_t sr = { .frames_left = 1, .frame_bytes = 8 };
    stream_transport_t t = { .send = m_send, .recv = m_recv, .user = &tr };
    pcm_source_t s = { .read = m_read, .user = &sr };
    uint8_t scratch[4];   /* too small for a frame + overhead */

    stream_client_cfg_t cfg = make_cfg(10);
    fsm_event_t ev = stream_client_run(&cfg, &t, &s, scratch, sizeof(scratch));
    CHECK_EQ_INT(ev, FSM_EV_ERROR);
}

TEST_MAIN_BEGIN("stream_client")
    RUN(test_server_closes_midstream);
    RUN(test_source_exhausts_then_close);
    RUN(test_session_timeout);
    RUN(test_send_failure);
    RUN(test_scratch_too_small);
TEST_MAIN_END()
