#include "RtcDriver.h"
#include "../utils/I2CBus.h"

namespace {
constexpr uint32_t RTC_RETRY_DELAY_MS = 1000UL;
constexpr uint32_t RTC_RETRY_MAX_DELAY_MS = 30000UL;
constexpr uint32_t RTC_ERROR_LOG_INTERVAL_MS = 3000UL;
constexpr uint32_t RTC_I2C_LOCK_TIMEOUT_MS = 200UL;
constexpr uint32_t RTC_RECOVERY_DELAY_MS = 20UL;
constexpr uint8_t RTC_I2C_MAX_ATTEMPTS = 2;
constexpr uint8_t RTC_RETRY_MAX_SHIFT = 5;
const DateTime RTC_DEFAULT_TIME = {0, 0, 0, 1, 1, 0, 6};
} // namespace

uint8_t RtcDriver::decToBcd(uint8_t val) {
  return ((val / 10 * 16) + (val % 10));
}

uint8_t RtcDriver::bcdToDec(uint8_t val) {
  return ((val / 16 * 10) + (val % 16));
}

bool RtcDriver::canRetryAccess() const {
  return _lastFailureTime == 0 ||
         millis() - _lastFailureTime >= getRetryDelay();
}

void RtcDriver::drainWire() {
  while (Wire.available()) {
    Wire.read();
  }
}

DateTime RtcDriver::getFallbackTime() const { return getSoftwareTime(); }

DateTime RtcDriver::calculateSoftwareTime() const {
  time_t base = toTimeT(_softBaseTime);
  uint32_t elapsed = (millis() - _softBaseMillis) / 1000UL;
  return fromTimeT(base + elapsed);
}

DateTime RtcDriver::fromTimeT(time_t value) const {
  struct tm t = {0};
  if (localtime_r(&value, &t) == nullptr)
    return RTC_DEFAULT_TIME;

  DateTime dt = RTC_DEFAULT_TIME;
  dt.second = t.tm_sec;
  dt.minute = t.tm_min;
  dt.hour = t.tm_hour;
  dt.day = t.tm_mday;
  dt.month = t.tm_mon + 1;
  dt.year = (t.tm_year + 1900) % 100;
  dt.week = t.tm_wday;
  return dt;
}

uint32_t RtcDriver::getRetryDelay() const {
  uint8_t shift =
      _failureCount > RTC_RETRY_MAX_SHIFT ? RTC_RETRY_MAX_SHIFT : _failureCount;
  uint32_t delayMs = RTC_RETRY_DELAY_MS << shift;
  return delayMs > RTC_RETRY_MAX_DELAY_MS ? RTC_RETRY_MAX_DELAY_MS : delayMs;
}

time_t RtcDriver::toTimeT(const DateTime &dt) const {
  struct tm t = {0};
  t.tm_sec = dt.second;
  t.tm_min = dt.minute;
  t.tm_hour = dt.hour;
  t.tm_mday = dt.day;
  t.tm_mon = dt.month - 1;
  t.tm_year = dt.year + 100;
  t.tm_isdst = -1;
  return mktime(&t);
}

bool RtcDriver::readBytes(uint8_t reg, uint8_t *buffer, size_t length) {
  if (!canRetryAccess())
    return false;
  for (uint8_t attempt = 0; attempt < RTC_I2C_MAX_ATTEMPTS; ++attempt) {
    if (tryReadBytes(reg, buffer, length))
      return true;
    if (!recoverAfterFailedAttempt(attempt))
      return false;
  }
  return false;
}

bool RtcDriver::readRegister(uint8_t reg, uint8_t &value) {
  uint8_t buffer = 0;
  if (!readBytes(reg, &buffer, 1))
    return false;
  value = buffer;
  return true;
}

bool RtcDriver::recoverAfterFailedAttempt(uint8_t attempt) {
  if (attempt + 1 >= RTC_I2C_MAX_ATTEMPTS)
    return false;

  // 关键逻辑：一次 Wire 超时后立即主动恢复总线，并清掉节流时间；
  // 否则同一轮 NTP 写 RTC 会被 1 秒重试窗口挡住，表现为时间一直不更新。
  I2CBus::recover();
  _lastFailureTime = 0;
  delay(RTC_RECOVERY_DELAY_MS);
  return true;
}

