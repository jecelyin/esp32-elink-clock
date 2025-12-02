#include "RtcDriver.h"

RtcDriver::RtcDriver() {}

bool RtcDriver::init() {
  Wire.begin(I2C_SDA, I2C_SCL);
  // Check if device is present
  Wire.beginTransmission(RX8010_I2C_ADDR);
  return (Wire.endTransmission() == 0);
}

DateTime RtcDriver::getTime() {
  DateTime dt = {0};
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(0x10); // Start reading from seconds register
  Wire.endTransmission();

  Wire.requestFrom(RX8010_I2C_ADDR, (uint8_t)7);
  if (Wire.available() == 7) {
    dt.second = bcd2bin(Wire.read() & 0x7F);
    dt.minute = bcd2bin(Wire.read() & 0x7F);
    dt.hour = bcd2bin(Wire.read() & 0x3F);
    dt.week = Wire.read() & 0x07; // Weekday
    dt.day = bcd2bin(Wire.read() & 0x3F);
    dt.month = bcd2bin(Wire.read() & 0x1F);
    dt.year = bcd2bin(Wire.read());
  }
  return dt;
}

void RtcDriver::setTime(DateTime dt) {
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(0x10); // Start writing from seconds register
  Wire.write(bin2bcd(dt.second));
  Wire.write(bin2bcd(dt.minute));
  Wire.write(bin2bcd(dt.hour));
  Wire.write(dt.week);
  Wire.write(bin2bcd(dt.day));
  Wire.write(bin2bcd(dt.month));
  Wire.write(bin2bcd(dt.year));
  Wire.endTransmission();
}

uint8_t RtcDriver::bcd2bin(uint8_t val) {
  return (val >> 4) * 10 + (val & 0x0F);
}

uint8_t RtcDriver::bin2bcd(uint8_t val) {
  return ((val / 10) << 4) + (val % 10);
}

void RtcDriver::writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t RtcDriver::readReg(uint8_t reg) {
  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(RX8010_I2C_ADDR, (uint8_t)1);
  return Wire.read();
}
