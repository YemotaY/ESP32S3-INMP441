/* Portable streaming-client session.
 *
 * Orchestrates one wake->stream->cut exchange over an injected transport (send/recv) and
 * an injected PCM source, using the stream_proto framing. The device wires a real socket
 * transport + I2S source; host tests wire mocks; the e2e loopback test wires a real POSIX
 * socket against the server. Returns the FSM event the session runner expects, so it drops
 * straight into an app `stream` hook.
 */
#ifndef CORE_NET_STREAM_CLIENT_H
#define CORE_NET_STREAM_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "core/fsm.h"
#include "core/net/stream_proto.h"

/* recv() returns this when no data is currently available (keep streaming). */
#define STREAM_WOULDBLOCK (-2L)

/* Byte transport. send() must transmit all bytes or return <0 on error.
 * recv() returns: >0 bytes read, 0 = peer closed (server cut), STREAM_WOULDBLOCK = no
 * data yet, <0 = error. */
typedef struct {
    long (*send)(void *user, const void *buf, size_t len);
    long (*recv)(void *user, void *buf, size_t len);
    void *user;
} stream_transport_t;

/* PCM source. read() fills up to `cap` bytes and returns the count; returning 0 signals
 * end-of-speech (the client then finalises the stream). */
typedef struct {
    size_t (*read)(void *user, void *buf, size_t cap);
    void *user;
} pcm_source_t;

typedef struct {
    stream_proto_cfg_t proto;
    size_t   frame_bytes;   /* PCM read granularity per chunk (e.g. 640 = 20ms@16k/16) */
    uint32_t max_chunks;    /* session guard: cap chunks before forcing SESSION_TIMEOUT */
} stream_client_cfg_t;

/* Run the streaming session. `scratch` must hold at least the request header and one
 * encoded chunk (frame_bytes + stream_proto_chunk_overhead(frame_bytes)).
 * Returns FSM_EV_SERVER_CLOSED (normal cut), FSM_EV_SESSION_TIMEOUT, or FSM_EV_ERROR. */
fsm_event_t stream_client_run(const stream_client_cfg_t *cfg,
                              const stream_transport_t *tr,
                              const pcm_source_t *src,
                              uint8_t *scratch, size_t scratch_len);

#endif /* CORE_NET_STREAM_CLIENT_H */
