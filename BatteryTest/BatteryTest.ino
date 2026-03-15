/*
 * Battery ADC Pin Finder for XIAO ESP32C3
 * Reads all ADC pins and prints millivolts to find
 * which one is connected to the battery voltage divider.
 */

void setup() {
  Serial.begin(115200);
  delay(3000);  // Wait for serial monitor
  Serial.println("\n--- Battery ADC Pin Test ---");

  // Set max attenuation for full voltage range
  analogSetAttenuation(ADC_11db);
}

void loop() {
  // Try all ADC-capable pins
  Serial.printf("A0(GPIO2): %4d mV  |  ", analogReadMilliVolts(A0));
  Serial.printf("A1(GPIO3): %4d mV  |  ", analogReadMilliVolts(A1));
  Serial.printf("A2(GPIO4): %4d mV  |  ", analogReadMilliVolts(A2));

  // Also try raw reads
  Serial.printf("  RAW A0:%4d A1:%4d A2:%4d\n",
    analogRead(A0), analogRead(A1), analogRead(A2));

  delay(1000);
}
