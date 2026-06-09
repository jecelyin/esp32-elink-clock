#pragma once

#include "../drivers/RtcDriver.h"
#include <Arduino.h>

namespace WakeTiming {
inline uint32_t getRemainingIntervalMs(uint32_t lastMs, uint32_t intervalMs,
                                       uint32_t nowMs) {
  if (intervalMs == 0) {
    return 0;
  }

  if (lastMs == 0 || nowMs - lastMs >= intervalMs) {
    return 0;
  }

  return intervalMs - (nowMs - lastMs);
}

inline uint32_t getMsUntilNextSecondBoundary(uint32_t nowMs) {
  uint32_t msIntoSecond = nowMs % 1000UL;
  return msIntoSecond == 0 ? 1000UL : 1000UL - msIntoSecond;
}

inline uint32_t getMsUntilNextMinuteBoundary(const DateTime &now,
                                             uint32_t nowMs) {
  // 关键逻辑：RTC 秒值必须和 millis() 的子秒偏移一起参与计算，
  // 才能把唤醒点精确对齐到下一分钟边界，而不是每次都晚几秒刷新。
  return (59UL - now.second) * 1000UL + getMsUntilNextSecondBoundary(nowMs);
}
} // namespace WakeTiming
