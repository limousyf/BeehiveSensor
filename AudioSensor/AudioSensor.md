# Audio Sensor — Technical Design Document

## Overview
The Audio Sensor captures hive acoustics using an INMP441 I2S MEMS microphone. Sound analysis provides insight into colony health, queen status, swarming behavior, and overall activity levels. This is a standalone node running on a XIAO ESP32S3, separate from the Hive Controller (C3) which handles slow sensors (temp/humidity/scale).

## Hardware

### Microphone: INMP441
- Type: I2S digital MEMS microphone
- Frequency response: 60Hz - 15kHz
- Sensitivity: -26 dBFS
- SNR: 61 dB
- Supply voltage: 1.8V - 3.3V
- Current draw: ~1.4mA active
- Output: 24-bit I2S data (left-justified in 32-bit words)

### Test Platform: ESP32-WROOM-32
Used for initial development. Production target is the XIAO ESP32S3.

### Pin Assignments (WROOM-32 — tested and working)
| INMP441 Pin | ESP32 Pin | Function |
|-------------|-----------|----------|
| VDD | 3V3 | Power |
| GND | GND | Ground |
| L/R | GND | Left channel select |
| SD | GPIO33 | I2S serial data |
| SCK | GPIO32 | I2S bit clock |
| WS | GPIO25 | I2S word select |

**Important:** SD and SCK labels can be confusing — if you get all zeros, try swapping these two wires.

### Pin Assignments (XIAO ESP32S3 — planned)
To be determined. The S3 has plenty of available GPIOs for I2S (3 pins: SD, SCK, WS).

### Power Architecture
- **Prototyping:** dedicated 18650 battery for the S3 audio node
- **Production:** single battery + shared charge controller powering both C3 and S3 boards

## I2S Configuration
- Sample rate: 16,000 Hz (sufficient for bee sounds up to 8kHz Nyquist)
- Bit depth: 32-bit (24-bit data in upper bits, shift right by 8)
- Channel: left only (L/R pin tied to GND)
- DMA buffers: 4 x 512 samples

## Acoustic Analysis

### Current Implementation (Test Sketch)
- **RMS level**: root-mean-square of sample block — overall loudness
- **dBFS**: decibels relative to full scale (0 = max, -96 = silence for 24-bit)
- **Peak frequency**: simple DFT magnitude scan across bins up to 4kHz
- Bin resolution: 31.25 Hz (16000 / 512)

### Planned Analysis for Production
For the hive controller, raw audio is too large to transmit. Instead, send a compact acoustic summary:

#### Option A: Frequency Band Energy
Split spectrum into bands relevant to bee behavior and send energy per band:
| Band | Frequency | Bee Significance |
|------|-----------|-----------------|
| Low | 50-200 Hz | Wing vibration fundamentals |
| Mid-Low | 200-400 Hz | Normal foraging buzz, fanning |
| Mid | 400-600 Hz | Queen piping (~500Hz) |
| Mid-High | 600-1000 Hz | Agitation, defensive buzz |
| High | 1000-4000 Hz | Harmonics, general activity |

#### Option B: Key Metrics Only
- Overall RMS / dBFS
- Peak frequency
- Activity score (0-100 derived from energy above baseline)

#### Option C: Both
Send band energies + overall metrics. ~30 bytes total — fits easily in the HivePacket.

### Recommended: Option C
Start with key metrics (Option B) in the HivePacket struct, add band energies when the analysis is validated against real hive recordings.

## Bee Sound Signatures

### Reference Frequencies
| Sound | Frequency | Meaning |
|-------|-----------|---------|
| Normal buzz | 200-300 Hz | Healthy foraging activity |
| Fanning | 200-250 Hz | Temperature/humidity regulation |
| Queen piping | 400-500 Hz | Virgin queen announcing presence |
| Tooting | ~500 Hz | Emerged virgin queen |
| Quacking | ~350 Hz | Caged virgin queen (still in cell) |
| Queenless roar | Broad 200-500 Hz, elevated | Colony stress, no queen |
| Swarming | High energy, broad spectrum | Imminent or active swarm |
| Hissing | 1-4 kHz | Defensive/alarm behavior |

### Detection Strategy
- Compare current readings against a **baseline** established during known-healthy state
- Track **trends** rather than absolute values (ambient noise varies by location)
- Flag anomalies: sudden frequency shifts, sustained elevation in specific bands
- Store baseline in NVS per hive

## Power Considerations
- INMP441 draws ~1.4mA continuously — significant for battery operation
- Strategy: power the mic via a GPIO pin, turn on only during sampling window
- Sample for ~1 second, compute metrics, power down, deep sleep
- Alternative: use a MOSFET to switch mic power if GPIO current is insufficient

## Integration into HivePacket

### New Fields (planned)
```cpp
typedef struct __attribute__((packed)) {
  // ... existing fields ...
  uint16_t audio_rms;       // overall sound level
  uint16_t audio_peak_freq; // dominant frequency in Hz
  uint8_t  audio_activity;  // activity score 0-100
} HivePacket;
```

### Sensor Status Bitmask
- Bit 2 (0x04): INMP441 microphone

## Placement
- Mount inside the hive body, in the frame space
- Protect from propolis buildup — small mesh cover over the mic port
- Orient mic port facing the frames, not the hive wall
- Keep wiring short to minimize I2S signal degradation

## Future Enhancements
- [ ] Integrate into HiveController on XIAO ESP32C3
- [ ] GPIO-controlled mic power for battery savings
- [ ] Frequency band energy analysis
- [ ] Baseline calibration stored in NVS
- [ ] Anomaly detection (queen loss, swarming, agitation alerts)
- [ ] Record short audio clips to LittleFS for remote playback (admin mode only)
- [ ] Validate against real hive recordings to tune detection thresholds
