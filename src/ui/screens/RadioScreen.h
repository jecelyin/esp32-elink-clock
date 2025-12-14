#pragma once

#include "../../drivers/RadioDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConfigManager.h"
#include "../components/StatusBar.h"
#include "../Screen.h"
#include "../UIManager.h"

class RadioScreen : public Screen {
public:
  RadioScreen(RadioDriver *radio, StatusBar *statusBar, ConfigManager *config);

  void init() override;
  void draw(DisplayDriver *display) override;
  void handleInput(UIKey key) override;
  void onLongPress() override;

private:
  RadioDriver *radio;
  StatusBar *statusBar;
  ConfigManager *config;

  // UI State
  int focusedControl; // 0: Seek-, 1: Seek+, 2: Volume, 3-10: Presets
  bool showVolumePopup;
  uint8_t tempVolume;
  
  // Helper methods
  void drawScale(DisplayDriver *display);
  void drawControls(DisplayDriver *display);
  void drawVolumePopup(DisplayDriver *display);
  void savePreset(int index);
  void loadPreset(int index);
};
