#include "BusManager.h"
#include <esp_debug_helpers.h>

void BusManager::begin() { currentMode = MODE_NONE; }

void BusManager::requestDisplay() {
  isLocked = true;

  if (currentMode == MODE_DISPLAY) {
    return;
  }

  if (currentMode == MODE_I2C) {
    Wire.end();
  }

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  currentMode = MODE_DISPLAY;
  Serial.println("BusManager: Switched to Display (SPI)");
}

void BusManager::requestI2C() {
  isLocked = true;

  if (currentMode == MODE_I2C) {
    return;
  }
  if (currentMode == MODE_DISPLAY) {
    SPI.end();
  }

  Wire.begin(I2C_SDA, I2C_SCL);

  currentMode = MODE_I2C;
  Serial.println("BusManager: Switched to I2C");
}

void BusManager::releaseBus() { isLocked = false; }
