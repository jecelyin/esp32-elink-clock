#pragma once

#include "../../drivers/AudioDriver.h"
#include "../../managers/ConfigManager.h"
#include "../../managers/MusicManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

#define BTN_PREV 0
#define BTN_PLAY 1
#define BTN_NEXT 2
#define BTN_VOL_DEC 3
#define BTN_VOL_INC 4
#define BTN_LOOP 5
#define BTN_LIST 6
#define BTN_PAGE_UP 7
#define BTN_PAGE_DOWN 8

class MusicScreen : public Screen {
public:
  MusicScreen(MusicManager *music, StatusBar *statusBar, ConfigManager *config);

  void init() override;
  void enter() override;
  void exit() override;
  void update() override;
  void draw(DisplayDriver *display) override;
  bool handleInput(UIKey key) override;
  bool onLongPress() override;

private:
  MusicManager *music;
  StatusBar *statusBar;
  ConfigManager *config;

  int focusedControl = 0; // 0: Prev, 1: Play, 2: Next, 3: DecVol, 4: IncVol, 5:
                          // Loop, 6: Playlist, 7: PageUp, 8: PageDown
  int playlistScrollOffset = 0;
  bool isFirstDraw = true;

  // State trackers for partial refresh
  int lastTrackIdx = -1;
  int lastVol = -1;
  LoopMode lastLoopMode = LOOP_ALL;
  bool lastIsPlaying = false;
  uint32_t lastElapsed = 0;
  int lastFocusedControl = -1;
  int lastScrollOffset = -1;

  struct UIButton {
    int x, y, w, h;
    char label[16];
  };

  static const int BUTTON_COUNT = 9;
  UIButton buttons[BUTTON_COUNT];

  void initLayout();
  void drawLeftPanel(DisplayDriver *display);
  void drawRightPanel(DisplayDriver *display);
  void drawFooter(DisplayDriver *display);
  void drawPlaylist(DisplayDriver *display);
  void drawPlaybackControls(DisplayDriver *display);

  void updateProgress(DisplayDriver *display);
  void updateVolumeUI(DisplayDriver *display);
  void updatePlaylist(DisplayDriver *display);
  void updateFocus(DisplayDriver *display, int oldIdx, int newIdx);
  void updatePlaybackInfo(DisplayDriver *display);
  void updateFooterInfo(DisplayDriver *display);
};
