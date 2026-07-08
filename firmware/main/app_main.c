/* SimonSays ESP32-S3 firmware entry point.
 *
 * Phase 1 skeleton: wires the tested portable core (FSM + session runner) to the
 * ESP-IDF power backend. The audio/KWS/Wi-Fi/HTTP steps are stubbed hooks that log
 * and return placeholder results so the wake -> ... -> sleep control flow can be
 * exercised on-device before the hardware-specific code lands.
 *
 * Power mode starts at STAY_AWAKE ("at beginning without deep sleep"); switch to
 * DRYRUN to simulate the sleep cycle, or REAL for production deep sleep.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "core/app.h"
#include "core/power.h"
#include "power_esp.h"

static const char *TAG = "simonsays";

/* Select the active power mode here during bring-up. */
#ifndef APP_POWER_MODE
#define APP_POWER_MODE POWER_MODE_STAY_AWAKE
#endif

/* --- Stub hooks (replaced in later phases by real KWS / Wi-Fi / HTTP) --- */

static fsm_event_t hook_run_kws(void *user)
{
    (void)user;
    /* TODO Phase 2/3: run log-mel front-end + custom DS-CNN and decide. */
    ESP_LOGI(TAG, "KWS: (stub) confirming wake word");
    return FSM_EV_KWS_CONFIRMED;
}

static fsm_event_t hook_connect(void *user)
{
    (void)user;
    /* TODO Phase 4: bring up Wi-Fi (restore channel/BSSID from RTC) + open stream. */
    ESP_LOGI(TAG, "CONNECT: (stub) connected");
    return FSM_EV_CONNECTED;
}

static fsm_event_t hook_stream(void *user)
{
    (void)user;
    /* TODO Phase 4: stream PCM until the server closes the connection. */
    ESP_LOGI(TAG, "STREAM: (stub) streaming, server closed");
    vTaskDelay(pdMS_TO_TICKS(500));
    return FSM_EV_SERVER_CLOSED;
}

void app_main(void)
{
    ESP_LOGI(TAG, "boot, power mode=%s", power_mode_str(APP_POWER_MODE));

    power_ctrl_t power;
    power_backend_t backend = power_esp_backend();
    power_init(&power, APP_POWER_MODE, &backend);

    app_hooks_t hooks = {
        .run_kws = hook_run_kws,
        .connect = hook_connect,
        .stream = hook_stream,
        .user = NULL,
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
