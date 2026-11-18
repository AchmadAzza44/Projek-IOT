#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  Serial.println("\nUNIB Lab-2 I2C Scanner");
  Serial.println("SDA=GPIO21, SCL=GPIO22");
  uint8_t found = 0;

  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("I2C device found at 0x%02X\n", address);
      ++found;
    }
    delay(2);
  }

  Serial.printf("Scan complete: %u device(s)\n", found);
}

void loop() {}
