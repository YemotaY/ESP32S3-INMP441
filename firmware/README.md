# SimonSays firmware (ESP32-S3)

Phase 1 skeleton. The application logic lives in the portable **`core`** component
(`firmware/core/`) and is unit-tested on the host (`make test` at the repo root). This
firmware wires that core to ESP-IDF (deep-sleep backend, logging, FreeRTOS loop).

## Layout

```
firmware/
  CMakeLists.txt          # ESP-IDF project (adds core as EXTRA_COMPONENT_DIR)
  partitions.csv          # 4 MB layout: nvs / phy / factory(1.5M) / model(256K) / storage
  sdkconfig.defaults      # no-PSRAM default board, USB-Serial/JTAG console
  sdkconfig.psram         # overlay for PSRAM (...R2) boards
  core/                   # portable, host-tested logic (ringbuf/power/fsm/app)
  port/esp/               # ESP-IDF backends (power_esp: deep sleep + EXT1 wake)
  main/                   # app_main.c entry + stub hooks
```

## Build

```sh
. ../esp/esp-idf/export.sh          # ESP-IDF v6.1-dev vendored in ../esp/esp-idf
idf.py -C firmware set-target esp32s3
idf.py -C firmware build flash monitor
```

PSRAM board:

```sh
idf.py -C firmware -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.psram" build
```

## Power modes (bring-up)

`main/app_main.c` selects the mode via `APP_POWER_MODE`:

- `POWER_MODE_STAY_AWAKE` (default) — never sleeps; loops the wake→…→sleep cycle every
  2 s so you can watch state transitions over the console. **Start here.**
- `POWER_MODE_DRYRUN` — same, but the power backend's sleep path is exercised (still no
  real power-down).
- `POWER_MODE_REAL` — production: `esp_deep_sleep_start()` after each cycle; wakes on the
  analog sound trigger via EXT1.

## Wake wiring (REAL mode)

The analog sound trigger drives an RTC-capable GPIO high on sound
(`POWER_ESP_SOUND_WAKE_GPIO`, default GPIO2). The INMP441 (digital I2S) cannot wake from
deep sleep — see `docs/ARCHITECTURE.md` §2.2. Optional manual button:
`POWER_ESP_BUTTON_WAKE_GPIO`.

## Stubbed in this phase

`hook_run_kws`, `hook_connect`, `hook_stream` are placeholders (see later roadmap phases in
`docs/ARCHITECTURE.md` §11). The I2S/DSP/KWS/Wi-Fi/HTTP code replaces them without touching
the state machine.
