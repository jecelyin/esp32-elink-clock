#pragma once

#include "../config.h"
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// BusManager handles the arbitration between VSPI (Display) and I2C devices
// which share GPIO 18 (SCK/SDA) and GPIO 23 (MOSI/SCL).
class BusManager {
public:
  static BusManager &getInstance() {
    static BusManager instance;
    return instance;
  }

  void begin();
  void requestDisplay();
  void requestI2C();

  void lock();
  void unlock();

private:
  BusManager() {}
  BusManager(const BusManager &) = delete;
  BusManager &operator=(const BusManager &) = delete;

  SemaphoreHandle_t busMutex;

  enum BusMode { MODE_NONE, MODE_DISPLAY, MODE_I2C };

  BusMode currentMode = MODE_NONE;
};
