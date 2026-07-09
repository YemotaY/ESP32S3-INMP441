/* SimonSays ESP32-S3 firmware entry point.
 *
 * Wires the tested portable core (FSM + session runner + streaming client) to the
 * device hardware: INMP441 I2S mic capture, Wi-Fi STA, and a TCP socket transport.
 *
 * Flow (one cycle): wake -> confirm voice (energy gate) -> Wi-Fi + connect ->
 * stream PCM as chunked HTTP until the server debounces a command and cuts -> sleep.
 *
 * Power mode starts at STAY_AWAKE ("at beginning without deep sleep"); switch to
 * DRYRUN to simulate the sleep cycle, or REAL for production deep sleep.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "core/app.h"
#include "core/power.h"
#include "core/net/stream_client.h"
#include "core/kws_frontend.h"
#include "power_esp.h"
#include "mic_i2s.h"
#include "net_wifi.h"
#include "net_socket.h"
#include "kws_model_data.h"

static const char *TAG = "simonsays";

/* Select the active power mode here during bring-up. */
#ifndef APP_POWER_MODE
#define APP_POWER_MODE POWER_MODE_STAY_AWAKE
#endif

/* Frame granularity: 20 ms @ 16 kHz / 16-bit mono = 320 samples = 640 bytes. */
#define FRAME_SAMPLES 320
#define FRAME_BYTES   (FRAME_SAMPLES * 2)
#define FRAMES_PER_SEC (MIC_SAMPLE_RATE / FRAME_SAMPLES)

/* Shared device context passed to the hooks via app_hooks_t.user. */
typedef struct {
    tcp_transport_ctx_t sock;   /* fd of the current session's socket */
    mic_source_t        mic;    /* end-of-speech tracking for the stream */
    kws_frontend_t      kws;    /* trained DS-CNN wake-word front-end */
    uint8_t             scratch[FRAME_BYTES + 64];  /* header/chunk staging */
} app_ctx_t;

/* --- Hook: confirm the wake word ---
 * Captures a short window, runs the trained int8 DS-CNN over its log-mel features, and
 * confirms only when the model reports the wake class above its confidence threshold.
 * A cheap energy floor first rejects silence so the model isn't run on room noise. */
static fsm_event_t hook_run_kws(void *user)
{
    app_ctx_t *ctx = user;
    static int16_t buf[KWS_FE_MAX_FRAMES * KWS_FE_FRAME_STEP + KWS_FE_FRAME_LEN];

    size_t need = kws_frontend_need_samples(&ctx->kws);
    if (need > sizeof(buf) / sizeof(buf[0])) {
        ESP_LOGE(TAG, "KWS: capture buffer too small (%zu)", need);
        return FSM_EV_KWS_REJECTED;
    }
    size_t got = mic_i2s_read(buf, need, 600);
    if (got < need) {
        ESP_LOGW(TAG, "KWS: short mic read (%zu/%zu)", got, need);
        return FSM_EV_KWS_REJECTED;
    }

    uint32_t energy = mic_frame_energy(buf, got);
    if (energy < (uint32_t)CONFIG_SIMONSAYS_WAKE_ENERGY) {
        ESP_LOGI(TAG, "KWS: silence (energy=%lu < %d)", (unsigned long)energy,
                 CONFIG_SIMONSAYS_WAKE_ENERGY);
        return FSM_EV_KWS_REJECTED;
    }

    kws_result_t res;
    if (!kws_frontend_run(&ctx->kws, buf, got, &res)) {
        ESP_LOGW(TAG, "KWS: inference failed");
        return FSM_EV_KWS_REJECTED;
    }
    ESP_LOGI(TAG, "KWS: class=%d conf=%.2f wake=%d (energy=%lu)",
             res.class_id, res.confidence, res.is_wake, (unsigned long)energy);
    return res.is_wake ? FSM_EV_KWS_CONFIRMED : FSM_EV_KWS_REJECTED;
}

