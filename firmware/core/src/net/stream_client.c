#include "core/net/stream_client.h"

#include <string.h>

/* Send all `len` bytes; returns 1 on success, 0 on transport error. */
static int send_all(const stream_transport_t *tr, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    size_t sent = 0;
    while (sent < len) {
        long n = tr->send(tr->user, p + sent, len - sent);
        if (n < 0) {
            return 0;
        }
        sent += (size_t)n;
    }
    return 1;
}

/* Poll the transport for a server-close signal without blocking on data.
 * Returns: 1 = server closed, 0 = still open, -1 = transport error. */
static int poll_close(const stream_transport_t *tr)
{
    uint8_t tmp[64];
    long n = tr->recv(tr->user, tmp, sizeof(tmp));
    if (n == 0) {
        return 1;               /* peer closed -> server cut the connection */
    }
    if (n == STREAM_WOULDBLOCK || n > 0) {
        return 0;               /* no data or ignorable control bytes: keep going */
    }
    return -1;                  /* real error */
}

fsm_event_t stream_client_run(const stream_client_cfg_t *cfg,
                              const stream_transport_t *tr,
                              const pcm_source_t *src,
                              uint8_t *scratch, size_t scratch_len)
{
    if (!cfg || !tr || !src || !scratch || cfg->frame_bytes == 0) {
        return FSM_EV_ERROR;
    }
    size_t need = cfg->frame_bytes + stream_proto_chunk_overhead(cfg->frame_bytes);
    if (scratch_len < need) {
        return FSM_EV_ERROR;
    }

    /* Send the request header. */
    size_t hlen = stream_proto_build_request_header((char *)scratch, scratch_len,
                                                    &cfg->proto);
    if (hlen == 0 || !send_all(tr, scratch, hlen)) {
        return FSM_EV_ERROR;
    }

    /* PCM read buffer sits at the tail of scratch; chunk encoding uses the head. */
    uint8_t *pcm = scratch + (scratch_len - cfg->frame_bytes);

    for (uint32_t chunk = 0; chunk < cfg->max_chunks; chunk++) {
        int closed = poll_close(tr);
        if (closed < 0) {
            return FSM_EV_ERROR;
        }
        if (closed) {
            return FSM_EV_SERVER_CLOSED;
        }

        size_t n = src->read(src->user, pcm, cfg->frame_bytes);
        if (n == 0) {
            /* End of speech: finalise the stream and wait for the server to cut. */
            size_t flen = stream_proto_encode_final((char *)scratch, scratch_len);
            if (flen == 0 || !send_all(tr, scratch, flen)) {
                return FSM_EV_ERROR;
            }
            for (uint32_t i = 0; i < cfg->max_chunks; i++) {
                int c = poll_close(tr);
                if (c < 0) {
                    return FSM_EV_ERROR;
                }
                if (c) {
                    return FSM_EV_SERVER_CLOSED;
                }
            }
            return FSM_EV_SESSION_TIMEOUT;
        }

        size_t clen = stream_proto_encode_chunk((char *)scratch, scratch_len - cfg->frame_bytes,
                                               pcm, n);
        if (clen == 0 || !send_all(tr, scratch, clen)) {
            return FSM_EV_ERROR;
        }
    }

    /* Session budget exhausted: finalise politely and report the timeout. */
    size_t flen = stream_proto_encode_final((char *)scratch, scratch_len);
    if (flen > 0) {
        (void)send_all(tr, scratch, flen);
    }
    return FSM_EV_SESSION_TIMEOUT;
}
