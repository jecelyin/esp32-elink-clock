#pragma once

#include "Arduino.h"
#include <map>

struct FakePreferencesState {
  std::map<std::string, uint32_t> numbers;
  std::map<std::string, String> strings;
  bool available = true;
  size_t uintWriteSize = sizeof(uint32_t);
};

inline FakePreferencesState &fakePreferences() {
  static FakePreferencesState state;
  return state;
}

class Preferences {
public:
  bool begin(const char *name, bool) {
    nameSpace = name;
    opened = fakePreferences().available;
    return opened;
  }

  uint8_t getUChar(const char *key, uint8_t fallback) const {
    return getUInt(key, fallback);
  }
  uint16_t getUShort(const char *key, uint16_t fallback) const {
    return getUInt(key, fallback);
  }
  bool getBool(const char *key, bool fallback) const {
    return getUInt(key, fallback);
  }
  uint32_t getUInt(const char *key, uint32_t fallback) const {
    auto found = fakePreferences().numbers.find(fullKey(key));
    return opened && found != fakePreferences().numbers.end() ? found->second
                                                             : fallback;
  }
  String getString(const char *key, const String &fallback) const {
    auto found = fakePreferences().strings.find(fullKey(key));
    return opened && found != fakePreferences().strings.end() ? found->second
                                                             : fallback;
  }

  size_t putUChar(const char *key, uint8_t value) {
    return putNumber(key, value, sizeof(value));
  }
  size_t putUShort(const char *key, uint16_t value) {
    return putNumber(key, value, sizeof(value));
  }
  size_t putBool(const char *key, bool value) {
    return putNumber(key, value, sizeof(value));
  }
  size_t putUInt(const char *key, uint32_t value) {
    size_t written = opened ? fakePreferences().uintWriteSize : 0;
    if (written == sizeof(value)) {
      fakePreferences().numbers[fullKey(key)] = value;
    }
    return written;
  }
  size_t putString(const char *key, const String &value) {
    if (!opened) return 0;
    fakePreferences().strings[fullKey(key)] = value;
    return value.length();
  }

private:
  String nameSpace;
  bool opened = false;

  String fullKey(const char *key) const { return nameSpace + "/" + key; }
  size_t putNumber(const char *key, uint32_t value, size_t size) {
    if (!opened) return 0;
    fakePreferences().numbers[fullKey(key)] = value;
    return size;
  }
};
