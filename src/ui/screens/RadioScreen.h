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
  int focusedControl; // 0: Seek-, 1: Seek+, 2: Vol-, 3: Vol+, 4-11: Presets
  
  // Helper methods
  void drawStaticGrid(DisplayDriver *display);
  void drawFrequency(DisplayDriver *display, bool partial);
  void drawDial(DisplayDriver *display, float centerFreq, bool partial);
  void drawVolume(DisplayDriver *display, int vol, bool partial);
  void drawHeaderInfo(DisplayDriver *display, const char* rds, int signal, int rssi, bool partial);
  void drawRDS(DisplayDriver *display, const char* text, bool partial);
  void drawButtons(DisplayDriver *display, bool partial);
  void setupWindow(DisplayDriver *display, int x, int y, int w, int h, bool partial);
  
  void savePreset(int index);
  void loadPreset(int index);
};
