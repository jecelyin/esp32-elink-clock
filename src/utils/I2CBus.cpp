#include "I2CBus.h"
#include "../config.h"
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
constexpr uint16_t I2C_TIMEOUT_MS = 100;
constexpr uint8_t I2C_RECOVERY_PULSES = 9;
constexpr uint32_t I2C_POWER_STABILIZE_MS = 2;
SemaphoreHandle_t i2cMutex = nullptr;
bool i2cStarted = false;
bool sharedBusPowered = false;

void ensureMutex() {
  if (i2cMutex == nullptr) {
    i2cMutex = xSemaphoreCreateMutex();
  }
}

void configureWire() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_CLOCK_HZ);
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  i2cStarted = true;
}

void ensureSharedBusPower() {
  if (sharedBusPowered)
    return;

  // 关键逻辑：当前硬件的 I2C 上拉与 ES8311 电源域耦合。
  // 如果 CODEC_EN 被拉低，RTC/SHT30 虽然没断电，也会因为总线失去上拉
  // 或被未上电器件拖住而触发 I2C timeout(code=5)。
  digitalWrite(CODEC_EN, HIGH);
  delay(I2C_POWER_STABILIZE_MS);
  sharedBusPowered = true;
}

void shutdownSharedBusPower() {
  if (!sharedBusPowered)
    return;
  digitalWrite(CODEC_EN, LOW);
  sharedBusPowered = false;
}

bool takeMutex(uint32_t timeoutMs) {
  ensureMutex();
  if (i2cMutex == nullptr)
    return false;
  TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
  return xSemaphoreTake(i2cMutex, ticks) == pdTRUE;
}

void releaseLines() {
  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  delayMicroseconds(10);
}

void pulseClock() {
  pinMode(I2C_SCL, OUTPUT_OPEN_DRAIN);
  for (uint8_t i = 0; i < I2C_RECOVERY_PULSES && digitalRead(I2C_SDA) == LOW;
       ++i) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(8);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(8);
  }
}

void sendStopCondition() {
  pinMode(I2C_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(8);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(8);
  digitalWrite(I2C_SDA, HIGH);
  delayMicroseconds(8);
}
} // namespace

namespace I2CBus {

void begin() {
  ensureMutex();
  if (i2cStarted)
    return;
  configureWire();
}

bool lock(uint32_t timeoutMs) {
  if (!takeMutex(timeoutMs))
    return false;

  ensureSharedBusPower();
  if (!i2cStarted) {
    configureWire();
  }
  return true;
}

void unlock() {
  if (i2cMutex != nullptr) {
    xSemaphoreGive(i2cMutex);
  }
}

bool recover() {
  if (!lock())
    return false;

  // 关键逻辑：ESP32 Wire 超时后硬件控制器可能仍保持 busy 状态；
  // 先释放控制器，再用 9 个 SCL 脉冲释放被从机拉低的 SDA，最后重建 Wire。
  Serial.println("I2C bus recovery starting");
  Wire.end();
  i2cStarted = false;
  releaseLines();
  pulseClock();
  sendStopCondition();
  releaseLines();
  configureWire();
  unlock();
  return i2cStarted;
}

bool powerDownSharedBus(uint32_t timeoutMs) {
  if (!takeMutex(timeoutMs))
    return false;

  // 关键逻辑：所有关闭共享 I2C 电源域的动作都必须先拿到总线锁，
  // 避免另一核正在访问 RTC/SHT30 时被突然断电，造成随机 timeout。
  shutdownSharedBusPower();
  unlock();
  return true;
}

Guard::Guard(uint32_t timeoutMs) { locked = lock(timeoutMs); }

Guard::~Guard() {
  if (locked) {
    unlock();
  }
}

bool Guard::isLocked() const { return locked; }

} // namespace I2CBus
