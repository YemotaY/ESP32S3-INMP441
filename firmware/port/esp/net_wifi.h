/* Minimal Wi-Fi STA bring-up for the streaming session.
 *
 * Connects to a fixed AP (from Kconfig), waiting for an IP with a bounded timeout, and
 * tears the connection down before deep sleep. Kept deliberately small — no reconnect
 * loops or scanning; the daemon connects, streams, and sleeps.
 */
#ifndef NET_WIFI_H
#define NET_WIFI_H

#include <stdint.h>
#include "esp_err.h"

/* Initialise NVS/netif/Wi-Fi once (idempotent). */
esp_err_t ss_wifi_init(void);

/* Connect and block until an IP is acquired or `timeout_ms` elapses. */
esp_err_t ss_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);

/* Disconnect (keeps the driver initialised for the next cycle). */
void ss_wifi_disconnect(void);

#endif /* NET_WIFI_H */
