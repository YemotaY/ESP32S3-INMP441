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

## Hardware hooks (implemented)

`app_main.c` now wires the real device backends into the tested portable core:

- `hook_run_kws` — captures one wake window from the INMP441 and runs the **trained int8
  DS-CNN** over its log-mel features (`core/kws_frontend`), confirming only when the model
  reports the wake class above its confidence threshold. A cheap energy floor
  (`SIMONSAYS_WAKE_ENERGY`) rejects silence before the model runs. The model weights live
  in `main/kws_model_data.h`, codegen'd from the training framework (see below).
- `hook_connect` — `ss_wifi_connect()` (STA) then `ss_tcp_connect()` opens the stream
  socket.
- `hook_stream` — runs the portable `stream_client_run()` over `ss_tcp_transport()` +
  `mic_pcm_source()`, streaming chunked-HTTP PCM until the server cuts the connection.

Port backends live in `port/esp/`: `mic_i2s` (INMP441 I2S RX), `net_wifi` (STA bring-up),
`net_socket` (lwip TCP transport). All are `ss_`-prefixed to avoid clashing with IDF/lwip
symbols (`tcp_connect`, `wifi_sta_disconnect`, …).

> The log-mel front-end (512-pt FFT) plus DS-CNN run in the main task, so
> `sdkconfig.defaults` raises `CONFIG_ESP_MAIN_TASK_STACK_SIZE` to 16 KB.

### Wake-word model (trained DS-CNN)

The wake model is trained and quantized off-device by the framework in `kws-framework/`,
then codegen'd to a C header consumed verbatim by the firmware and the host parity test:

```sh
python kws-framework/tools/train.py --epochs 80 --out firmware/main/kws_model_data.h
```

`main/kws_model_data.h` defines `kws_model_get()` (a `kws_model_t`). The demo model ships
trained on a synthetic separable dataset — retrain on real recorded wake-word features
(same `(X, y)` interface) for production. The C engine reproduces the Python reference
bit-for-bit (`tests/host/test_parity.c`), and `tests/host/test_kws_frontend.c` exercises
the full PCM→log-mel→int8→decision path.


### I2S microphone wiring (INMP441)

| INMP441 | ESP32-S3 GPIO | Kconfig |
|---|---|---|
| SCK / BCLK | **GPIO4** | `SIMONSAYS_I2S_BCLK_GPIO` |
| WS / LRCL  | **GPIO5** | `SIMONSAYS_I2S_WS_GPIO` |
| SD / DOUT  | **GPIO6** | `SIMONSAYS_I2S_DIN_GPIO` |
| L/R | GND (left slot) | — |
| VDD | 3V3 | — |
| GND | GND | — |

16 kHz / 16-bit / mono. `SIMONSAYS_MIC_GAIN_SHIFT` (default 2) boosts the quiet mic.

## Configuration

Set Wi-Fi credentials, the server address, the mic pins, and VAD thresholds via
`idf.py -C firmware menuconfig` → **SimonSays configuration** (see
`main/Kconfig.projbuild`). At minimum set `SIMONSAYS_WIFI_SSID`,
`SIMONSAYS_WIFI_PASSWORD`, and `SIMONSAYS_SERVER_HOST`.

## Flash & run

```sh
. ../esp/esp-idf/export.sh
idf.py -C firmware -p /dev/ttyACM0 flash monitor   # port varies
```

Start the server on the host first: `./build/server/simonsays-server --port 8080`.
