# Beeyard Intelligence System

## Project Overview
Modular IoT pipeline for apiary monitoring. Full scope in `input-req.md`.

## Current Phase: 0 — Hive Controller Telemetry Baseline

### Architecture (working)
```
XIAO ESP32C3 (Hive Controller)
  --[ESP-NOW broadcast]-->
ESP32-WROOM-32 (Bridge)
  --[UART 9600 baud, GPIO17 TX2 → Pi Pin 10 RXD]-->
Pi Zero W (Yard C&C)
  --> ~/beeyard_logs/YYYY-MM-DD.jsonl
```

### Hardware Notes

#### XIAO ESP32C3 (Hive Controller)
- I2C: SDA=D4, SCL=D5, BME280 at 0x76
- Battery ADC: **not built-in** — requires external voltage divider (2x 100kΩ) on A0
- USB power detection: not feasible without extra hardware (charge IC doesn't expose VBUS)
- Deep sleep: USB serial disconnects during sleep; BOOT+RESET needed to reflash
- Flashing tip: hold BOOT, click Upload, release BOOT when "Connecting..." appears

#### ESP32-WROOM-32 (Bridge)
- UART2 to Pi: GPIO17 (TX2) → Pi Pin 10, GPIO16 (RX2) → Pi Pin 8
- USB Serial (debug): 115200 baud — independent from UART2
- Uses newer ESP-IDF v5 API: `esp_now_recv_info_t*` for receive callback, `wifi_tx_info_t*` for send callback

#### Pi Zero W (Yard C&C)
- UART: `/dev/serial0` → `ttyAMA0` (PL011, reliable)
- Required `/boot/firmware/config.txt` settings: `enable_uart=1`, `dtoverlay=disable-bt`
- Config file location is `/boot/firmware/config.txt` (not `/boot/config.txt`)
- Python dependency: `sudo apt install python3-serial` (pip not available)

### Key Findings
- **UART baud rate**: 9600 baud is reliable over dupont wires between WROOM-32 and Pi Zero W. 115200 caused heavy corruption.
- **Pi 5V GPIO pins**: can power the WROOM-32 via VIN, but check solder joints — cold joints cause intermittent power loss.
- **ESP-NOW API differences**: the C3 and WROOM-32 board packages may use different ESP-IDF versions. Includes for `esp_now.h` and `WiFi.h` must be at the top level (not inside `#if` blocks) due to Arduino preprocessor behavior.
- **Fault tolerance**: system sends whatever data is available. Missing sensors are reported via a bitmask in the packet (`sensor_status` field).

### Data Format (JSON over UART)
```json
{"sensor_id":"HIVE_NODE_01","bme280":"ok","temp":21.0,"hum":61.5,"press":978.0,"batt_v":4.16,"batt_pct":96}
{"sensor_id":"HIVE_NODE_01","bme280":"offline","batt_v":4.16,"batt_pct":96}
```

### Sensor Status Bitmask
- Bit 0 (0x01): BME280
- Bit 1 (0x02): reserved for HX711 scale (Phase 1)
- Bit 2 (0x04): reserved for INMP441 mic (Phase 2)

### Development Phases
- [x] Phase 0: Hive Controller telemetry baseline (T&H + battery + fault tolerance)
- [ ] Phase 1: HX711 scale integration
- [ ] Phase 2: INMP441 acoustic integration
- [ ] Phase 3: ESP32-CAM bee counter
- [ ] Phase 4: Yard C&C + WS-90 weather intercept
- [ ] Phase 5: Solar relay + cloud pipeline
