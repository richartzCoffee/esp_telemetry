# esp_telemetry
Code to create a telemetry system with ESP32 and MQTT broker.

## Wi-Fi Behavior in the System

The firmware uses ESP32 in `STA` (station) mode, which means it connects to an existing Wi-Fi network.

Implemented flow:
1. In `app_main`, the system initializes NVS (`nvs_flash_init`) and creates the default event loop (`esp_event_loop_create_default`).
2. Then it calls `wifi_connection_init_std()`, which:
   - initializes `esp_netif` and the Wi-Fi driver;
   - applies SSID/password and security settings;
   - registers Wi-Fi event handlers;
   - starts Wi-Fi with `esp_wifi_start()`.
3. When `WIFI_EVENT_STA_START` is received, the system calls `esp_wifi_connect()`.
4. If `WIFI_EVENT_STA_DISCONNECTED` occurs:
   - it retries immediately up to `ESP_MAXIMUM_RETRY`;
   - after reaching the limit, it waits and retries using a timer (`WIFI_RECONNECT_INTERVAL_SEC`).
5. On `WIFI_EVENT_STA_CONNECTED`, the retry counter is reset.

Configurable parameters in `menuconfig`:
- `ESP_WIFI_SSID`: network name.
- `ESP_WIFI_PASSWORD`: network password.
- `ESP_MAXIMUM_RETRY`: maximum number of immediate reconnection attempts.
- `WIFI_RECONNECT_INTERVAL_SEC`: interval (in seconds) before the next retry after reaching the limit.
- `ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD`: minimum accepted authentication mode.
- WPA3 SAE options (`ESP_WIFI_SAE_MODE` and `ESP_WIFI_PW_ID`).

## Commands to Configure and Run

```bash
# (optional) set the target, e.g., ESP32
idf.py set-target esp32

# reconfigure project (CMake)
idf.py reconfigure

# open configuration menu (SSID, password, retries, etc.)
idf.py menuconfig

# build
idf.py build

# flash to device and open serial monitor
idf.py flash monitor
```
