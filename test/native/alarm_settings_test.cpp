#include "../../src/managers/ConfigManager.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace {
const char *ALARM_KEY = "clock_cfg/alarm_sound";
const char *MEDIA_KEY = "clock_cfg/vol";

AlarmSoundSettings settings(uint8_t volume, uint8_t seconds) {
  AlarmSoundSettings result;
  result.volume = volume;
  result.fadeInSeconds = seconds;
  return result;
}

void expectSettings(const ConfigManager &manager, uint8_t volume,
                    uint8_t seconds) {
  AlarmSoundSettings actual = manager.getAlarmSoundSettings();
  assert(actual.volume == volume);
  assert(actual.fadeInSeconds == seconds);
}

void testDefaults() {
  ConfigManager manager;
  expectSettings(manager, 21, 3);
  manager.begin();
  expectSettings(manager, 21, 3);
  assert(fakePreferences().numbers.count(ALARM_KEY) == 0);
}

void testRoundTrip() {
  ConfigManager manager;
  manager.begin();
  // 直接编译实际 ConfigManager，使用重建实例模拟断电重启后的 NVS 恢复。
  const AlarmSoundSettings cases[] = {
      settings(0, 0), settings(10, 3), settings(21, 60)};
  for (const AlarmSoundSettings &value : cases) {
    assert(manager.saveAlarmSoundSettings(value));
    expectSettings(manager, value.volume, value.fadeInSeconds);
    ConfigManager rebooted;
    rebooted.begin();
    expectSettings(rebooted, value.volume, value.fadeInSeconds);
    uint32_t stored = fakePreferences().numbers.at(ALARM_KEY);
    assert(stored == (static_cast<uint32_t>(value.volume) |
                      (static_cast<uint32_t>(value.fadeInSeconds) << 8)));
  }
}

void testInvalidRanges() {
  ConfigManager manager;
  manager.begin();
  assert(manager.saveAlarmSoundSettings(settings(9, 5)));
  uint32_t previous = fakePreferences().numbers.at(ALARM_KEY);
  const AlarmSoundSettings cases[] = {
      settings(22, 3), settings(10, 61), settings(255, 255)};
  for (const AlarmSoundSettings &value : cases) {
    assert(!manager.saveAlarmSoundSettings(value));
    expectSettings(manager, 9, 5);
    assert(fakePreferences().numbers.at(ALARM_KEY) == previous);
  }
}

void testWriteFailures() {
  ConfigManager manager;
  manager.begin();
  assert(manager.saveAlarmSoundSettings(settings(9, 5)));
  uint32_t previous = fakePreferences().numbers.at(ALARM_KEY);
  // 失败和异常短写均不允许提前更新 RAM，否则界面会与重启后的状态不符。
  const size_t failures[] = {0, 2};
  for (size_t written : failures) {
    fakePreferences().uintWriteSize = written;
    assert(!manager.saveAlarmSoundSettings(settings(4, 0)));
    expectSettings(manager, 9, 5);
    assert(fakePreferences().numbers.at(ALARM_KEY) == previous);
  }
  fakePreferences().uintWriteSize = sizeof(uint32_t);
  ConfigManager rebooted;
  rebooted.begin();
  expectSettings(rebooted, 9, 5);
}

void testUnavailableStore() {
  ConfigManager uninitialized;
  assert(!uninitialized.saveAlarmSoundSettings(settings(4, 0)));
  expectSettings(uninitialized, 21, 3);
  fakePreferences().available = false;
  ConfigManager manager;
  manager.begin();
  assert(!manager.saveAlarmSoundSettings(settings(4, 0)));
  expectSettings(manager, 21, 3);
  assert(fakePreferences().numbers.count(ALARM_KEY) == 0);
}

void testCorruptStore() {
  const uint32_t corrupt[] = {22U | (3U << 8), 10U | (61U << 8),
                              21U | (3U << 8) | (1U << 16), UINT32_MAX};
  for (uint32_t stored : corrupt) {
    fakePreferences().numbers[ALARM_KEY] = stored;
    ConfigManager manager;
    manager.begin();
    expectSettings(manager, 21, 3);
  }
}

void testIndependentMediaVolume() {
  fakePreferences().numbers[MEDIA_KEY] = 7;
  ConfigManager manager;
  manager.begin();
  assert(manager.config.volume == 7);
  assert(manager.saveAlarmSoundSettings(settings(0, 0)));
  assert(manager.config.volume == 7);
  assert(fakePreferences().numbers.at(MEDIA_KEY) == 7);
  manager.config.volume = 13;
  manager.saveVolume();
  expectSettings(manager, 0, 0);
  ConfigManager rebooted;
  rebooted.begin();
  assert(rebooted.config.volume == 13);
  expectSettings(rebooted, 0, 0);
}
} // namespace

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string test = argv[1];
  if (test == "defaults") testDefaults();
  else if (test == "round-trip") testRoundTrip();
  else if (test == "invalid") testInvalidRanges();
  else if (test == "write-failure") testWriteFailures();
  else if (test == "unavailable") testUnavailableStore();
  else if (test == "corrupt") testCorruptStore();
  else if (test == "media") testIndependentMediaVolume();
  else throw std::logic_error("Unknown alarm settings test case: " + test);
  std::cout << "PASS alarm-settings-" << test << '\n';
}
