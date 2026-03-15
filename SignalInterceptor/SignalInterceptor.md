# Signal Interceptor — Technical Design Document

## Overview
The Signal Interceptor captures raw RF packets broadcast by the Ecowitt WS-90 weather station, decodes them locally, and feeds the weather data to the Yard C&C. This eliminates dependency on Ecowitt's proprietary cloud servers and provides hyper-local environmental context for hive telemetry.

## Weather Station: Ecowitt WS-90

### Capabilities
- Wind speed and direction (ultrasonic, no moving parts)
- Haptic rain sensing
- Solar radiation (W/m²)
- UV index
- Temperature
- Humidity
- Barometric pressure

### RF Characteristics
- Frequency: 915MHz (US) / 868MHz (EU) — check local regulations
- Protocol: proprietary Ecowitt/Fine Offset
- Broadcast interval: ~16 seconds (typical)
- Range: ~100m line-of-sight

## Interceptor Hardware Options

### Option A: RTL-SDR USB Dongle
- Hardware: RTL2832U-based USB dongle (~$25) + antenna
- Connected to: Pi Zero W USB port
- Software: `rtl_433` — open-source decoder that supports Ecowitt/Fine Offset protocol out of the box
- Pros: very well documented, large community, supports hundreds of weather station protocols
- Cons: uses a USB port on the Pi, higher power draw, bulkier

### Option B: Dedicated Sub-GHz RF Module
- Hardware: CC1101 or RFM69 module wired via SPI to the Pi or a dedicated MCU
- Pros: lower power, smaller, can be integrated into the C&C enclosure
- Cons: requires custom decoding, fewer community resources

### Option C: CC1101 on a Dedicated ESP32
- Hardware: CC1101 module wired to a spare ESP32
- The ESP32 decodes the packets and forwards to the Pi via serial or ESP-NOW
- Pros: offloads RF processing from the Pi, modular
- Cons: another board to power and maintain

### Recommended Approach
**Start with Option A (RTL-SDR + rtl_433)**. It's the fastest path to working weather data with minimal custom code. Migrate to Option B or C later if power/size constraints demand it.

## Software: rtl_433

### Installation on Pi
```bash
sudo apt install rtl-433
```
Or build from source for the latest protocol support:
```bash
git clone https://github.com/merbanan/rtl_433.git
cd rtl_433 && mkdir build && cd build
cmake .. && make && sudo make install
```

### Usage
```bash
# List detected devices
rtl_433 -F json

# Filter for Ecowitt/Fine Offset devices only
rtl_433 -R 78 -F json

# Output to a file
rtl_433 -R 78 -F json > weather.jsonl
```

### Output Format (example)
```json
{
  "time": "2026-03-15 14:30:00",
  "model": "Fineoffset-WS90",
  "id": 12345,
  "battery_ok": 1,
  "temperature_C": 22.3,
  "humidity": 55,
  "wind_dir_deg": 180,
  "wind_avg_m_s": 2.1,
  "wind_max_m_s": 4.5,
  "uv": 3,
  "uvi": 2,
  "light_lux": 45000,
  "rain_mm": 0.0
}
```

## Integration with Yard C&C

### Approach 1: Pipe rtl_433 into Python
```bash
rtl_433 -R 78 -F json | python3 weather_ingest.py
```
A small Python script reads JSON lines from stdin, timestamps them, and writes to the same log directory as hive data.

### Approach 2: rtl_433 MQTT Output
```bash
rtl_433 -R 78 -F mqtt://localhost:1883
```
The Pi runs a lightweight MQTT broker (Mosquitto). Both weather and hive data publish to topics. This scales better when adding more data sources.

### Approach 3: Direct File Logging
```bash
rtl_433 -R 78 -F json:~/beeyard_logs/weather.jsonl
```
Simplest approach — just append to a file. The C&C merges hive + weather logs at query time.

### Recommended Approach
**Start with Approach 1 (pipe to Python)** for consistency with the existing `yard_cc.py` pattern. Move to MQTT when adding more data sources in Phase 5.

## Data Merging Strategy
- Weather data is timestamped independently from hive data
- Correlation is done by time window (e.g., closest weather reading within ±30 seconds of a hive reading)
- Weather provides context: sudden temperature drops, rain events, wind — all correlate with bee behavior

## Antenna Considerations
- The stock RTL-SDR antenna is tuned for ~1090MHz (ADS-B) — works at 915MHz but not optimal
- A 915MHz quarter-wave whip antenna (~8cm) improves range significantly
- Mount the antenna with line-of-sight to the WS-90 — avoid metal enclosures

## Future Enhancements
- [ ] Implement rtl_433 integration on the Pi
- [ ] Weather data ingest script (pipe or MQTT)
- [ ] Merge weather + hive data in the C&C logs
- [ ] Alert on weather events (high wind, rain, temperature drops)
- [ ] Migrate from RTL-SDR to CC1101 module for lower power/size (if needed)
- [ ] Validate WS-90 RF range and antenna placement at the apiary site
