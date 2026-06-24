#pragma once

#include <Arduino.h>
#include <Preferences.h>

static const uint8_t RADIO_PRESET_COUNT = 50;

struct AppConfig {
  String wifi_ssid;
  String wifi_pass;
  uint8_t volume;
  bool time_format_24;
  uint16_t radio_presets[RADIO_PRESET_COUNT];
  uint8_t radio_focus_index;
  uint8_t radio_preset_page;
  bool hw_checked; // 硬件自检状态
  uint8_t hw_check_version; // 硬件自检清单版本
  // Add more config items as needed
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  void load();
  void save();

  AppConfig config;

private:
  Preferences prefs;
};
