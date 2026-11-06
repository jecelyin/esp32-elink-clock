#pragma once

#include "../fakes/Arduino.h"
#include <ctime>

class String : public std::string {
public:
  String() = default;
  String(const char *value) : std::string(value) {}
  String(const std::string &value) : std::string(value) {}
  bool startsWith(const char *value) const { return find(value) == 0; }
  bool endsWith(const char *value) const {
    size_t length = std::string(value).length();
    return size() >= length && compare(size() - length, length, value) == 0;
  }
  String substring(size_t offset) const { return substr(offset); }
};

bool setCpuFrequencyMhz(uint32_t frequency);
