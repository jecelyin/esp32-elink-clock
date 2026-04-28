#pragma once

#include <esp_sleep.h>

namespace SleepLogger {
void logEnterLightSleep();
void logWakeFromLightSleep(esp_sleep_wakeup_cause_t wakeupCause);
} // namespace SleepLogger
