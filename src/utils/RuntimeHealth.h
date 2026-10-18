#pragma once

#include <stdint.h>

namespace RuntimeHealth {
enum class Stage : uint8_t {
  Starting,
  ScheduledTasks,
  RtcSync,
  UiUpdate,
  Audio,
  Alarm,
  Input,
  Power,
  Idle,
  Sleeping,
  Count
};

// 关键逻辑：仅在主循环真正推进时刷新期限。120 秒覆盖正常联网、刷屏等
// 有限阻塞；搜台和文件传输通过实际处理进度续期，不限制操作的总时长。
constexpr int64_t ACTIVE_TIMEOUT_US = 120000000LL;
void reportBoot();
bool begin();
void mark(Stage stage, uint8_t screen);
void progress();
void prepareSleep(uint64_t durationUs);
} // namespace RuntimeHealth
