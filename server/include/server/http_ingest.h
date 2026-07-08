/* Incremental HTTP/1.1 chunked-POST ingest parser (server side).
 *
 * Push bytes from the socket as they arrive; the parser extracts the request line, the
 * SimonSays headers, and the decoded PCM payload of each transfer chunk, invoking a
 * callback per payload span. It is the counterpart to firmware core/net/stream_proto and
 * shares no state with sockets, so it is fully host-testable (round-trip in
 * tests/host/test_http_ingest.c).
 */
#ifndef SERVER_HTTP_INGEST_H
#define SERVER_HTTP_INGEST_H

#include <stddef.h>
#include <stdint.h>

#define HTTP_INGEST_LINE_MAX 512
#define HTTP_INGEST_PATH_MAX 128
#define HTTP_INGEST_SESSION_MAX 64

/* Called for each decoded payload span (raw PCM). May be called many times per chunk. */
typedef void (*http_ingest_payload_cb)(void *user, const uint8_t *data, size_t len);
/* Called once per parsed header (name/value are NUL-terminated, valid for the call only). */
typedef void (*http_ingest_header_cb)(void *user, const char *name, const char *value);

typedef struct {
    int      state;
    char     line[HTTP_INGEST_LINE_MAX];
    size_t   line_len;
    size_t   chunk_remaining;

    char     method[8];
    char     path[HTTP_INGEST_PATH_MAX];
    char     session_id[HTTP_INGEST_SESSION_MAX];
    uint32_t wake_conf_milli;

    int      complete;   /* set when the terminating 0-chunk is parsed */
    int      error;

    http_ingest_payload_cb on_payload;
    http_ingest_header_cb  on_header;
    void    *user;
} http_ingest_t;

void http_ingest_init(http_ingest_t *ing,
                      http_ingest_payload_cb on_payload,
                      http_ingest_header_cb on_header,
                      void *user);

/* Feed `len` bytes. Returns bytes consumed (== len unless an error occurred), or -1 on
 * a protocol error. Check `ing->complete` for end-of-body. Safe to call repeatedly. */
long http_ingest_feed(http_ingest_t *ing, const uint8_t *buf, size_t len);

#endif /* SERVER_HTTP_INGEST_H */
