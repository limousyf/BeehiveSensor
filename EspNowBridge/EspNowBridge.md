# ESP-NOW Bridge — Technical Design Document

## Overview
The ESP-NOW Bridge sits at the Yard C&C and acts as the radio-to-serial translator. It receives ESP-NOW packets from all Hive Controllers in range, converts them to JSON, and forwards them over UART to the Pi Zero W.

## Hardware

### Core Board: ESP32-WROOM-32
- CPU: Xtensa LX6 dual-core 240MHz
- Flash: 4MB
- RAM: 520KB
- WiFi: 2.4GHz 802.11 b/g/n
- BLE: Bluetooth 4.2
- Power: continuously on (must catch asynchronous ESP-NOW packets)

### Pin Assignments
| Pin | Function | Connected To |
|-----|----------|-------------|
| GPIO17 (TX2) | UART2 TX | Pi Pin 10 (GPIO15/RXD) |
| GPIO16 (RX2) | UART2 RX | Pi Pin 8 (GPIO14/TXD) — future use |
| GND | Ground | Pi Pin 6 |
| VIN | 5V input | Pi Pin 2 or Pin 4 |

### Power
- Powered from Pi Zero W 5V GPIO pins (Pin 2 or Pin 4 → VIN)
- Alternative: separate USB power supply (more reliable for current-limited Pi power supplies)
- Current draw: ~120-240mA continuous (WiFi radio always listening)
- **Important:** verify solder joints on Pi GPIO header — cold joints cause intermittent power loss

## Communication

### ESP-NOW Receive (Hive Controllers → Bridge)
- Listens in WiFi station mode (`WIFI_STA`) with no AP connection
- Receives broadcast packets from any Hive Controller in range
- No pairing required — any ESP-NOW device on the same channel is accepted
- Default WiFi channel: 1 (must match all Hive Controllers)
- Callback-driven: `onDataRecv` fires immediately on packet receipt

### UART to Pi (Bridge → Yard C&C)
- Uses hardware UART2 (`Serial2`) — independent from USB Serial
- Baud rate: **9600** (115200 caused corruption over dupont wires to Pi)
- Format: one JSON line per packet, terminated with `\n`
- USB Serial (UART0, 115200 baud) remains available for debug output to a laptop

### Future: Pi → Bridge Commands
- GPIO16 (RX2) is wired but not yet used
- Could accept commands from the Pi to relay back to Hive Controllers
- Would require ESP-NOW unicast (targeted MAC) for reliable delivery

## Data Flow

```
Hive Controller --[ESP-NOW broadcast]--> Bridge
Bridge: validate packet size → deserialize struct → build JSON → Serial2.println()
Bridge --[UART 9600]--> Pi /dev/serial0
Bridge --[USB 115200]--> laptop serial monitor (debug, optional)
```

## JSON Output Format

### All sensors online
```json
{"sensor_id":"HIVE_NODE_01","bme280":"ok","temp":21.0,"hum":61.5,"press":978.0,"batt_v":4.16,"batt_pct":96}
```

### BME280 offline
```json
{"sensor_id":"HIVE_NODE_01","bme280":"offline","batt_v":4.16,"batt_pct":96}
```

The bridge conditionally includes sensor data fields based on the `sensor_status` bitmask in the packet. Fields for offline sensors are omitted entirely (not sent as zero).

## Packet Validation
- Incoming packets are checked against `sizeof(HivePacket)`
- Mismatched sizes are logged as `[BRIDGE] Bad packet size` and dropped
- **Both the bridge and hive controller must be reflashed together when the struct changes**

## ESP-IDF API Notes
- The WROOM-32 board package uses ESP-IDF v5.x
- Receive callback signature: `void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)`
- Sender MAC accessed via `info->src_addr`
- Boot ROM outputs garbage on all TX pins at startup — the Pi script strips leading garbage before the `{` in each line

## Scaling Considerations
- One bridge can receive from **many** hive controllers — ESP-NOW supports up to 20 peers, but broadcast doesn't count against peer limits
- If multiple apiaries are in range, use sensor_id to distinguish hives
- Future: could filter by MAC whitelist to reject unknown senders

## Future Enhancements
- [ ] Relay commands from Pi back to Hive Controllers via ESP-NOW unicast
- [ ] MAC address whitelist for sender validation
- [ ] Heartbeat/watchdog: Pi can detect bridge is alive by checking for periodic data
- [ ] Status LED: blink on successful packet receive
