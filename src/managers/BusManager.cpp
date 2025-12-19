#include "BusManager.h"

void BusManager::begin() {
  currentMode = MODE_NONE;
  busMutex = xSemaphoreCreateRecursiveMutex();
}

void BusManager::lock() {
  if (busMutex)
    xSemaphoreTakeRecursive(busMutex, portMAX_DELAY);
}

void BusManager::unlock() {
  if (busMutex)
    xSemaphoreGiveRecursive(busMutex);
}

void BusManager::requestDisplay() {
  lock(); // This ensures no other task is using the bus
  if (currentMode == MODE_DISPLAY) {
    unlock();
    return;
  }
  digitalWrite(CODEC_EN, LOW);

  if (currentMode == MODE_I2C) {
    Wire.end();
  }

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  currentMode = MODE_DISPLAY;
  Serial.println("BusManager: Switched to Display (SPI)");
  unlock();
}

void BusManager::requestI2C() {
  lock();
  if (currentMode == MODE_I2C) {
    unlock();
    return;
  }
  digitalWrite(CODEC_EN, HIGH);
  if (currentMode == MODE_DISPLAY) {
    SPI.end();
  }

  Wire.begin(I2C_SDA, I2C_SCL);

  currentMode = MODE_I2C;
  Serial.println("BusManager: Switched to I2C");
  unlock();
}
