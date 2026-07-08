/* End-to-end loopback test: the portable stream_client streams synthesized PCM over a
 * real TCP socket to an in-process server built from serverlib, exactly as the ESP32 and
 * the deployed server would interact. Proves the full loop wake->stream->cut->sleep:
 * the client reports SERVER_CLOSED because the server's debounce logic cut the connection
 * after recognising a command. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "core/net/stream_client.h"
#include "server/session.h"
#include "server/http_ingest.h"
#include "posix_transport.h"
#include "test.h"

static uint32_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

typedef struct {
    int          listen_fd;
    cut_reason_t reason;
    int          commands;
    int          last_intent;
} srv_result_t;

typedef struct {
    session_t *sess;
    uint32_t   now;
} ing_ctx_t;

static void on_payload(void *user, const uint8_t *data, size_t len)
{
    ing_ctx_t *c = user;
    session_on_pcm(c->sess, data, len, c->now);
}

static const intent_entry_t entries[] = {
    { 0, "light_on", "light on" },
};
static const intent_table_t table = { .entries = entries, .count = 1 };

static void *server_thread(void *arg)
{
    srv_result_t *r = arg;
    int fd = accept(r->listen_fd, NULL, NULL);
    if (fd < 0) {
        return NULL;
    }
    struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    static const char *scripts[] = { "light on" };
    stt_stub_t stub;
    stt_stub_init(&stub, scripts, 1, 500, 400, 200);

    session_cfg_t cfg = {
        .intents = &table,
        .vad = { .energy_thresh = 500 },
        .debounce = { .debounce_ms = 300, .max_session_ms = 10000, .silence_ms = 2000 },
    };
    session_t sess;
    session_init(&sess, &cfg, stt_stub_backend(&stub), monotonic_ms());

    ing_ctx_t ctx = { .sess = &sess, .now = monotonic_ms() };
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &ctx);

    uint8_t buf[2048];
    for (;;) {
        ctx.now = monotonic_ms();
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            if (http_ingest_feed(&ing, buf, (size_t)n) < 0) {
                break;
            }
        } else if (n == 0) {
            break;
        }
        if (session_tick(&sess, monotonic_ms())) {
            const char *body = "{\"cut\":\"ok\"}";
            char hdr[128];
            int hl = snprintf(hdr, sizeof(hdr),
                              "HTTP/1.1 200 OK\r\nConnection: close\r\n"
                              "Content-Length: %zu\r\n\r\n", strlen(body));
            (void)!write(fd, hdr, (size_t)hl);
            (void)!write(fd, body, strlen(body));
            break;
        }
    }
    r->reason = session_cut_reason(&sess);
    r->commands = sess.commands;
    r->last_intent = sess.last_intent;
    close(fd);
    return NULL;
}

static void test_e2e_loop(void)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(srv >= 0);
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;   /* ephemeral */
    CHECK(bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(listen(srv, 1) == 0);

    socklen_t alen = sizeof(addr);
    getsockname(srv, (struct sockaddr *)&addr, &alen);
    int port = ntohs(addr.sin_port);

    srv_result_t result = { .listen_fd = srv, .reason = CUT_NONE, .commands = 0 };
    pthread_t th;
    CHECK(pthread_create(&th, NULL, server_thread, &result) == 0);

    int fd = posix_connect("127.0.0.1", port);
    CHECK(fd >= 0);

    /* Synthesize one spoken command: speech burst + trailing silence. */
    static uint8_t pcm[8000];
    size_t len = 0;
    for (int i = 0; i < 1000; i++) {           /* speech */
        uint16_t u = 5000;
        pcm[len++] = (uint8_t)(u & 0xFF);
        pcm[len++] = (uint8_t)(u >> 8);
    }
    for (int i = 0; i < 600; i++) {            /* trailing silence ends the utterance */
        pcm[len++] = 0;
        pcm[len++] = 0;
    }

    posix_transport_ctx_t tctx = { .fd = fd };
    stream_transport_t tr = posix_transport(&tctx);
    pcm_buf_src_t bsrc = { .data = pcm, .len = len, .pos = 0 };
    pcm_source_t src = pcm_buffer_source(&bsrc);

    stream_client_cfg_t cfg = {
        .proto = { .path = "/v1/stream", .host = "127.0.0.1",
                   .session_id = "e2e", .wake_conf_milli = 990 },
        .frame_bytes = 640,
        .max_chunks = 4000,
    };
    uint8_t scratch[1600];
    fsm_event_t ev = stream_client_run(&cfg, &tr, &src, scratch, sizeof(scratch));

    CHECK_EQ_INT(ev, FSM_EV_SERVER_CLOSED);
    close(fd);

    pthread_join(th, NULL);
    CHECK_EQ_INT(result.commands, 1);
    CHECK_EQ_INT(result.last_intent, 0);
    CHECK_EQ_INT(result.reason, CUT_DEBOUNCE);
    close(srv);
}

TEST_MAIN_BEGIN("server_e2e")
    RUN(test_e2e_loop);
TEST_MAIN_END()
