#pragma once

#include "../../drivers/RadioDriver.h"
#include "../../managers/ConfigManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

namespace Layout {
static const int SCREEN_W = 400;
static const uint16_t COLOR_BG = GxEPD_WHITE;
static const uint16_t COLOR_FG = GxEPD_BLACK;
static const int MAIN_SECTION_Y = 24;
static const int CONTROLS_Y = 200;
static const int MAIN_SECTION_H = CONTROLS_Y - MAIN_SECTION_Y;
static const int VOL_BAR_W = 40;
static const int DISPLAY_AREA_W = SCREEN_W - VOL_BAR_W;
static const int FREQUENCY_Y = 50;
static const int FREQUENCY_MESSAGE_Y = 136;
static const int FREQUENCY_MESSAGE_H = 24;
static const int DIAL_Y = 160;
static const int DIAL_H = CONTROLS_Y - DIAL_Y;
} // namespace Layout

class RadioScreen : public Screen {
public:
  struct UIButton {
    int x, y, w, h;
    char label[16];
  };

  RadioScreen(RadioDriver *radio, StatusBar *statusBar, ConfigManager *config);

  void init() override;
  void enter() override;
  void exit() override;
  void update() override;
  void draw(DisplayDriver *display) override;
  bool onInput(UIKey key) override;

private:
  RadioDriver *radio;
  StatusBar *statusBar;
  ConfigManager *config;

  static const int CONTROL_SCAN = 0;
  static const int CONTROL_SEEK_DOWN = 1;
  static const int CONTROL_SEEK_UP = 2;
  static const int CONTROL_VOL_DOWN = 3;
  static const int CONTROL_VOL_UP = 4;
  static const int CONTROL_PAGE_PREV = 5;
  static const int PRESET_PAGE_SIZE = 10;
  static const int CONTROL_PRESET_START = 6;
  static const int CONTROL_PAGE_NEXT = CONTROL_PRESET_START + PRESET_PAGE_SIZE;
  static const int BUTTON_COUNT = CONTROL_PAGE_NEXT + 1;
  UIButton buttons[BUTTON_COUNT];

  // UI State
  int focusedControl;
  int lastFocusedControl;
  uint16_t lastFreq;
  int lastVol;
  String lastRDS;
  int lastRSSI;
  bool lastStereo;
  int smoothedRSSI;
  int presetPage;
  bool isFirstDraw;
  String radioStatus;
  uint32_t radioStatusExpiresAt;

  // Helper methods
  void initButtons();
  String buildRDSLabel(const RDA5807M_Info &radioInfo);
  void updateSmoothedRSSI(int rssi);
  void drawFullRefresh(DisplayDriver *display, uint16_t freq, int vol,
                       bool isStereo, int rssi, const char *rds);
  void drawPartialRefresh(DisplayDriver *display, uint16_t freq, int vol,
                          bool isStereo, const char *rds);
  void drawSingleButton(DisplayDriver *display, int idx, bool isFocused,
                        bool partial);
  void updateButtonFocus(DisplayDriver *display, int oldIdx, int newIdx);
  void drawFrequency(DisplayDriver *display, bool partial);
  void drawDial(DisplayDriver *display, float centerFreq, bool partial);
  void drawSignal(DisplayDriver *display, int rssi, bool partial);
  void drawHeaderInfo(DisplayDriver *display, int vol, bool isStereo, int rssi,
                      bool partial);
  void drawRDS(DisplayDriver *display, const char *text, bool partial);
  void drawButtons(DisplayDriver *display, bool partial);
  void setupWindow(DisplayDriver *display, int x, int y, int w, int h,
                   bool partial);
  void setAlignedPartialWindow(DisplayDriver *display, int x, int y, int w,
                               int h);
  void updateButtonRow(DisplayDriver *display, int rowY);
  void restoreSavedState();
  void saveFocusIfChanged();
  bool hasExpiredRadioStatus(uint32_t now) const;
  void clearExpiredRadioStatus();
  void setRadioStatus(const char *text);
  void setFrequencyStatus(const char *prefix);
  void setScanResultStatus(uint8_t count);
  void setPresetPageStatus();
  void setVolumeStatus();

  void updateFrequency(DisplayDriver *display, const char *rds);
  void updateHeader(DisplayDriver *display, int vol, bool isStereo, int rssi);
  void updateSignal(DisplayDriver *display, int rssi);
  void updateRDS(DisplayDriver *display, const char *text);

  void handleEnterAction();
  void moveFocus(int offset);
  void changePresetPage(int offset);
  void updatePresetButtonLabels();
  int getPresetPageCount() const;
  int getPresetIndexForControl(int controlIndex) const;
  bool isPresetControl(int controlIndex) const;
  void tuneByStep(int offset, const char *statusPrefix);
  void changeVolume(int offset);
  void scanAndSavePresets();
  void saveScannedPresets(uint16_t *stations, uint8_t count);
  void loadPreset(int index);
};
