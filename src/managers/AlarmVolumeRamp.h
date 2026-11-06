#pragma once

#include "AlarmSoundSettings.h"

// 闹钟独占的渐强包络；仅新闹钟重置，铃声文件循环时保持当前进度。
class AlarmVolumeRamp {
public:
  static constexpr uint16_t UNITY_GAIN = 32768;

  void reset(const AlarmSoundSettings &value) {
    settings = value.isValid() ? value : AlarmSoundSettings();
    started = false;
  }

  void start(uint32_t nowMs) {
    if (!started) {
      startedAtMs = nowMs;
      started = true;
    }
  }

  uint8_t volume(uint32_t nowMs) const {
    return static_cast<uint8_t>(volumeProgress(nowMs) / UNITY_GAIN);
  }

  uint16_t gain(uint32_t nowMs) const {
    // 与现有 ESP32-audioI2S 的 0～21 档数字增益一致，并在相邻档位间
    // 插值，让直通 PCM 在每一小块采样中也平滑增大，最大档保持原始采样。
    static const uint8_t levels[] = {0, 1, 2, 3, 4, 6, 8, 10, 12, 14, 17,
                                    20, 23, 27, 30, 34, 38, 43, 48, 52, 58, 64};
    uint32_t progress = volumeProgress(nowMs);
    uint8_t level = progress / UNITY_GAIN;
    uint16_t low = levels[level] * 512U;
    if (level == AlarmSoundSettings::MAX_VOLUME) {
      return low;
    }
    uint16_t difference = (levels[level + 1] - levels[level]) * 512U;
    return low + difference * (progress % UNITY_GAIN) / UNITY_GAIN;
  }

private:
  uint32_t volumeProgress(uint32_t nowMs) const {
    uint32_t duration = settings.fadeInSeconds * 1000UL;
    uint32_t elapsed = started ? nowMs - startedAtMs : 0;
    if (duration == 0 || elapsed >= duration) {
      return settings.volume * UNITY_GAIN;
    }
    // 无符号时间差兼容 millis() 回绕；先做 64 位乘法，避免 60 秒溢出。
    return static_cast<uint64_t>(settings.volume) * UNITY_GAIN * elapsed /
           duration;
  }

  AlarmSoundSettings settings;
  uint32_t startedAtMs = 0;
  bool started = false;
};
