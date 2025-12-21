#include "BusManager.h"
#include <esp_debug_helpers.h>

void BusManager::begin() { currentMode = MODE_NONE; }

void BusManager::requestDisplay() {
  if (currentMode == MODE_DISPLAY) {
    return;
  }

  if (currentMode == MODE_I2C) {
    Wire.end();
  }

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  currentMode = MODE_DISPLAY;
  Serial.println("BusManager: Switched to Display (SPI)");
  // esp_backtrace_print(100);
}

void BusManager::requestI2C() {
  if (currentMode == MODE_I2C) {
    return;
  }
  if (currentMode == MODE_DISPLAY) {
    SPI.end();
  }

  Wire.begin(I2C_SDA, I2C_SCL);

  currentMode = MODE_I2C;
  Serial.println("BusManager: Switched to I2C");
  // esp_backtrace_print(100);
}
