#include "ConfigManager.h"

ConfigManager::ConfigManager() {
  // Defaults
  config.volume = 10;
  config.radio_focus_index = 0;
  config.radio_preset_page = 0;
  config.radio_seek_step = 10;
  config.radio_seek_threshold = 8;
  config.radio_bass_boost = false;
  config.radio_force_mono = false;
  config.radio_soft_mute = true;
  config.hw_checked = false;
  config.hw_check_version = 0;
  for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
    config.radio_presets[i] = 0;
    config.radio_alias_frequencies[i] = 0;
    config.radio_names[i] = "";
  }
}

void ConfigManager::begin() {
  if (configMutex == nullptr) {
    configMutex = xSemaphoreCreateMutex();
  }
  prefsReady = prefs.begin("clock_cfg", false);
  load();
}

void ConfigManager::load() {
  lockConfig();
  config.volume = prefs.getUChar("vol", 10);
  config.radio_focus_index = prefs.getUChar("radio_focus", 0);
  config.radio_preset_page = prefs.getUChar("radio_page", 0);
  config.radio_seek_step = prefs.getUShort("radio_step", 10);
  config.radio_seek_threshold = prefs.getUChar("radio_th", 8);
  config.radio_bass_boost = prefs.getBool("radio_bass", false);
  config.radio_force_mono = prefs.getBool("radio_mono", false);
  config.radio_soft_mute = prefs.getBool("radio_soft", true);
  config.weather_api_token = prefs.getString("weather_token", "");
  config.holiday_api_token = prefs.getString("holiday_token", "");
  config.hw_checked = prefs.getBool("hwck", false);
  config.hw_check_version = prefs.getUChar("hwv", 0);
  for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
    char key[16];
    snprintf(key, sizeof(key), "radio_p%d", i);
    config.radio_presets[i] = prefs.getUShort(key, 0);
    snprintf(key, sizeof(key), "radio_n%d", i);
    config.radio_names[i] = prefs.getString(key, "");
    snprintf(key, sizeof(key), "radio_a%d", i);
    config.radio_alias_frequencies[i] =
        prefs.getUShort(key, config.radio_names[i].length() > 0
                                ? config.radio_presets[i]
                                : 0);
  }
  unlockConfig();
}

String ConfigManager::getRadioName(uint16_t frequency) const {
  lockConfig();
  for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
    if (config.radio_alias_frequencies[i] == frequency &&
        config.radio_names[i].length() > 0) {
      String name = config.radio_names[i];
      unlockConfig();
      return name;
    }
  }
  unlockConfig();
  return "";
}

bool ConfigManager::setRadioName(uint16_t frequency, const String &name) {
  lockConfig();
  int emptyIndex = -1;
  for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
    if (config.radio_alias_frequencies[i] == frequency) {
      config.radio_names[i] = name;
      if (name.length() == 0) {
        config.radio_alias_frequencies[i] = 0;
      }
      unlockConfig();
      return true;
    }
    if (emptyIndex < 0 && config.radio_alias_frequencies[i] == 0) {
      emptyIndex = i;
    }
  }
  if (emptyIndex >= 0 && name.length() > 0) {
    config.radio_alias_frequencies[emptyIndex] = frequency;
    config.radio_names[emptyIndex] = name;
    unlockConfig();
    return true;
  }
  unlockConfig();
  return name.length() == 0;
}

void ConfigManager::saveVolume() {
  lockConfig();
  prefs.putUChar("vol", config.volume);
  unlockConfig();
}

void ConfigManager::saveHardwareCheck() {
  lockConfig();
  prefs.putBool("hwck", config.hw_checked);
  prefs.putUChar("hwv", config.hw_check_version);
  unlockConfig();
}

void ConfigManager::saveRadioUiState() {
  lockConfig();
  prefs.putUChar("radio_focus", config.radio_focus_index);
  prefs.putUChar("radio_page", config.radio_preset_page);
  unlockConfig();
}

void ConfigManager::saveRadioPresets() {
  lockConfig();
  for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "radio_p%d", i);
    prefs.putUShort(key, config.radio_presets[i]);
  }
  unlockConfig();
}

bool ConfigManager::saveRadioConfiguration() {
  lockConfig();
  bool saved = prefs.putUShort("radio_step", config.radio_seek_step) > 0;
  saved = (prefs.putUChar("radio_th", config.radio_seek_threshold) > 0) &&
          saved;
  saved = (prefs.putBool("radio_bass", config.radio_bass_boost) > 0) &&
          saved;
  saved = (prefs.putBool("radio_mono", config.radio_force_mono) > 0) &&
          saved;
  saved = (prefs.putBool("radio_soft", config.radio_soft_mute) > 0) &&
          saved;
  for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
    char key[16];
    snprintf(key, sizeof(key), "radio_a%d", i);
    saved =
        (prefs.putUShort(key, config.radio_alias_frequencies[i]) > 0) && saved;
    snprintf(key, sizeof(key), "radio_n%d", i);
    size_t written = prefs.putString(key, config.radio_names[i]);
    saved = (config.radio_names[i].length() == 0 || written > 0) && saved;
  }
  unlockConfig();
  return saved;
}

String ConfigManager::getWeatherApiToken() const {
  lockConfig();
  String token = config.weather_api_token;
  unlockConfig();
  return token;
}

String ConfigManager::getHolidayApiToken() const {
  lockConfig();
  String token = config.holiday_api_token;
  unlockConfig();
  return token;
}

bool ConfigManager::updateApiTokens(const String &weather,
                                    bool updateWeather,
                                    const String &holiday,
                                    bool updateHoliday) {
  if (!prefsReady) {
    return false;
  }

  lockConfig();
  String previousWeather = config.weather_api_token;
  String previousHoliday = config.holiday_api_token;
  if (updateWeather) {
    config.weather_api_token = weather;
    prefs.putString("weather_token", weather);
  }
  if (updateHoliday) {
    config.holiday_api_token = holiday;
    prefs.putString("holiday_token", holiday);
  }
  bool saved =
      (!updateWeather || prefs.getString("weather_token", "\x01") == weather) &&
      (!updateHoliday || prefs.getString("holiday_token", "\x01") == holiday);
  if (!saved) {
    config.weather_api_token = previousWeather;
    config.holiday_api_token = previousHoliday;
  }
  unlockConfig();
  return saved;
}

void ConfigManager::lockConfig() const {
  if (configMutex != nullptr) {
    xSemaphoreTake(configMutex, portMAX_DELAY);
  }
}

void ConfigManager::unlockConfig() const {
  if (configMutex != nullptr) {
    xSemaphoreGive(configMutex);
  }
}
