#include "IdleSleep.h"
#include "../config.h"
#include "RuntimeHealth.h"
#include "SleepLogger.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace {
bool checkResult(const char *operation, esp_err_t error) {
  if (error == ESP_OK) {
    return true;
  }
  static uint32_t lastLogMs = 0;
  uint32_t nowMs = millis();
  if (lastLogMs == 0 || nowMs - lastLogMs >= 5000UL) {
    Serial.printf("[Sleep] %s failed: %s; staying awake\n", operation,
                  esp_err_to_name(error));
    lastLogMs = nowMs;
  }
  return false;
}

bool configureWakeup(uint64_t durationUs) {
  if (!checkResult("disable old wakeup sources",
                   esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL)) ||
      !checkResult("enable timer wakeup",
                   esp_sleep_enable_timer_wakeup(durationUs))) {
    return false;
  }
  const uint8_t pins[] = {KEY_LEFT, KEY_RIGHT, KEY_ENTER};
  for (uint8_t pin : pins) {
    char operation[40];
    snprintf(operation, sizeof(operation), "enable button GPIO%u wakeup", pin);
    if (!checkResult(operation,
                     gpio_wakeup_enable(static_cast<gpio_num_t>(pin),
                                        GPIO_INTR_LOW_LEVEL))) {
      return false;
    }
  }
  return checkResult("enable GPIO wakeup source", esp_sleep_enable_gpio_wakeup());
}
} // namespace

namespace IdleSleep {
bool enter(uint64_t durationUs) {
  // 关键逻辑：任何唤醒源配置失败都不能继续睡眠，否则设备可能只剩
  // 屏幕上的旧画面，且所有按键都无法唤醒；保留主循环继续处理输入。
  if (!configureWakeup(durationUs)) {
    return false;
  }
  SleepLogger::logEnterLightSleep();
  RuntimeHealth::prepareSleep(durationUs);
  esp_err_t error = esp_light_sleep_start();
  RuntimeHealth::progress();
  if (!checkResult("enter light sleep", error)) {
    return false;
  }
  SleepLogger::logWakeFromLightSleep(esp_sleep_get_wakeup_cause());
  return true;
}
} // namespace IdleSleep
