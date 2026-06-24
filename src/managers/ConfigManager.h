#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const uint8_t RADIO_PRESET_COUNT = 50;

struct AppConfig {
  uint8_t volume;
  uint16_t radio_presets[RADIO_PRESET_COUNT];
  uint16_t radio_alias_frequencies[RADIO_PRESET_COUNT];
  String radio_names[RADIO_PRESET_COUNT];
  uint8_t radio_focus_index;
  uint8_t radio_preset_page;
  uint16_t radio_seek_step;
  uint8_t radio_seek_threshold;
  bool radio_bass_boost;
  bool radio_force_mono;
  bool radio_soft_mute;
  String weather_api_token;
  String holiday_api_token;
  bool hw_checked; // 硬件自检状态
  uint8_t hw_check_version; // 硬件自检清单版本
};

class ConfigManager {
public:
  ConfigManager();
  void begin();
  void load();
  String getRadioName(uint16_t frequency) const;
  bool setRadioName(uint16_t frequency, const String &name);
  void saveVolume();
  void saveHardwareCheck();
  void saveRadioUiState();
  void saveRadioPresets();
  bool saveRadioConfiguration();
  String getWeatherApiToken() const;
  String getHolidayApiToken() const;
  bool updateApiTokens(const String &weather, bool updateWeather,
                       const String &holiday, bool updateHoliday);

  AppConfig config;

private:
  Preferences prefs;
  mutable SemaphoreHandle_t configMutex = nullptr;
  bool prefsReady = false;
  void lockConfig() const;
  void unlockConfig() const;
};
