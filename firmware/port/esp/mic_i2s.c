#include "mic_i2s.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "mic_i2s";

static i2s_chan_handle_t s_rx;
static int s_gain_shift;

esp_err_t mic_i2s_start(int bclk_gpio, int ws_gpio, int din_gpio, int gain_shift)
{
    if (s_rx) {
        return ESP_OK;
    }
    s_gain_shift = gain_shift;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    /* Generous DMA buffering (~300 ms) so the stream keeps up while the client also
     * polls the socket between frames. */
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 600;
    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(err));
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                        I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)bclk_gpio,
            .ws   = (gpio_num_t)ws_gpio,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)din_gpio,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    /* INMP441 L/R tied low -> data on the left slot. */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(s_rx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init_std_mode: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return err;
    }
    err = i2s_channel_enable(s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "channel_enable: %s", esp_err_to_name(err));
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return err;
    }
    ESP_LOGI(TAG, "started (bclk=%d ws=%d din=%d gain<<%d)",
             bclk_gpio, ws_gpio, din_gpio, gain_shift);
    return ESP_OK;
}

static inline int16_t clamp16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

size_t mic_i2s_read(int16_t *out, size_t max_samples, uint32_t timeout_ms)
{
    if (!s_rx || max_samples == 0) {
        return 0;
    }
    /* Read 32-bit slots and downshift. Chunk through a small stack buffer so we never
     * need a large allocation. */
    int32_t raw[128];
    size_t done = 0;
    while (done < max_samples) {
        size_t want = max_samples - done;
        if (want > 128) {
            want = 128;
        }
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(s_rx, raw, want * sizeof(int32_t),
                                         &bytes_read, timeout_ms);
        if (err != ESP_OK || bytes_read == 0) {
            break;
        }
        size_t got = bytes_read / sizeof(int32_t);
        for (size_t i = 0; i < got; i++) {
            /* 24-bit data sits in the top bits of the 32-bit slot; >>16 yields 16-bit. */
            int32_t s = (raw[i] >> 16) << s_gain_shift;
            out[done + i] = clamp16(s);
        }
        done += got;
        if (got < want) {
            break;
        }
    }
    return done;
}

void mic_i2s_stop(void)
{
    if (s_rx) {
        i2s_channel_disable(s_rx);
        i2s_del_channel(s_rx);
        s_rx = NULL;
    }
}

uint32_t mic_frame_energy(const int16_t *pcm, size_t n)
{
    if (n == 0) {
        return 0;
    }
    uint64_t acc = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t v = pcm[i];
        acc += (uint32_t)(v < 0 ? -v : v);
    }
    return (uint32_t)(acc / n);
}

void mic_source_init(mic_source_t *s, uint32_t energy_thresh,
                     uint32_t hang_frames, uint32_t max_frames)
{
    memset(s, 0, sizeof(*s));
    s->energy_thresh = energy_thresh;
    s->hang_frames = hang_frames;
    s->max_frames = max_frames;
    s->read_timeout_ms = 200;
}

static size_t mic_src_read(void *user, void *buf, size_t cap)
{
    mic_source_t *s = user;
    size_t samples = cap / sizeof(int16_t);
    if (samples == 0) {
        return 0;
    }
    size_t got = mic_i2s_read((int16_t *)buf, samples, s->read_timeout_ms);
    if (got == 0) {
        return 0;   /* read failure ends the stream */
    }

    uint32_t energy = mic_frame_energy((int16_t *)buf, got);
    if (energy >= s->energy_thresh) {
        s->speech_seen = 1;
        s->silence_run = 0;
    } else {
        s->silence_run++;
    }
    s->total_frames++;

    if (s->total_frames >= s->max_frames) {
        return 0;   /* hard cap: stop streaming */
    }
    if (s->speech_seen && s->silence_run >= s->hang_frames) {
        return 0;   /* end of speech: trailing silence observed */
    }
    return got * sizeof(int16_t);
}

pcm_source_t mic_pcm_source(mic_source_t *s)
{
    pcm_source_t src = { .read = mic_src_read, .user = s };
    return src;
}