void RtcDriver::seedSoftwareClock(const DateTime &dt) {
  _softBaseTime = dt;
  _softBaseMillis = millis();
  _hasSoftwareTime = true;
}

bool RtcDriver::tryReadBytes(uint8_t reg, uint8_t *buffer, size_t length) {
  I2CBus::Guard guard(RTC_I2C_LOCK_TIMEOUT_MS);
  if (!guard.isLocked()) {
    markBusFailure("lock", -1);
    return false;
  }

  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  // RX8010 在当前板级连线下对 repeated-start 不稳定，必须显式发送 STOP，
  // 否则 Wire 会走 i2cWriteReadNonStop 路径并触发 Error 263。
  int txError = Wire.endTransmission(true);
  if (txError != 0) {
    markBusFailure("write address", txError);
    return false;
  }

  size_t received = Wire.requestFrom((uint8_t)RX8010_I2C_ADDR,
                                     (uint8_t)length, true);
  if (received != length) {
    drainWire();
    markBusFailure("read data", static_cast<int>(received));
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  _lastFailureTime = 0;
  return true;
}

bool RtcDriver::writeBytes(uint8_t reg, const uint8_t *buffer, size_t length,
                           bool ignoreRetry) {
  if (!ignoreRetry && !canRetryAccess())
    return false;
  for (uint8_t attempt = 0; attempt < RTC_I2C_MAX_ATTEMPTS; ++attempt) {
    if (tryWriteBytes(reg, buffer, length))
      return true;
    if (!recoverAfterFailedAttempt(attempt))
      return false;
  }
  return false;
}

bool RtcDriver::tryWriteBytes(uint8_t reg, const uint8_t *buffer,
                              size_t length) {
  I2CBus::Guard guard(RTC_I2C_LOCK_TIMEOUT_MS);
  if (!guard.isLocked()) {
    markBusFailure("lock", -1);
    return false;
  }

  Wire.beginTransmission(RX8010_I2C_ADDR);
  Wire.write(reg);
  for (size_t i = 0; i < length; ++i) {
    Wire.write(buffer[i]);
  }
  int txError = Wire.endTransmission(true);
  if (txError != 0) {
    markBusFailure("write data", txError);
    return false;
  }
  _lastFailureTime = 0;
  return true;
}

bool RtcDriver::writeRegister(uint8_t reg, uint8_t val, bool ignoreRetry) {
  return writeBytes(reg, &val, 1, ignoreRetry);
}

void RtcDriver::markBusFailure(const char *operation, int code) {
  uint32_t now = millis();
  _lastFailureTime = now;
  if (_failureCount < RTC_RETRY_MAX_SHIFT) {
    _failureCount++;
  }
  // 失败后短时间内直接返回缓存，避免 UI 一帧内多次触发相同的 Wire
  // 回溯，把一次硬件异常放大成整屏日志风暴。
  if (now - _lastErrorLogTime >= RTC_ERROR_LOG_INTERVAL_MS ||
      _lastErrorLogTime == 0) {
    Serial.printf("RTC I2C %s failed, code=%d\n", operation, code);
    _lastErrorLogTime = now;
  }
}

void RtcDriver::markReadSuccess(const DateTime &dt) {
  _cachedTime = dt;
  _hasCachedTime = true;
  _lastReadTime = millis();
  _lastFailureTime = 0;
  _failureCount = 0;
  seedSoftwareClock(dt);
}

uint8_t RtcDriver::weekBinToBitmask(uint8_t week0to6) {
  return (1 << week0to6);
}

uint8_t RtcDriver::weekBitmaskToBin(uint8_t bitmask) {
  for (uint8_t i = 0; i < 7; i++) {
    if ((bitmask >> i) & 0x01) {
      return i;
    }
  }
  return 0;
}
