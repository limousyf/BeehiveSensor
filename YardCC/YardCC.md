# Yard Command & Control — Technical Design Document

## Overview
The Yard C&C is the central nervous system for one apiary. It runs on a Pi Zero W, receives aggregated hive data from the ESP-NOW Bridge over UART, and logs it locally. In future phases it will also ingest weather data from the Signal Interceptor and push everything to the cloud.

## Hardware

### Core Board: Raspberry Pi Zero W
- CPU: BCM2835 ARM11 1GHz single-core
- RAM: 512MB
- WiFi: 2.4GHz 802.11 b/g/n
- Bluetooth: 4.1
- Planned upgrade: Pi Zero 2 W (quad-core, same form factor)

### GPIO Pin Assignments
| Pi Pin | GPIO | Function | Connected To |
|--------|------|----------|-------------|
| Pin 2 | — | 5V power out | ESP32 Bridge VIN |
| Pin 4 | — | 5V power out | (alternative to Pin 2) |
| Pin 6 | — | GND | ESP32 Bridge GND |
| Pin 8 | GPIO14 (TXD) | UART TX | ESP32 Bridge GPIO16 (RX2) — future use |
| Pin 10 | GPIO15 (RXD) | UART RX | ESP32 Bridge GPIO17 (TX2) |

### Power
- USB Micro-B, requires a reliable 5V 2.5A+ supply
- Must supply enough current for both the Pi and the ESP32 Bridge via GPIO 5V pins
- Weak USB blocks cause brownouts on the bridge — use a quality adapter

## OS & Configuration

### Raspberry Pi OS
- Standard Raspberry Pi OS Lite (headless)
- SSH enabled for remote access

### Required `/boot/firmware/config.txt` Settings
```
enable_uart=1
dtoverlay=disable-bt
```
- **Config file location:** `/boot/firmware/config.txt` (NOT `/boot/config.txt` on newer OS versions)
- `enable_uart=1`: enables the UART hardware
- `dtoverlay=disable-bt`: switches `/dev/serial0` from the unreliable mini UART (`ttyS0`) to the stable PL011 UART (`ttyAMA0`)
- Disabling Bluetooth is acceptable — the Pi doesn't use BLE in this design
- Reboot required after changes

### Serial Console
- Login shell over serial must be **disabled** (`raspi-config` → Interface Options → Serial Port → Login shell: NO, Hardware: YES)
- Otherwise the Linux console grabs the UART port

### Python Dependencies
```bash
sudo apt install python3-serial
```
- `pip`/`pip3` may not be available on minimal Raspberry Pi OS — use `apt` instead

## Software: yard_cc.py

### Function
- Reads JSON lines from `/dev/serial0` at 9600 baud
- Adds a `received_at` ISO timestamp to each record
- Strips leading garbage before `{` (ESP32 boot ROM noise)
- Logs to daily JSONL files: `~/beeyard_logs/YYYY-MM-DD.jsonl`
- Prints to console for live monitoring

### Log Format
Each line in the log file is a complete JSON object:
```json
{"sensor_id":"HIVE_NODE_01","bme280":"ok","temp":21.0,"hum":61.5,"press":978.0,"batt_v":4.16,"batt_pct":96,"received_at":"2026-03-15T13:41:42.507164"}
```

### Running
```bash
python3 yard_cc.py
```
- Runs in the foreground — use `screen`, `tmux`, or a systemd service for persistence
- Logs directory is auto-created at `~/beeyard_logs/`

## Data Flow

```
ESP32 Bridge --[UART 9600 /dev/serial0]--> yard_cc.py
  --> console output (live monitoring)
  --> ~/beeyard_logs/YYYY-MM-DD.jsonl (persistent storage)
```

## Future Architecture

### Phase 4: Weather Data Integration
- Signal Interceptor feeds decoded WS-90 weather data to the Pi
- yard_cc.py (or a separate process) merges weather + hive data
- Combined payload provides environmental context for hive telemetry

### Phase 5: Cloud Push
- Pi pushes aggregated data to the cloud (AWS, InfluxDB, custom backend)
- Grafana or dedicated app for visualization and alerting
- Backhaul over the hill via Ubiquiti point-to-point relay

## Future Enhancements
- [ ] Run as a systemd service (auto-start on boot, restart on crash)
- [ ] Send commands back to Hive Controllers via Bridge (UART TX → ESP-NOW unicast)
- [ ] Merge weather data from Signal Interceptor
- [ ] Cloud push with retry queue for connectivity gaps
- [ ] Dashboard: local web UI on the Pi for field diagnostics
- [ ] Alert system: swarm detection, weight anomalies, battery low, sensor offline
- [ ] Log rotation / archival for long-term deployments
- [ ] Upgrade path: Pi Zero W → Pi Zero 2 W (drop-in replacement, same GPIO layout)
