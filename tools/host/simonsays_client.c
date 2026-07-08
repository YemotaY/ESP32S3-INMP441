/* simonsays-client: stream a WAV to a running simonsays-server exactly as the ESP32
 * would, using the portable stream_client + framing. Prints the FSM event that the
 * device would report (SERVER_CLOSED when the server debounce-cuts the connection).
 *
 * Usage: simonsays-client <wav> [host_ip] [port]
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "core/net/stream_client.h"
#include "core/fsm.h"
#include "posix_transport.h"
#include "wav.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <wav> [host_ip] [port]\n", argv[0]);
        return 2;
    }
    const char *host = argc > 2 ? argv[2] : "127.0.0.1";
    int port = argc > 3 ? atoi(argv[3]) : 8080;

    wav_t wav;
    if (wav_read(argv[1], &wav) != 0) {
        fprintf(stderr, "failed to read WAV: %s\n", argv[1]);
        return 1;
    }
    /* Stream channel 0 as raw little-endian int16 (assumes host is little-endian). */
    size_t nbytes = (size_t)wav.num_frames * sizeof(int16_t);

    int fd = posix_connect(host, port);
    if (fd < 0) {
        fprintf(stderr, "connect failed to %s:%d\n", host, port);
        wav_free(&wav);
        return 1;
    }

    posix_transport_ctx_t tctx = { .fd = fd };
    stream_transport_t tr = posix_transport(&tctx);
    pcm_buf_src_t bsrc = { .data = (const uint8_t *)wav.samples, .len = nbytes, .pos = 0 };
    pcm_source_t src = pcm_buffer_source(&bsrc);

    stream_client_cfg_t cfg = {
        .proto = { .path = "/v1/stream", .host = host,
                   .session_id = "demo", .wake_conf_milli = 950 },
        .frame_bytes = 640,      /* 20 ms @ 16 kHz mono int16 */
        .max_chunks = 4000,
    };
    uint8_t scratch[1600];
    fsm_event_t ev = stream_client_run(&cfg, &tr, &src, scratch, sizeof(scratch));

    printf("stream result: %s\n", fsm_event_str(ev));
    close(fd);
    wav_free(&wav);
    return ev == FSM_EV_SERVER_CLOSED ? 0 : 1;
}
