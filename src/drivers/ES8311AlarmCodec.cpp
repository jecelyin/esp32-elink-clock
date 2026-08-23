#include "ES8311AlarmCodec.h"
#include "../config.h"
#include "../utils/I2CBus.h"
#include <Wire.h>

namespace {
constexpr uint8_t CODEC_I2C_ADDRESS = ES8311_ADDRESS;
constexpr uint8_t MAX_WRITE_ATTEMPTS = 3;

struct RegisterValue {
  uint8_t reg;
  uint8_t value;
};

bool writeRegisterLocked(uint8_t reg, uint8_t value) {
  for (uint8_t attempt = 1; attempt <= MAX_WRITE_ATTEMPTS; ++attempt) {
    Wire.beginTransmission(CODEC_I2C_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    uint8_t error = Wire.endTransmission(true);
    if (error == 0) {
      return true;
    }
    if (attempt == MAX_WRITE_ATTEMPTS) {
      Serial.printf("[Alarm][ES8311] write failed reg=0x%02X value=0x%02X "
                    "error=%u\n",
                    reg, value, error);
      return false;
    }
    delay(5);
  }
  return false;
}

bool readRegisterLocked(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(CODEC_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(static_cast<uint16_t>(CODEC_I2C_ADDRESS),
                       static_cast<size_t>(1), true) != 1 ||
      !Wire.available()) {
    return false;
  }
  value = Wire.read();
  return true;
}

void logStateLocked() {
  static const uint8_t registers[] = {0x00, 0x01, 0x02, 0x06, 0x09,
                                      0x0D, 0x12, 0x13, 0x31, 0x32};
  Serial.print("[Alarm][ES8311]");
  for (uint8_t reg : registers) {
    uint8_t value = 0;
    if (readRegisterLocked(reg, value)) {
      Serial.printf(" %02X=%02X", reg, value);
    } else {
      Serial.printf(" %02X=??", reg);
    }
  }
  Serial.println();
}
} // namespace

namespace ES8311AlarmCodec {

bool begin() {
  // CODEC_EN controls the 3.3V_codec rail. The I2S owner has already enabled
  // its clock, but the analog supply needs time before the first I2C write.
  digitalWrite(CODEC_EN, HIGH);
  delay(30);

  I2CBus::Guard guard;
  if (!guard.isLocked()) {
    Serial.println("[Alarm][ES8311] I2C lock failed");
    return false;
  }

  Wire.beginTransmission(CODEC_I2C_ADDRESS);
  if (Wire.endTransmission(true) != 0) {
    Serial.println("[Alarm][ES8311] codec not found");
    return false;
  }

  // The netlist leaves MCLK unconnected. ESP32 emits a 16-bit stereo frame
  // (32 BCLK per LRCK), so ES8311 derives its 256-fs internal clock by
  // multiplying BCLK by eight.
  if (!writeRegisterLocked(0x00, 0x1F)) {
    return false;
  }
  delay(20);
  if (!writeRegisterLocked(0x00, 0x00) ||
      !writeRegisterLocked(0x00, 0x80)) {
    return false;
  }

  static const RegisterValue initSequence[] = {
      {0x01, 0xBF}, // Use BCLK as clock source and enable codec clocks
      {0x02, 0x18}, // BCLK x8
      {0x03, 0x10},
      {0x04, 0x10},
      {0x05, 0x00},
      {0x06, 0x03},
      {0x07, 0x00},
      {0x08, 0xFF},
      {0x09, 0x0C}, // Slave, standard I2S, 16-bit input
      {0x0A, 0x0C},
      {0x0D, 0x01}, // Power up analog circuitry
      {0x0E, 0x02},
      {0x12, 0x00}, // Power up DAC
      {0x13, 0x10}, // Enable differential headphone output
      {0x1C, 0x6A},
      {0x37, 0x08}, // Bypass DAC equalizer
      {0x31, 0x00}, // Unmute DAC
      // The normalized alarm peaks at about -2.2 dBFS. 0xC3 applies about
      // +2 dB, giving the loudest clean codec output without hard clipping.
      {0x32, 0xC3},
  };

  for (const RegisterValue &item : initSequence) {
    if (!writeRegisterLocked(item.reg, item.value)) {
      return false;
    }
  }

  delay(20);
  logStateLocked();
  Serial.println("[Alarm][ES8311] DAC ready");
  return true;
}

} // namespace ES8311AlarmCodec
