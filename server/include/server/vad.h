/* Energy-based voice-activity detection (frame level).
 *
 * Trivial mean-absolute-amplitude gate: enough to drive debounce "speech seen" notes and
 * to segment utterances in the stub STT. Pure, host-tested in tests/host/test_session.c.
 */
#ifndef SERVER_VAD_H
#define SERVER_VAD_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t energy_thresh;   /* mean |sample| above which a frame counts as speech */
} vad_cfg_t;

/* Mean absolute amplitude of `n` int16 PCM samples. */
uint32_t vad_frame_energy(const int16_t *pcm, size_t n);

/* 1 if the frame's energy exceeds the threshold. */
int vad_is_speech(const vad_cfg_t *cfg, const int16_t *pcm, size_t n);

#endif /* SERVER_VAD_H */
