# Hive Controller — Technical Design Document

## Overview
The Hive Controller is split into two boards per hive:
- **This board (XIAO ESP32C3)**: slow sensors — BME280 (temp/humidity/pressure) + HX711 (scale). Deep sleep between readings.
- **Audio node (XIAO ESP32S3)**: fast sensor — INMP441 microphone. Needs active sampling windows.

Both boards transmit independently via ESP-NOW to the bridge.

Prototyping: separate 18650 battery per board.
Production: single battery + shared charge controller powering both boards.

## Hardware

### Core Board: Seeed Studio XIAO ESP32C3
- CPU: RISC-V 160MHz, single core
- Flash: 4MB
- RAM: 400KB (8KB RTC memory survives deep sleep)
- WiFi: 2.4GHz 802.11 b/g/n
- BLE: Bluetooth 5.0
- Deep sleep current: ~5µA (board overhead brings this higher)
- Operating voltage: 3.3V (regulator accepts 5V on VIN or USB)
- Battery pads: direct to TP4056 charge IC, supports 1x 3.7V Li-ion/LiPo

### Pin Assignments
| Pin | Function | Notes |
|-----|----------|-------|
| D4 (SDA) | I2C SDA | BME280 |
| D5 (SCL) | I2C SCL | BME280 |
| A0 (GPIO2) | Battery ADC | External voltage divider |
| D3 | Reserved | Future: reed switch for admin mode |

### Sensors

#### BME280 (Temperature, Humidity, Pressure)
- Interface: I2C at address 0x76
- Library: `Adafruit_BME280`
- Placement: inside the inner cover of the hive
- Failure behavior: non-fatal, controller sends packet without BME280 data

#### Battery Voltage (External Voltage Divider)
- **XIAO ESP32C3 does NOT have a built-in battery ADC** — the battery pads go straight to the charge IC with no divider to any GPIO
- Solution: external voltage divider with 2x 100kΩ resistors
- Wiring: BAT+ → 100kΩ → A0 → 100kΩ → GND
- Divides battery voltage by 2 (~2.1V at ADC for a full 4.2V cell)
- Current draw: ~20µA (negligible)
- Calibration: `voltage = analogReadMilliVolts(A0) * 2.0 / 1000.0`
- Percentage: linear approximation, 4.2V=100%, 3.0V=0%

#### USB Power Detection
- **Not feasible** without extra hardware — the charge IC doesn't expose VBUS to any GPIO
- Considered and removed from the design

#### Future Sensors (not yet integrated)
- **HX711 + 4x 50kg load cells** (Phase 1): hive weight/scale under the stand
- **INMP441 I2S MEMS mic** (Phase 2): acoustics inside the frame space

### Power
- Source: 1x 18650 Li-ion (3.7V nominal, 4.2V full, 3.0V cutoff)
- Charging: via XIAO onboard TP4056 over USB-C
- Budget: deep sleep ~5µA + board quiescent; wake cycle ~120mA for <2 seconds
- At 5-minute intervals, a 3000mAh 18650 should last months

## Communication

### ESP-NOW (Primary — Production)
- Broadcast mode (FF:FF:FF:FF:FF:FF) — any nearby bridge will receive
- No pairing or handshake required
- Packet: binary struct, ~34 bytes
- No delivery confirmation in broadcast mode (unicast would provide this)
- Requires `WiFi.mode(WIFI_STA)` but no actual WiFi connection
- Both sender and receiver must be on the same WiFi channel (default: 1)

### WiFi (Debug/Bench Testing)
- Connects to a configured AP and serves a simple web dashboard
- Stays awake (no deep sleep) — not suitable for battery operation
- Credentials: placeholder values, edit before use

### BLE (Debug/Bench Testing + Future Admin)
- Advertises readable characteristics for temp/hum/pressure
- Stays awake (no deep sleep) — not suitable for battery operation
- Future: writable command characteristic for admin interface

### Radio Mode Selection
- Compile-time toggle: `USE_ESPNOW`, `USE_WIFI`, `USE_BLUETOOTH`
- Exactly one must be enabled (enforced by `#error`)
- Future: admin mode could temporarily enable BLE alongside ESP-NOW

## Data Packet

### HivePacket Struct (ESP-NOW)
```cpp
typedef struct __attribute__((packed)) {
  char sensor_id[20];
  float temperature;
  float humidity;
  float pressure;
  float battery_voltage;
  uint8_t battery_pct;
  uint8_t sensor_status; // bitmask
} HivePacket;
```

### Sensor Status Bitmask
| Bit | Mask | Sensor |
|-----|------|--------|
| 0 | 0x01 | BME280 |
| 1 | 0x02 | HX711 scale (Phase 1) |
| 2 | 0x04 | INMP441 mic (Phase 2) |

