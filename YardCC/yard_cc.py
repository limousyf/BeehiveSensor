#!/usr/bin/env python3
"""
Yard Command & Control (Pi Zero W)

Reads JSON lines from the ESP-NOW bridge over UART
and logs them to a timestamped file.

UART: /dev/serial0 at 115200 baud
  Pi Pin 10 (GPIO15/RXD) <-- ESP32 GPIO17 (TX2)
"""

import serial
import json
import os
from datetime import datetime

SERIAL_PORT = "/dev/serial0"
BAUD_RATE = 115200
LOG_DIR = os.path.expanduser("~/beeyard_logs")


def get_log_path():
    """Return log file path for today: ~/beeyard_logs/2026-03-14.jsonl"""
    os.makedirs(LOG_DIR, exist_ok=True)
    date_str = datetime.now().strftime("%Y-%m-%d")
    return os.path.join(LOG_DIR, f"{date_str}.jsonl")


def main():
    print("--- Yard C&C Starting ---")
    print(f"Listening on {SERIAL_PORT} at {BAUD_RATE} baud")
    print(f"Logging to {LOG_DIR}/")

    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

    while True:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if not line:
            continue

        # Add a receive timestamp
        timestamp = datetime.now().isoformat()

        try:
            data = json.loads(line)
            data["received_at"] = timestamp
        except json.JSONDecodeError:
            print(f"[WARN] Bad JSON: {line}")
            continue

        # Log to file
        log_path = get_log_path()
        with open(log_path, "a") as f:
            f.write(json.dumps(data) + "\n")

        # Print to console
        print(f"[{timestamp}] {data.get('sensor_id', '???')}: "
              f"T={data.get('temp')}C H={data.get('hum')}% P={data.get('press')}hPa")


if __name__ == "__main__":
    main()
