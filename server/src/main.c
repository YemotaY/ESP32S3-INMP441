/* SimonSays STT/intent/debounce server -- POSIX socket entry point.
 *
 * A deliberately thin accept loop: all protocol/logic lives in the host-tested modules
 * (http_ingest, session, debounce, intent, stt). It reads a chunked-POST audio stream,
 * runs the session pipeline, and closes the socket when the debounce logic decides the
 * command is finished -- which is exactly what makes the ESP32 go back to sleep.
 *
 * STT is stubbed here (no whisper.cpp dependency): pass `--script "light on"` (repeatable)
 * to feed the stub scripted transcripts, emitted per VAD-detected utterance, so the full
 * loop can be demonstrated end-to-end. Swap in a whisper.cpp backend behind stt_backend_t
 * for real recognition.
 *
 * Usage: simonsays-server [--port N] [--script TEXT]...
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "server/config.h"
#include "server/http_ingest.h"
#include "server/session.h"
#include "server/stt.h"

#define MAX_SCRIPTS 16

typedef struct {
    session_t *sess;
    uint32_t   now_ms;
} conn_ctx_t;

static uint32_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static void on_payload(void *user, const uint8_t *data, size_t len)
{
    conn_ctx_t *c = user;
    session_on_pcm(c->sess, data, len, c->now_ms);
}

static void send_cut_response(int fd, const session_t *sess)
{
    char body[256];
    int blen = snprintf(body, sizeof(body),
                        "{\"cut\":\"%s\",\"commands\":%d,\"intent\":\"%s\","
                        "\"transcript\":\"%s\"}",
                        cut_reason_str(session_cut_reason(sess)),
                        sess->commands,
                        sess->last_intent_name ? sess->last_intent_name : "",
                        sess->last_transcript);
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: %d\r\n\r\n", blen);
    (void)!write(fd, hdr, (size_t)hlen);
    (void)!write(fd, body, (size_t)blen);
}

static void handle_conn(int fd, session_cfg_t cfg, stt_backend_t stt)
{
    struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };  /* 100ms tick */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    session_t sess;
    session_init(&sess, &cfg, stt, monotonic_ms());

    conn_ctx_t cctx = { .sess = &sess, .now_ms = monotonic_ms() };
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &cctx);

    uint8_t buf[2048];
    for (;;) {
        cctx.now_ms = monotonic_ms();
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            if (http_ingest_feed(&ing, buf, (size_t)n) < 0) {
                break;
            }
        } else if (n == 0) {
            break;                          /* client closed */
        } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            break;                          /* real error */
        }
        if (session_tick(&sess, monotonic_ms())) {
            send_cut_response(fd, &sess);
            break;
        }
        if (ing.complete) {
            /* Client finished sending; keep ticking until debounce cuts. */
            if (session_tick(&sess, monotonic_ms())) {
                send_cut_response(fd, &sess);
                break;
            }
        }
    }

    printf("session end: reason=%s commands=%d intent=%s transcript=\"%s\"\n",
           cut_reason_str(session_cut_reason(&sess)), sess.commands,
           sess.last_intent_name ? sess.last_intent_name : "-", sess.last_transcript);
    close(fd);
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    int port = 8080;
    const char *scripts[MAX_SCRIPTS];
    size_t n_scripts = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
            if (n_scripts < MAX_SCRIPTS) {
                scripts[n_scripts++] = argv[++i];
            }
        } else {
            fprintf(stderr, "usage: %s [--port N] [--script TEXT]...\n", argv[0]);
            return 2;
        }
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(srv, 4) < 0) {
        perror("listen");
        return 1;
    }
    printf("simonsays-server listening on :%d (%zu scripted transcripts)\n",
           port, n_scripts);

    session_cfg_t cfg = server_default_session_cfg();
    for (;;) {
        int fd = accept(srv, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }
        stt_stub_t stub;
        stt_stub_init(&stub, scripts, n_scripts, 500, 4800, 1600);
        handle_conn(fd, cfg, stt_stub_backend(&stub));
    }
    close(srv);
    return 0;
}
