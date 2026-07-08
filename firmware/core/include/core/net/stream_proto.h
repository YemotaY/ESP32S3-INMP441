/* Portable chunked-HTTP streaming protocol codec (client side).
 *
 * The device streams PCM to the server as an HTTP/1.1 `Transfer-Encoding: chunked`
 * POST body. This module builds the request header, encodes PCM buffers as chunks, and
 * emits the terminating chunk -- pure functions over caller-provided buffers, no malloc,
 * no sockets. The server-side counterpart is server/http_ingest, and the round trip is
 * exercised host-side in tests/host/test_stream_proto.c and the e2e loopback test.
 *
 * Wire format (per RFC 7230 §4.1):
 *   POST <path> HTTP/1.1\r\n
 *   Host: <host>\r\n
 *   Transfer-Encoding: chunked\r\n
 *   Content-Type: application/octet-stream\r\n
 *   X-SimonSays-Session: <session_id>\r\n
 *   X-SimonSays-Wake-Conf: <wake_conf_milli>\r\n
 *   \r\n
 *   <hexlen>\r\n<payload bytes>\r\n   (repeated)
 *   0\r\n\r\n                          (terminator)
 */
#ifndef CORE_NET_STREAM_PROTO_H
#define CORE_NET_STREAM_PROTO_H

#include <stddef.h>
#include <stdint.h>

/* Streaming request configuration. Strings must outlive the header build call. */
typedef struct {
    const char *path;             /* e.g. "/v1/stream" */
    const char *host;             /* e.g. "192.168.1.10:8080" */
    const char *session_id;       /* opaque session identifier */
    uint32_t    wake_conf_milli;  /* wake-word confidence * 1000 (for the dashboard) */
} stream_proto_cfg_t;

/* Build the HTTP request header into `buf` (not NUL-terminated in the count).
 * Returns the number of bytes written, or 0 if the buffer is too small. */
size_t stream_proto_build_request_header(char *buf, size_t cap,
                                         const stream_proto_cfg_t *cfg);

/* Encode one chunked-transfer chunk wrapping `payload_len` bytes of PCM.
 * Writes "<hexlen>\r\n<payload>\r\n" into `buf`. A zero-length payload is rejected
 * (returns 0) since that would be misread as the terminator.
 * Returns bytes written, or 0 if the buffer is too small / payload_len == 0. */
size_t stream_proto_encode_chunk(char *buf, size_t cap,
                                 const void *payload, size_t payload_len);

/* Number of bytes stream_proto_encode_chunk needs for a given payload length. */
size_t stream_proto_chunk_overhead(size_t payload_len);

/* Emit the terminating chunk "0\r\n\r\n". Returns bytes written, or 0 if too small. */
size_t stream_proto_encode_final(char *buf, size_t cap);

#endif /* CORE_NET_STREAM_PROTO_H */
