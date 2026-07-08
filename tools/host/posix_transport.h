/* Host helpers to drive the portable stream_client against a real TCP server:
 * a blocking-send / non-blocking-recv POSIX socket transport and an in-memory PCM source.
 * Shared by the simonsays-client demo and the e2e loopback test. */
#ifndef POSIX_TRANSPORT_H
#define POSIX_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "core/net/stream_client.h"

/* Connect to host:port over TCP. Returns a socket fd, or -1 on error. */
int posix_connect(const char *host_ip, int port);

typedef struct {
    int fd;
} posix_transport_ctx_t;

/* Wrap a connected socket as a stream transport (recv is non-blocking via MSG_DONTWAIT). */
stream_transport_t posix_transport(posix_transport_ctx_t *ctx);

/* In-memory PCM source handing out bytes in frame_bytes-sized reads, then 0 (end). */
typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         pos;
} pcm_buf_src_t;

pcm_source_t pcm_buffer_source(pcm_buf_src_t *s);

#endif /* POSIX_TRANSPORT_H */
