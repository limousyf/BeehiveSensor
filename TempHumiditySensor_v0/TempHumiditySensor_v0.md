# Temperature & Humidity Sensor v0 — Technical Design Document

## Overview
The original Phase 0 prototype for bench testing the BME280 sensor with the XIAO ESP32C3. Supports WiFi (web dashboard) and BLE output modes. This sketch is superseded by the HiveController for production use but remains useful for isolated sensor validation and calibration.

## BOM (Bill of Materials)

| Component | Qty | Description | Approx Cost |
|-----------|-----|-------------|-------------|
| Seeed Studio XIAO ESP32C3 | 1 | RISC-V microcontroller, WiFi + BLE | $5 |
| BME280 breakout (generic) | 1 | I2C temp/humidity/pressure, addr 0x76 | $3 |
| Dupont jumper wires (F-F) | 4 | SDA, SCL, VCC, GND connections | $1 |
| USB-C cable | 1 | Power + programming | $2 |
| Breadboard | 1 | For prototyping | $3 |
| **Total** | | | **~$14** |

### Optional (for sensor cross-validation)
| Component | Qty | Description | Approx Cost |
|-----------|-----|-------------|-------------|
| Adafruit BME280 breakout | 1 | I2C addr 0x77 (Adafruit default) | $15 |
| Adafruit SHT41 breakout | 1 | I2C addr 0x44, high-precision T&H | $10 |

## Pin Assignments

### XIAO ESP32C3
| Pin | Function | Connected To |
|-----|----------|-------------|
| D4 | I2C SDA | BME280 SDA |
| D5 | I2C SCL | BME280 SCL |
| 3V3 | Power | BME280 VCC |
| GND | Ground | BME280 GND |

### BME280 Breakout Wiring
```
XIAO ESP32C3          BME280
─────────────         ──────
3V3  ──────────────── VCC (or VIN)
GND  ──────────────── GND
D4 (SDA) ──────────── SDA
D5 (SCL) ──────────── SCL
```

**I2C Address:** 0x76 (generic boards). Adafruit boards default to 0x77 — check the solder jumper on the breakout.

## Radio Modes

### WiFi Mode (`USE_WIFI = true`)
- Connects to a configured AP
- Serves a styled HTML dashboard on port 80
- Shows sensor ID, temperature, humidity, pressure
- Credentials: edit `ssid` and `password` in the sketch
- Stays awake — not battery-suitable

### BLE Mode (`USE_BLUETOOTH = true`)
- Advertises as "Beeyard_Hive_1"
- Service UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- Characteristics (all read-only):

| Characteristic | UUID | Data |
|---------------|------|------|
| Sensor ID | `11111111-1fb5-459e-8fcc-c5c9c331914b` | "HIVE_NODE_01" |
| Temperature | `beb5483e-36e1-4688-b7f5-ea07361b26a8` | "21.0 C" |
| Humidity | `8d9830da-e7cc-4bf4-be23-57577884bbbb` | "61.5 %" |
| Pressure | `76a3928e-6b80-4573-a444-15f5cc14ba64` | "978.0 hPa" |

- Readable with any BLE scanner app (nRF Connect, LightBlue)
- Stays awake — not battery-suitable

### Mode Guard
- Only one mode can be enabled at a time
- `#if USE_WIFI && USE_BLUETOOTH` triggers a compile error

## Sensor Reading
- Polling interval: 2 seconds
- Temperature: °C with 1 decimal
- Humidity: % with 1 decimal
- Pressure: hPa with 1 decimal (raw Pa / 100)
- All readings printed to serial monitor at 115200 baud

## Three-Sensor Calibration Sketch

The `TempHumidity3sensors/` directory contains a separate sketch for cross-validating sensor accuracy using three I2C sensors simultaneously:

| Sensor | Address | Purpose |
|--------|---------|---------|
| SHT41 | 0x44 | High-precision reference |
| BME280 (generic) | 0x76 | Production candidate |
| BME280 (Adafruit) | 0x77 | Second reference |

### Additional BOM for Calibration
| Component | Qty | Description |
|-----------|-----|-------------|
| Adafruit SHT41 breakout | 1 | Reference sensor |
| Adafruit BME280 breakout | 1 | Second BME280 at 0x77 |

All three share the same I2C bus (SDA=D4, SCL=D5) with different addresses.

## Relationship to HiveController
This sketch was the starting point for Phase 0. The production HiveController evolved from it by adding:
- ESP-NOW communication (deep sleep compatible)
- Battery voltage monitoring (external voltage divider)
- Fault tolerance (sends data even if BME280 is missing)
- Sensor status bitmask
- Configurable sleep modes (planned)

This sketch is kept for:
- Quick sensor validation without the ESP-NOW stack
- BLE characteristic testing
- WiFi dashboard demos
- Multi-sensor calibration runs
