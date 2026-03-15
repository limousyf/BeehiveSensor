#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- THE MASTER TOGGLES ---
#define USE_WIFI false
#define USE_BLUETOOTH true

#if USE_WIFI && USE_BLUETOOTH
  #error "Please set only ONE radio mode to true!"
#endif

// --- SENSOR IDENTIFIER ---
const String SENSOR_ID = "HIVE_NODE_01";

// XIAO C3 I2C Pins
#define I2C_SDA D4
#define I2C_SCL D5

Adafruit_BME280 bme;

// Global variables to hold our latest readings
float temp = 0.0;
float humidity = 0.0;
float pressure = 0.0;

unsigned long previousMillis = 0;
const long interval = 2000;

// --- CONDITIONAL LIBRARIES & VARIABLES ---
#if USE_WIFI
  #include <WiFi.h>
  #include <WebServer.h>
  const char* ssid = "YOUR_SSID";
  const char* password = "YOUR_PASSWORD";
  WebServer server(80);

  void handleRoot() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body{font-family:sans-serif; text-align:center; margin-top:30px; background-color: #fafafa;}";
    html += "h1{color:#E6A817;} .meta{color:#888; font-size:16px;}</style></head>";
    html += "<body><h1>Beeyard Phase 0</h1>";
    html += "<p class='meta'>Sensor ID: <b>" + SENSOR_ID + "</b></p><hr>";
    html += "<h2>Temp: " + String(temp, 1) + " &deg;C</h2>";
    html += "<h2>Hum: " + String(humidity, 1) + " %</h2>";
    html += "<h2>Press: " + String(pressure, 1) + " hPa</h2></body></html>";
    server.send(200, "text/html", html);
  }
#endif

#if USE_BLUETOOTH
  #include <BLEDevice.h>
  #include <BLEServer.h>
  #define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
  #define ID_CHAR_UUID        "11111111-1fb5-459e-8fcc-c5c9c331914b" // ID Slot
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
  //while (!Serial) { delay(10); }

  Serial.println("\n--- Starting Beeyard Phase 0 ---");
  Serial.println("Sensor ID: " + SENSOR_ID);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!bme.begin(0x76, &Wire)) {
    Serial.println("Error: Could not find BME280 at 0x76!");
  } else {
    Serial.println("BME280 (0x76) Initialized.");
  }

  #if USE_WIFI
    Serial.print("Starting WiFi... Connecting to "); Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected!");
    
    Serial.print("Visit IP in your browser: "); 
    Serial.println(WiFi.localIP());
    server.on("/", handleRoot);
    server.begin();
  #endif

  #if USE_BLUETOOTH
    Serial.println("Starting BLE Beacon... (Name: Beeyard_Hive_1)");
    BLEDevice::init("Beeyard_Hive_1");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    // Create the data slots
    pIdChar = pService->createCharacteristic(ID_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pTempChar = pService->createCharacteristic(TEMP_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pHumChar = pService->createCharacteristic(HUM_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pPressChar = pService->createCharacteristic(PRESS_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    
    // Set the static Sensor ID immediately
    pIdChar->setValue(SENSOR_ID.c_str());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("BLE Active. Open your scanner app.");
  #endif
}

void loop() {
  #if USE_WIFI
    server.handleClient();
  #endif

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    temp = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;

    // Print to Serial Monitor with the ID tag
    Serial.print("[" + SENSOR_ID + "] ");
    Serial.print("T: "); Serial.print(temp, 1);
    Serial.print("C | H: "); Serial.print(humidity, 1);
    Serial.print("% | P: "); Serial.print(pressure, 1); Serial.println("hPa");

    // Update Bluetooth Environment Characteristics
    #if USE_BLUETOOTH
      pTempChar->setValue((String(temp, 1) + " C").c_str());
      pHumChar->setValue((String(humidity, 1) + " %").c_str());
      pPressChar->setValue((String(pressure, 1) + " hPa").c_str());
    #endif
  }
}