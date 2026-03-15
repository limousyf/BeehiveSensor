Product Requirements Document (PRD): Beeyard Intelligence System
Version: 3.0
Date: March 14, 2026
Objective: A modular, multi-tiered data collection pipeline spanning from individual hive metrics up to an apiary-wide Command & Control (C&C) hub, bridging challenging physical terrain to deliver real-time telemetry and computer vision data to the cloud.

1. System Topology Overview (The Data Pipeline)
The architecture follows a strict hierarchy to manage power consumption and data flow across difficult terrain.

The Hive Stack: Individual sensor nodes (Telemetry + Vision) on each hive.

The Yard Stack (C&C): A central aggregator for the entire apiary that also intercepts local weather data.

The Backhaul (Relay): A solar-powered bridge traversing physical obstacles to reach the primary internet connection.

The Cloud: The final database and visualization dashboard.

2. Level 1: The Hive Stack (Replicated 1 Per Hive)
Each hive requires two physically decoupled nodes to manage conflicting power requirements (low-power telemetry vs. high-power continuous video processing).

A. The Hive Controller (Telemetry & Health)

Function: The deep-sleep capable "heartbeat" monitor.

Core Hardware: Seeed Studio XIAO ESP32C3.

Power: 1x 18650 Lithium-Ion Battery (utilizing onboard charge circuit).

Attached Sensors:

Temperature & Humidity: Adafruit BME280 (I2C) inside the inner cover.

Weight / Scale: 4x 50kg Load Cells + HX711 Amplifier under the hive stand.

Sound / Acoustics: INMP441 I2S Digital MEMS Microphone inside the frame space.

Comms: Shouts data to the Yard C&C via ESP-NOW, then immediately returns to deep sleep.

B. The Hive Bee Counter (Vision & Traffic)

Function: The high-power, continuously awake monitor running object/motion detection algorithms to count foragers.

Core Hardware: ESP32-CAM (or similar vision module).

Power: Independent high-capacity battery bank (3x 18650s) + Dedicated 5V Solar Panel on the hive roof.

Placement: Mounted in a 3D-printed shroud directly above the landing board.

Comms: Transmits numerical activity scores to the Yard C&C via ESP-NOW or Wi-Fi.

3. Level 2: The Yard Stack (Replicated 1 Per Apiary)
This is the central nervous system for the physical location. It aggregates all hive data and provides crucial environmental context.

A. The Yard Command & Control (C&C)

Function: The "Base Station" that listens for all incoming ESP-NOW packets from the Hive Controllers and Bee Counters.

Core Hardware: A robust microcontroller (e.g., standard ESP32, ESP32-S3) or a Single Board Computer (Raspberry Pi Zero 2 W).

Power: Sizable solar array and deep-cycle battery (or large lithium bank), as this unit must remain powered continuously to "catch" asynchronous data packets.

B. The Weather Station (Ecowitt WS-90)

Function: A professional-grade ultrasonic weather array providing hyper-local context (wind speed/direction, haptic rain sensing, solar radiation, UV, temperature, humidity).

Hardware: Standalone Ecowitt WS-90 mounted on a pole within the apiary.

Comms: Broadcasts proprietary RF packets (typically 915MHz in the US).

C. The Signal Interceptor

Function: Sniffs the raw RF packets broadcast by the WS-90 so the apiary doesn't rely on Ecowitt's proprietary cloud servers.

Hardware: An RTL-SDR (Software Defined Radio) USB dongle, or a dedicated sub-GHz RF receiver module (like an RFM69 or CC1101) wired directly to the C&C board.

Data Flow: The interceptor pulls the weather data locally, and the C&C packages it together with the hive metrics.

4. Level 3: Backhaul & The Cloud (The "Over the Hill" Link)
Getting data out of an isolated apiary—especially one blocked by a 100ft tree-covered hill where LTE is poor—requires a multi-hop infrastructure.

A. C&C to Relay Communications

The Yard C&C must push its aggregated data payload up the hill. Depending on the exact line-of-sight and bandwidth requirements (especially if transmitting raw images from the Bee Counter), the C&C uses one of the following:

High Bandwidth (Vision included): Standard Wi-Fi bridging.

Low Bandwidth (Text/Numbers only): Long-range ESP-NOW or LoRa (Long Range RF).

B. The Infrastructure Relay

Function: A solar-powered repeater stationed at the crest of the hill to establish Line-of-Sight (LoS) with both the apiary and the main property.

Core Hardware: Ubiquiti AirMAX or SunMAX point-to-point dish antennas.

Power: A dedicated 12V or 24V solar panel paired with an SLA (Sealed Lead Acid) or LiFePO4 battery and an MPPT charge controller to keep the networking gear running 24/7.

C. The Cloud Connection

The Final Hop: The relay at the top of the hill beams the signal down to the existing outdoor Wi-Fi router on the main property.

The Destination: The router pushes the structured payload to the cloud (e.g., AWS, InfluxDB, or a custom backend), where Grafana or a dedicated app translates the data into readable graphs and swarm alerts.

5. Development Phasing
Phase 0: Validate the Hive Controller telemetry baseline (T&H + Power + Enclosure).

Phase 1: Integrate the HX711 scale hardware into the Hive Controller.

Phase 2: Integrate the INMP441 acoustic hardware into the Hive Controller.

Phase 3: Develop the standalone Hive Bee Counter (Vision Node).

Phase 4: Build the Yard C&C and intercept the WS-90 RF weather signals.

Phase 5: Deploy the solar-powered network Relay on the hill to finalize the data pipeline to the Cloud.