#include "../../src/managers/AlarmVolumeRamp.h"
#include <cassert>
#include <iostream>

void testFade() {
  AlarmVolumeRamp ramp;
  AlarmSoundSettings settings;
  ramp.reset(settings);
  assert(ramp.gain(5000) == 0); // 尚未成功开始播放，不消耗渐强时间。
  ramp.start(5000);
  assert(ramp.volume(5000) == 0 && ramp.gain(5000) == 0);
  uint16_t previous = 0;
  for (uint32_t elapsed = 0; elapsed <= 3000; ++elapsed) {
    uint16_t gain = ramp.gain(5000 + elapsed);
    assert(gain >= previous && gain <= AlarmVolumeRamp::UNITY_GAIN);
    previous = gain;
  }
  assert(ramp.volume(6500) == 10);
  assert(ramp.volume(8000) == 21);
  assert(ramp.gain(8000) == AlarmVolumeRamp::UNITY_GAIN);
  assert(ramp.gain(20000) == AlarmVolumeRamp::UNITY_GAIN);
}

void testLifecycle() {
  AlarmVolumeRamp ramp;
  AlarmSoundSettings settings;
  settings.volume = 10;
  ramp.reset(settings);
  ramp.start(100);
  uint16_t halfway = ramp.gain(1600);
  ramp.start(1600); // 同次响铃的文件循环和重试不能重新渐强。
  assert(ramp.gain(1600) == halfway);
  assert(ramp.gain(3100) == 17 * 512);
  settings.volume = 21; // 已开始的响铃使用设置快照。
  assert(ramp.gain(4000) == 17 * 512);
  ramp.reset(settings);
  assert(ramp.gain(5000) == 0);
  ramp.start(5000);
  assert(ramp.gain(5000) == 0);
  assert(ramp.gain(8000) == AlarmVolumeRamp::UNITY_GAIN);
}

void testBoundaries() {
  AlarmSoundSettings settings;
  AlarmVolumeRamp ramp;
  settings.volume = 0;
  ramp.reset(settings);
  ramp.start(0);
  assert(ramp.gain(0) == 0 && ramp.gain(60000) == 0);
  settings.volume = 1;
  settings.fadeInSeconds = 0;
  ramp.reset(settings);
  assert(ramp.volume(0) == 1 && ramp.gain(0) == 512);
  settings.fadeInSeconds = 60;
  ramp.reset(settings);
  ramp.start(0);
  assert(ramp.gain(59999) < 512 && ramp.gain(60000) == 512);
  settings.volume = 255;
  ramp.reset(settings);
  ramp.start(0);
  assert(ramp.gain(3000) == AlarmVolumeRamp::UNITY_GAIN);
}

void testClockWrap() {
  AlarmVolumeRamp ramp;
  ramp.reset(AlarmSoundSettings());
  const uint32_t start = UINT32_MAX - 1000;
  ramp.start(start);
  assert(ramp.volume(start + 1500U) == 10);
  assert(ramp.gain(start + 3000U) == AlarmVolumeRamp::UNITY_GAIN);
}

int main() {
  testFade();
  testLifecycle();
  testBoundaries();
  testClockWrap();
  std::cout << "PASS alarm-volume fade lifecycle boundaries clock-wrap\n";
}
