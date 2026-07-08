/* TCP socket transport for the portable stream_client (device side).
 *
 * Mirrors tools/host/posix_transport: blocking send (all-or-error) and a recv with a
 * short SO_RCVTIMEO so a timeout maps to STREAM_WOULDBLOCK. The streaming loop is paced
 * by the I2S mic read (~one frame per 20 ms), so a small recv timeout adds little
 * latency while still letting the post-speech drain wait for the server's connection cut.
 */
#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include "core/net/stream_client.h"

typedef struct {
    int fd;
} tcp_transport_ctx_t;

/* Connect to host:port. Returns a socket fd (>=0) or -1 on failure. Sets a short
 * receive timeout so recv() reports STREAM_WOULDBLOCK instead of blocking forever. */
int ss_tcp_connect(const char *host, int port);

/* Build a stream_transport_t bound to the connected socket in `ctx`. */
stream_transport_t ss_tcp_transport(tcp_transport_ctx_t *ctx);

void ss_tcp_close(int fd);

#endif /* NET_SOCKET_H */
