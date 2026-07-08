/* ESP32-S3 power backend: maps the portable power abstraction onto ESP-IDF
 * deep sleep and wake-source detection.
 *
 * Analog sound trigger -> EXT1 wake (see docs/ARCHITECTURE.md §2.2). The INMP441
 * (digital I2S) cannot wake from deep sleep, so a separate analog sensor drives an
 * RTC-capable GPIO. A push button can share EXT1 as a manual wake.
 */
#ifndef POWER_ESP_H
#define POWER_ESP_H

#include "core/power.h"

/* GPIO (RTC-capable) driven high by the analog sound trigger for EXT1 wake.
 * Override via build flags; must be an RTC IO on the ESP32-S3 (GPIO0..21). */
#ifndef POWER_ESP_SOUND_WAKE_GPIO
#define POWER_ESP_SOUND_WAKE_GPIO 2
#endif

/* Optional manual-wake button GPIO (RTC-capable), also EXT1. Set to -1 to disable. */
#ifndef POWER_ESP_BUTTON_WAKE_GPIO
#define POWER_ESP_BUTTON_WAKE_GPIO -1
#endif

/* Build a power backend bound to the ESP-IDF deep-sleep implementation. */
power_backend_t power_esp_backend(void);

#endif /* POWER_ESP_H */
