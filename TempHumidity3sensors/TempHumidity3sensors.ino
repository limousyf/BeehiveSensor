#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// 1. Create the SHT41 object
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// 2. Create TWO separate BME280 objects
Adafruit_BME280 bme_generic;  // For the 0x76 board
Adafruit_BME280 bme_adafruit; // For the 0x77 board

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } 

  Serial.println("\n--- Beeyard Triple I2C Sensor Test ---");

  // Initialize SHT41 (0x44)
  if (!sht4.begin()) {
    Serial.println("Error: Could not find SHT41 (0x44)!");
    while (1) delay(1);
  }
  sht4.setPrecision(SHT4X_HIGH_PRECISION);

  // Initialize Generic BME280 (0x76)
  if (!bme_generic.begin(0x76)) {   
    Serial.println("Error: Could not find Generic BME280 (0x76)!");
    while (1) delay(1);
  }

  // Initialize Adafruit BME280 (0x77)
  if (!bme_adafruit.begin(0x77)) {   
    Serial.println("Error: Could not find Adafruit BME280 (0x77)!");
    while (1) delay(1);
  }
  
  Serial.println("All sensors initialized. Starting data stream...\n");
}

void loop() {
  // --- Read SHT41 ---
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);

  // --- Read Generic BME280 (0x76) ---
  float genTemp = bme_generic.readTemperature();
  float genHum = bme_generic.readHumidity();
  float genPress = bme_generic.readPressure() / 100.0F;

  // --- Read Adafruit BME280 (0x77) ---
  float adaTemp = bme_adafruit.readTemperature();
  float adaHum = bme_adafruit.readHumidity();
  float adaPress = bme_adafruit.readPressure() / 100.0F;

  // --- Print All Data on a Single Line ---
  
  // SHT41 Print
  Serial.print("[SHT41] T:"); Serial.print(temp.temperature, 1); 
  Serial.print("C H:"); Serial.print(humidity.relative_humidity, 1); 
  Serial.print("%  |  ");

  // Generic BME280 Print
  Serial.print("[BME-76] T:"); Serial.print(genTemp, 1); 
  Serial.print("C H:"); Serial.print(genHum, 1); 
  Serial.print("% P:"); Serial.print(genPress, 1); 
  Serial.print("hPa  |  ");

  // Adafruit BME280 Print
  Serial.print("[BME-77] T:"); Serial.print(adaTemp, 1); 
  Serial.print("C H:"); Serial.print(adaHum, 1); 
  Serial.print("% P:"); Serial.print(adaPress, 1); 
  Serial.println("hPa"); // The println creates the new line for the next loop

  delay(2000); 
}