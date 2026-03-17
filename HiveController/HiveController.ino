/*
 * Hive Controller (XIAO ESP32C3)
 *
 * Reads BME280 (temp, humidity, pressure) and sends data via
 * one of three radio modes: ESP-NOW, WiFi, or BLE.
 *
 * Fault-tolerant: sends whatever data is available.
 * Missing sensors are flagged in the status field.
 *
 * ESP-NOW mode: broadcast to bridge, then deep sleep.
 * WiFi mode:    serve a web dashboard (stays awake).
 * BLE mode:     advertise BLE characteristics (stays awake).
 *
 * Hardware:
 *   - Seeed Studio XIAO ESP32C3
 *   - BME280 on I2C (SDA=D4, SCL=D5, addr 0x76)
 *   - 1x 18650 battery via onboard charge circuit
 *   - External voltage divider (2x 100kΩ) on A0 for battery ADC
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <esp_now.h>

// --- RADIO MODE (pick exactly one) ---
#define USE_ESPNOW    true
#define USE_WIFI      false
#define USE_BLUETOOTH false

#if (USE_ESPNOW + USE_WIFI + USE_BLUETOOTH) != 1
  #error "Enable exactly ONE radio mode!"
#endif

// --- SENSOR IDENTIFIER ---
const char SENSOR_ID[20] = "HIVE_NODE_01";

// Deep sleep duration (only used in ESP-NOW mode)
#define SLEEP_SECONDS 30

// XIAO C3 I2C Pins
#define I2C_SDA D4
#define I2C_SCL D5

// Battery voltage ADC pin
#define BATT_ADC_PIN A0

// Low battery cutoff — sleep indefinitely below this voltage to protect unprotected cells
#define BATT_CUTOFF_V 3.0

// Sensor status flags (bitmask)
#define STATUS_BME280_OK  0x01

Adafruit_BME280 bme;
bool bmeAvailable = false;

// Global readings
float temp = 0.0;
float humidity = 0.0;
float pressure = 0.0;

float readBatteryVoltage() {
  uint32_t raw = analogReadMilliVolts(BATT_ADC_PIN);
  // External voltage divider (2x 100kΩ) halves the battery voltage
  return (raw * 2.0) / 1000.0;
}

uint8_t voltageToPct(float v) {
  // Rough Li-ion discharge curve: 4.2V=100%, 3.0V=0%
  if (v >= 4.2) return 100;
  if (v <= 3.0) return 0;
  return (uint8_t)((v - 3.0) / 1.2 * 100.0);
}

unsigned long previousMillis = 0;
const long interval = 2000;

// --- ESP-NOW ---
#if USE_ESPNOW
  uint8_t broadcastAddr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Must match the struct on the ESP-NOW Bridge
  typedef struct __attribute__((packed)) {
    char sensor_id[20];
    float temperature;
    float humidity;
    float pressure;
    float battery_voltage;
    uint8_t battery_pct;
    uint8_t sensor_status; // bitmask: bit 0 = BME280
  } HivePacket;

  volatile bool sendDone = false;
  volatile bool sendOk = false;

  void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    sendOk = (status == ESP_NOW_SEND_SUCCESS);
    sendDone = true;
    Serial.printf("[HIVE] Send %s\n", sendOk ? "OK" : "FAILED");
  }

  void goToSleep() {
    Serial.printf("[HIVE] Sleeping for %d seconds...\n", SLEEP_SECONDS);
    Serial.flush();
    esp_deep_sleep(SLEEP_SECONDS * 1000000ULL);
  }
#endif

// --- WIFI ---
#if USE_WIFI
  #include <WebServer.h>
  const char* ssid = "YOUR_SSID";
  const char* password = "YOUR_PASSWORD";
  WebServer server(80);

  void handleRoot() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body{font-family:sans-serif; text-align:center; margin-top:30px; background-color: #fafafa;}";
    html += "h1{color:#E6A817;} .meta{color:#888; font-size:16px;}</style></head>";
    html += "<body><h1>Beeyard Hive Controller</h1>";
    html += "<p class='meta'>Sensor ID: <b>" + String(SENSOR_ID) + "</b></p><hr>";
    if (bmeAvailable) {
      html += "<h2>Temp: " + String(temp, 1) + " &deg;C</h2>";
      html += "<h2>Hum: " + String(humidity, 1) + " %</h2>";
      html += "<h2>Press: " + String(pressure, 1) + " hPa</h2>";
    } else {
      html += "<h2 style='color:red;'>BME280: OFFLINE</h2>";
    }
    html += "</body></html>";
    server.send(200, "text/html", html);
  }
#endif

// --- BLUETOOTH ---
#if USE_BLUETOOTH
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
  #define ID_CHAR_UUID        "11111111-1fb5-459e-8fcc-c5c9c331914b"
  #define TEMP_CHAR_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
  #define HUM_CHAR_UUID       "8d9830da-e7cc-4bf4-be23-57577884bbbb"
  #define PRESS_CHAR_UUID     "76a3928e-6b80-4573-a444-15f5cc14ba64"

  BLECharacteristic *pIdChar;
  BLECharacteristic *pTempChar;
  BLECharacteristic *pHumChar;
  BLECharacteristic *pPressChar;
#endif

void setup() {
  Serial.begin(115200);
  delay(3000);  // Wait for serial monitor connection

  Serial.println("\n--- Hive Controller Starting ---");
  Serial.printf("Sensor ID: %s\n", SENSOR_ID);

  // Check battery voltage — protect unprotected cells from over-discharge
  float bootVoltage = readBatteryVoltage();
  Serial.printf("Battery: %.2fV\n", bootVoltage);
  if (bootVoltage < BATT_CUTOFF_V && bootVoltage > 0.5) {
    // Below 0.5V means no battery / no voltage divider — skip check
    Serial.println("[CRIT] Battery too low! Sleeping indefinitely.");
    Serial.flush();
    esp_deep_sleep(0);  // Sleep forever — only USB/reset wakes it
  }

  // Init BME280 (non-fatal if missing)
  Wire.begin(I2C_SDA, I2C_SCL);
  bmeAvailable = bme.begin(0x76, &Wire);
  if (!bmeAvailable) {
    Serial.println("[WARN] BME280 not found at 0x76 — skipping");
  } else {
    Serial.println("BME280 (0x76) Initialized.");
  }

  // --- ESP-NOW: read, send, sleep ---
  #if USE_ESPNOW
    HivePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    strncpy(pkt.sensor_id, SENSOR_ID, sizeof(pkt.sensor_id));

    // Build sensor status bitmask
    pkt.sensor_status = 0;
    if (bmeAvailable) {
      pkt.sensor_status |= STATUS_BME280_OK;
      pkt.temperature = bme.readTemperature();
      pkt.humidity = bme.readHumidity();
      pkt.pressure = bme.readPressure() / 100.0F;
    }

    // Battery is always available (external voltage divider)
    pkt.battery_voltage = readBatteryVoltage();
    pkt.battery_pct = voltageToPct(pkt.battery_voltage);

    Serial.printf("[HIVE] BME280:%s", bmeAvailable ? "OK" : "OFFLINE");
    if (bmeAvailable) {
      Serial.printf(" T:%.1fC H:%.1f%% P:%.1fhPa",
        pkt.temperature, pkt.humidity, pkt.pressure);
    }
    Serial.printf(" Batt:%.2fV (%d%%)\n",
      pkt.battery_voltage, pkt.battery_pct);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
      Serial.println("Error: ESP-NOW init failed!");
      goToSleep();
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, broadcastAddr, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("Error: Failed to add broadcast peer!");
      goToSleep();
    }

    esp_now_send(broadcastAddr, (uint8_t *)&pkt, sizeof(pkt));

    // Wait for send callback confirmation (with timeout)
    unsigned long start = millis();
    while (!sendDone && (millis() - start < 2000)) {
      delay(1);
    }

    goToSleep();
  #endif

  // --- WiFi: start web server ---
  #if USE_WIFI
    Serial.print("Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected!");
    Serial.print("Visit IP: "); Serial.println(WiFi.localIP());
    server.on("/", handleRoot);
    server.begin();
  #endif

  // --- BLE: start advertising ---
  #if USE_BLUETOOTH
    Serial.println("Starting BLE... (Name: Beeyard_Hive_1)");
    BLEDevice::init("Beeyard_Hive_1");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pIdChar = pService->createCharacteristic(ID_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pTempChar = pService->createCharacteristic(TEMP_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pHumChar = pService->createCharacteristic(HUM_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pPressChar = pService->createCharacteristic(PRESS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);

    pIdChar->setValue(SENSOR_ID);

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("BLE Active.");
  #endif
}

void loop() {
  #if USE_ESPNOW
    // Never reached — deep sleep resets into setup()
  #endif

  #if USE_WIFI
    server.handleClient();
  #endif

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (bmeAvailable) {
      temp = bme.readTemperature();
      humidity = bme.readHumidity();
      pressure = bme.readPressure() / 100.0F;

      Serial.printf("[%s] T:%.1fC H:%.1f%% P:%.1fhPa\n",
        SENSOR_ID, temp, humidity, pressure);
    }

    #if USE_BLUETOOTH
      if (bmeAvailable) {
        pTempChar->setValue((String(temp, 1) + " C").c_str());
        pHumChar->setValue((String(humidity, 1) + " %").c_str());
        pPressChar->setValue((String(pressure, 1) + " hPa").c_str());
      }
    #endif
  }
}
