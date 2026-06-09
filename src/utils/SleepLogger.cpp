#include "SleepLogger.h"
#include <Arduino.h>

namespace {
const char *toWakeupCauseName(esp_sleep_wakeup_cause_t wakeupCause) {
  switch (wakeupCause) {
  case ESP_SLEEP_WAKEUP_TIMER:
    return "timer";
  case ESP_SLEEP_WAKEUP_GPIO:
    return "gpio";
  case ESP_SLEEP_WAKEUP_UNDEFINED:
    return "undefined";
  default:
    return "other";
  }
}
} // namespace

namespace SleepLogger {
void logEnterLightSleep() {
#if ENABLE_SERIAL_DEBUG
  // 关键逻辑：轻睡眠前必须先刷出串口缓冲，
  // 否则日志可能还没真正发完，CPU 就已经进入轻睡眠。
  Serial.printf("[Sleep] Enter light sleep at %lums\n", millis());
  Serial.flush();
#endif
}

void logWakeFromLightSleep(esp_sleep_wakeup_cause_t wakeupCause) {
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Sleep] Exit light sleep at %lums, cause=%s(%d)\n", millis(),
                toWakeupCauseName(wakeupCause), (int)wakeupCause);
#endif
}
} // namespace SleepLogger
