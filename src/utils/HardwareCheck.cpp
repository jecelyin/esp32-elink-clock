#include "HardwareCheck.h"
#include "../config.h"
#include "I2CBus.h"

namespace HardwareCheck {

const Device DEVICES[DEVICE_COUNT] = {
    {"RTC (RX8010SJ)", "RTC", RX8010_I2C_ADDR, false},
    {"Sensor (SHT30)", "Sensor", SHT30_I2C_ADDR, false},
    {"Audio (ES8311)", "Audio", ES8311_ADDRESS, false},
    {"Radio (RDA5807)", "Radio", M5807_ADDR_FULL_ACCESS, false},
    {"Battery (CW2015)", "Battery", CW2015_I2C_ADDR, true},
};

void setPowerEnabled(bool enabled) {
  if (enabled) {
    // 关键逻辑：ES8311 电源域提供共享 I2C 上拉，RDA5807 还需要独立打开
    // 收音机电源，否则硬件检测会把供电问题误判为器件缺失。
    digitalWrite(CODEC_EN, HIGH);
    digitalWrite(RADIO_EN, HIGH);
    delay(20);
    return;
  }

  digitalWrite(RADIO_EN, LOW);
}

bool checkDevice(const Device &device, BatteryDriver *battery) {
  if (device.fuelGauge) {
    return battery != nullptr && battery->checkFuelGauge();
  }

  return I2CBus::probeDevice(device.address);
}

} // namespace HardwareCheck
