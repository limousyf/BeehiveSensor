/*
 * ESP-NOW Bridge (ESP32-WROOM-32)
 *
 * Receives ESP-NOW packets from Hive Controllers and forwards
 * them as JSON lines over UART (Serial2) to the Pi Zero W C&C.
 *
 * Wiring to Pi Zero W:
 *   ESP32 GPIO17 (TX2) --> Pi Pin 10 (GPIO15 / RXD)
 *   ESP32 GPIO16 (RX2) <-- Pi Pin 8  (GPIO14 / TXD)  [future use]
 *   ESP32 GND          --- Pi Pin 6  (GND)
 *   ESP32 VIN          <-- Pi Pin 2  (5V)
 */

#include <esp_now.h>
#include <WiFi.h>

// UART2 to Pi
#define PI_SERIAL Serial2
#define PI_TX 17
#define PI_RX 16
#define PI_BAUD 115200

// Must match the struct on the Hive Controller
typedef struct __attribute__((packed)) {
  char sensor_id[20];
  float temperature;
  float humidity;
  float pressure;
} HivePacket;

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(HivePacket)) {
    Serial.printf("[BRIDGE] Bad packet size: %d (expected %d)\n", len, sizeof(HivePacket));
    return;
  }

  HivePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  // Build JSON line
  char json[200];
  snprintf(json, sizeof(json),
    "{\"sensor_id\":\"%s\",\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f}",
    pkt.sensor_id, pkt.temperature, pkt.humidity, pkt.pressure);

  // Forward to Pi over UART
  PI_SERIAL.println(json);

  // Echo to USB serial for debugging
  const uint8_t *mac = info->src_addr;
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[BRIDGE] From %s: %s\n", macStr, json);
}

void setup() {
  Serial.begin(115200);
  PI_SERIAL.begin(PI_BAUD, SERIAL_8N1, PI_RX, PI_TX);

  Serial.println("\n--- ESP-NOW Bridge Starting ---");

  // ESP-NOW requires WiFi in station mode (no connection needed)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("Bridge MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error: ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("ESP-NOW listening. Forwarding to Pi on UART2.");
}

void loop() {
  // Nothing here — all work happens in the onDataRecv callback
  delay(1);
}
