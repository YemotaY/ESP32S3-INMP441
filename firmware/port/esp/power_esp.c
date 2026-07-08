#include "power_esp.h"

#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/rtc_io.h"

static const char *TAG = "power_esp";

static wake_cause_t esp_wake_cause(power_ctrl_t *pc)
{
    (void)pc;
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT1: {
        uint64_t mask = esp_sleep_get_ext1_wakeup_status();
#if POWER_ESP_BUTTON_WAKE_GPIO >= 0
        if (mask & (1ULL << POWER_ESP_BUTTON_WAKE_GPIO)) {
            return WAKE_CAUSE_BUTTON;
        }
#endif
        if (mask & (1ULL << POWER_ESP_SOUND_WAKE_GPIO)) {
            return WAKE_CAUSE_SOUND_TRIGGER;
        }
        return WAKE_CAUSE_SOUND_TRIGGER;
    }
    case ESP_SLEEP_WAKEUP_TIMER:
        return WAKE_CAUSE_TIMER;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        /* Power-on reset / first boot. */
        return WAKE_CAUSE_POWER_ON;
    }
}

static void esp_enter_deep_sleep(power_ctrl_t *pc)
{
    (void)pc;

    uint64_t mask = (1ULL << POWER_ESP_SOUND_WAKE_GPIO);
#if POWER_ESP_BUTTON_WAKE_GPIO >= 0
    mask |= (1ULL << POWER_ESP_BUTTON_WAKE_GPIO);
#endif

    /* Wake when any selected RTC pin goes high. */
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_HIGH);

    ESP_LOGI(TAG, "entering deep sleep (ext1 mask=0x%llx)", (unsigned long long)mask);
    esp_deep_sleep_start(); /* does not return */
}

power_backend_t power_esp_backend(void)
{
    power_backend_t be = {
        .enter_deep_sleep = esp_enter_deep_sleep,
        .wake_cause = esp_wake_cause,
        .ctx = NULL,
    };
    return be;
}
