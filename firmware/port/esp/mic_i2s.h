/* INMP441 I2S microphone capture (ESP32-S3, new i2s_std driver).
 *
 * The INMP441 is a 24-bit digital MEMS mic on the I2S bus. We run I2S in master RX
 * mode at 16 kHz, mono (left slot, L/R pin tied low), 32-bit slots, and downshift each
 * sample to signed 16-bit PCM — the exact format the streaming client and server expect
 * (16 kHz / 16-bit / mono, little-endian).
 *
 * Default wiring (overridable): BCLK/SCK = GPIO4, WS = GPIO5, DIN/SD = GPIO6.
 */
#ifndef MIC_I2S_H
#define MIC_I2S_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#include "core/net/stream_client.h"

#define MIC_SAMPLE_RATE 16000

/* Bring up the I2S RX channel. `gain_shift` left-shifts each 16-bit sample to boost the
 * fairly quiet INMP441 (0 = raw, 1..4 typical). Safe to call once at boot. */
esp_err_t mic_i2s_start(int bclk_gpio, int ws_gpio, int din_gpio, int gain_shift);

/* Blocking read of up to `max_samples` int16 PCM samples. Returns the count actually
 * read (may be short on timeout). */
size_t mic_i2s_read(int16_t *out, size_t max_samples, uint32_t timeout_ms);

void mic_i2s_stop(void);

/* Mean absolute amplitude of a PCM frame (cheap on-device VAD energy). */
uint32_t mic_frame_energy(const int16_t *pcm, size_t n);

/* End-of-speech tracking state for the streaming pcm_source. */
typedef struct {
    uint32_t energy_thresh;   /* frames with energy >= this count as speech */
    uint32_t hang_frames;     /* trailing silence frames that end the utterance */
    uint32_t max_frames;      /* hard cap on utterance length */
    uint32_t silence_run;     /* consecutive silent frames seen */
    uint32_t total_frames;    /* frames streamed so far */
    int      speech_seen;     /* at least one speech frame observed */
    uint32_t read_timeout_ms; /* per-frame I2S read timeout */
} mic_source_t;

void mic_source_init(mic_source_t *s, uint32_t energy_thresh,
                     uint32_t hang_frames, uint32_t max_frames);

/* Wrap a mic_source_t as the portable stream_client pcm_source_t: each read pulls one
 * frame from I2S and returns 0 (end-of-speech) once the utterance ends. */
pcm_source_t mic_pcm_source(mic_source_t *s);

#endif /* MIC_I2S_H */