When a sensor is offline, its data fields are zeroed and the corresponding status bit is cleared. The bridge only includes data fields in the JSON when the sensor is online.

## Sleep & Wake Cycle

### Current Behavior
```
Boot → Init sensors → Read → ESP-NOW send → Wait for callback (2s timeout) → Deep sleep
```
Deep sleep resets the CPU — `loop()` is never reached in ESP-NOW mode.

### Planned Sleep Modes
Configurable via NVS (`Preferences` library), switchable at runtime via BLE admin.

| Mode | Behavior | Use Case |
|------|----------|----------|
| Rapid | Every 30s | Debugging, battery rundown tests |
| Battery Saver | Every 5min | Normal production |
| Hybrid | 60 readings @ 1s, then 5min sleep, repeat | Detailed monitoring bursts |

- Mode selection stored in **NVS** (survives power loss)
- Burst counter stored in **RTC memory** (survives deep sleep, resets on power loss)

## Admin Mode

### Trigger Mechanisms
- **Phase 1 (no hardware):** Triple-reset within 10 seconds detected via RTC memory counter
- **Phase 2 (enclosure design):** Magnetic reed switch on a GPIO, triggered by holding a magnet to marked spot on enclosure. Fully sealed, waterproof.

### Reed Switch + Status LED (Phase 2)
A reed switch and LED together provide a zero-drain diagnostic interface:
- **Short magnet hold**: LED flashes a diagnostic code, then system resumes normal operation
- **Long magnet hold (3+ seconds)**: enters full BLE admin mode

#### Status LED Flash Codes
| Flash Sequence | Meaning |
|---------------|---------|
| 1 flash | System OK |
| 2 flashes | ESP-NOW send failed last cycle |
| 3 flashes | Sensor(s) offline |
| Long blink | Low battery |
| Rapid flashing | Cached data waiting (connectivity lost) |

#### Power Impact
Zero during normal operation — LED only draws current when magnet is physically held to the enclosure. Status from the last wake cycle is stored in RTC memory so it's available instantly when the reed switch activates.

#### Hardware
- Reed switch: GPIO D3 (input, internal pullup, active LOW when magnet present)
- Status LED: GPIO D3 shared or separate pin + 330Ω resistor
- Both fit inside the sealed enclosure, no holes needed

### Admin Mode Behavior
- Starts BLE advertising with admin service
- **5-minute auto-timeout** → returns to normal sleep cycle (battery safety net)
- Skips deep sleep while admin session is active

### BLE Admin Interface
- CMD characteristic (writable): accepts commands like `mode:rapid`, `mode:saver`, `status`, `send:test`
- RESP characteristic (notifiable): returns JSON responses to the connected client
- Any BLE scanner app (nRF Connect, LightBlue) works as a basic admin tool — no custom app needed initially

## Local Data Caching

### Problem
ESP-NOW broadcast has no delivery confirmation. If the bridge is offline, data is lost.

### Planned Solution
- Use **LittleFS** on the ESP32C3's 4MB flash for a message queue
- Always write readings to local cache before sending
- On successful send (requires switching to **unicast ESP-NOW** for confirmation), mark as sent
- Alternative: always cache, let the C&C deduplicate by timestamp
- Flash handles ~100K write cycles per sector; LittleFS does wear leveling

## Local Diagnostics Counters

### Planned — stored in NVS
| Counter | Description | Reset |
|---------|-------------|-------|
| `total_sends` | Lifetime successful ESP-NOW sends | Never (or manual via admin) |
| `total_failures` | Lifetime failed sends | Never |
| `sends_since_charge` | Sends since last battery charge | Auto-reset when voltage transitions from rising→falling (charge→discharge) |
| `sensor_errors` | Count of sensor init failures | Never |
| `boot_count` | Total wake cycles | Never |

- All stored in **NVS** (survives power loss)
- Readable via the BLE admin interface or the status LED diagnostic sequence
- Could optionally include counters in the HivePacket for remote monitoring from the C&C

## Flashing Notes
- Deep sleep disconnects USB — board won't appear as a serial device
- To flash: hold **BOOT**, press **RESET**, release **RESET**, wait 1-2s, release **BOOT**
- Alternative timing: hold BOOT, click Upload in Arduino IDE, release BOOT when "Connecting..." appears
- Battery may need to be disconnected if flashing fails repeatedly
- 3-second delay at start of `setup()` to allow serial monitor connection for debugging

## ESP-IDF API Notes
- The XIAO ESP32C3 board package uses ESP-IDF v5.x with newer callback signatures
- `#include <WiFi.h>` and `#include <esp_now.h>` must be at the **top level** of the .ino file, not inside `#if` blocks — Arduino preprocessor collects function declarations before processing conditionals
- Send callback signature: `void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)`
- If compilation fails with "does not name a type" errors, check include ordering
