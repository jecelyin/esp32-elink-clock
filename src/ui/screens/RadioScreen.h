#pragma once

#include "../../drivers/RadioDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConfigManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class RadioScreen : public Screen {
public:
  RadioScreen(RadioDriver *radio, StatusBar *statusBar, ConfigManager *config);

  void init() override;
  void enter() override;
  void exit() override;
  void update() override;
  void draw(DisplayDriver *display) override;
  bool handleInput(UIKey key) override;
  bool onLongPress() override;

private:
  RadioDriver *radio;
  StatusBar *statusBar;
  ConfigManager *config;

  struct UIButton {
    int x, y, w, h;
    char label[16];
  };
  static const int BUTTON_COUNT = 13;
  UIButton buttons[BUTTON_COUNT];

  // UI State
  int focusedControl; // 0: Seek-, 1: Seek+, 2: Vol-, 3: Vol+, 4: BIAS, 5-12:
                      // Presets
  int lastFocusedControl;
  uint16_t lastFreq;
  int lastVol;
  String lastRDS;
  int lastRSSI;
  bool lastStereo;
  int smoothedRSSI;
  bool isFirstDraw;

  // Helper methods
  void initButtons();
  void drawSingleButton(DisplayDriver *display, int idx, bool isFocused,
                        bool partial);
  void updateButtonFocus(DisplayDriver *display, int oldIdx, int newIdx);
  void drawStaticGrid(DisplayDriver *display);
  void drawFrequency(DisplayDriver *display, bool partial);
  void drawDial(DisplayDriver *display, float centerFreq, bool partial);
  void drawSignal(DisplayDriver *display, int rssi, bool partial);
  void drawHeaderInfo(DisplayDriver *display, int vol, bool isStereo,
                      int signal, int rssi, bool partial);
  void drawRDS(DisplayDriver *display, const char *text, bool partial);
  void drawButtons(DisplayDriver *display, bool partial);
  void setupWindow(DisplayDriver *display, int x, int y, int w, int h,
                   bool partial);

  void savePreset(int index);
  void loadPreset(int index);
};