/* --- Hook: bring up Wi-Fi and open the TCP socket --- */
static fsm_event_t hook_connect(void *user)
{
    app_ctx_t *ctx = user;
    if (ss_wifi_connect(CONFIG_SIMONSAYS_WIFI_SSID, CONFIG_SIMONSAYS_WIFI_PASSWORD,
                        15000) != ESP_OK) {
        return FSM_EV_CONNECT_FAILED;
    }
    int fd = ss_tcp_connect(CONFIG_SIMONSAYS_SERVER_HOST, CONFIG_SIMONSAYS_SERVER_PORT);
    if (fd < 0) {
        ss_wifi_disconnect();
        return FSM_EV_CONNECT_FAILED;
    }
    ctx->sock.fd = fd;
    ESP_LOGI(TAG, "CONNECT: streaming to %s:%d",
             CONFIG_SIMONSAYS_SERVER_HOST, CONFIG_SIMONSAYS_SERVER_PORT);
    return FSM_EV_CONNECTED;
}

/* --- Hook: stream PCM until the server cuts the connection --- */
static fsm_event_t hook_stream(void *user)
{
    app_ctx_t *ctx = user;

    uint32_t hang = (CONFIG_SIMONSAYS_VAD_HANG_MS * FRAMES_PER_SEC) / 1000;
    uint32_t maxf = (CONFIG_SIMONSAYS_MAX_UTTERANCE_MS * FRAMES_PER_SEC) / 1000;
    mic_source_init(&ctx->mic, CONFIG_SIMONSAYS_VAD_ENERGY, hang ? hang : 1, maxf);

    stream_transport_t tr = ss_tcp_transport(&ctx->sock);
    pcm_source_t src = mic_pcm_source(&ctx->mic);

    stream_client_cfg_t cfg = {
        .proto = {
            .path = CONFIG_SIMONSAYS_STREAM_PATH,
            .host = CONFIG_SIMONSAYS_SERVER_HOST,
            .session_id = CONFIG_SIMONSAYS_SESSION_ID,
            .wake_conf_milli = 990,
        },
        .frame_bytes = FRAME_BYTES,
        .max_chunks = maxf + FRAMES_PER_SEC * 5,   /* stream cap + drain budget */
    };

    fsm_event_t ev = stream_client_run(&cfg, &tr, &src, ctx->scratch, sizeof(ctx->scratch));
    ESP_LOGI(TAG, "STREAM: %s (frames=%lu)", fsm_event_str(ev),
             (unsigned long)ctx->mic.total_frames);

    ss_tcp_close(ctx->sock.fd);
    ctx->sock.fd = -1;
    ss_wifi_disconnect();
    return ev;
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot, power mode=%s", power_mode_str(APP_POWER_MODE));

    ESP_ERROR_CHECK(mic_i2s_start(CONFIG_SIMONSAYS_I2S_BCLK_GPIO,
                                  CONFIG_SIMONSAYS_I2S_WS_GPIO,
                                  CONFIG_SIMONSAYS_I2S_DIN_GPIO,
                                  CONFIG_SIMONSAYS_MIC_GAIN_SHIFT));

    power_ctrl_t power;
    power_backend_t backend = power_esp_backend();
    power_init(&power, APP_POWER_MODE, &backend);

    static app_ctx_t ctx;
    ctx.sock.fd = -1;
    if (!kws_frontend_init(&ctx.kws, kws_model_get(), MIC_SAMPLE_RATE)) {
        ESP_LOGE(TAG, "KWS front-end init failed (model geometry too large)");
        abort();
    }
    ESP_LOGI(TAG, "KWS model ready: %dx%d feats, %d classes, %zu samples/window",
             kws_model_get()->in_h, kws_model_get()->in_w,
             kws_model_get()->num_classes, kws_frontend_need_samples(&ctx.kws));

    app_hooks_t hooks = {
        .run_kws = hook_run_kws,
        .connect = hook_connect,
        .stream = hook_stream,
        .user = &ctx,
    };

    app_t app;
    app_init(&app, &power, &hooks);

    for (;;) {
        wake_cause_t cause = app_run_cycle(&app);
        ESP_LOGI(TAG, "cycle %lu done (wake=%s, sleeps=%lu)",
                 (unsigned long)app.cycles, power_wake_cause_str(cause),
                 (unsigned long)power.sleep_count);

        if (APP_POWER_MODE == POWER_MODE_REAL) {
            /* REAL mode already deep-slept inside the cycle (no return). */
            break;
        }
        /* STAY_AWAKE / DRYRUN: pace the loop instead of powering down. */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
