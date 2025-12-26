#include "MusicScreen.h"
#include <Arduino.h>

namespace MusicLayout {
const int SCREEN_W = 400;
const int SCREEN_H = 300;
const int SYS_BAR_H = 24;
const int FOOTER_H = 40;
const int LAYOUT_H = SCREEN_H - SYS_BAR_H - FOOTER_H;

const int PANEL_LEFT_W = 160;
const int PANEL_RIGHT_W = SCREEN_W - PANEL_LEFT_W;

const uint16_t COLOR_BG = GxEPD_WHITE;
const uint16_t COLOR_FG = GxEPD_BLACK;
} // namespace MusicLayout

MusicScreen::MusicScreen(MusicManager *music, StatusBar *statusBar,
                         ConfigManager *config)
    : music(music), statusBar(statusBar), config(config) {
  initLayout();
}

void MusicScreen::initLayout() {
  using namespace MusicLayout;
  // Initialize button regions for input handling and drawing focus

  // Left Panel Buttons (Prev, Play, Next)
  int controlY = SYS_BAR_H + (LAYOUT_H / 2) + 20;
  buttons[BTN_PREV] = {25, controlY, 30, 30, "<<"};    // Prev
  buttons[BTN_PLAY] = {65, controlY - 5, 40, 40, ">"}; // Play/Pause
  buttons[BTN_NEXT] = {115, controlY, 30, 30, ">>"};   // Next

  // Footer Tools
  int footerY = SCREEN_H - FOOTER_H;
  buttons[BTN_LOOP] = {SCREEN_W - 120, footerY + 5, 30, 30, "L"};   // Loop Mode
  buttons[BTN_VOL_DEC] = {SCREEN_W - 80, footerY + 5, 20, 30, "-"}; // Vol Dec
  buttons[BTN_VOL_INC] = {SCREEN_W - 30, footerY + 5, 20, 30, "+"}; // Vol Inc

  // Right Panel Buttons (Playlist area and Pagination)
  buttons[BTN_LIST] = {PANEL_LEFT_W, SYS_BAR_H, PANEL_RIGHT_W, LAYOUT_H - 36,
                       "List"}; // Playlist selection
  buttons[BTN_PAGE_UP] = {PANEL_LEFT_W, SCREEN_H - FOOTER_H - 36,
                          PANEL_RIGHT_W / 2, 36, "^"}; // Page Up
  buttons[BTN_PAGE_DOWN] = {PANEL_LEFT_W + PANEL_RIGHT_W / 2,
                            SCREEN_H - FOOTER_H - 36, PANEL_RIGHT_W / 2, 36,
                            "v"}; // Page Down
}

void MusicScreen::init() {}

void MusicScreen::enter() {
  digitalWrite(AMP_EN, 1);
  digitalWrite(CODEC_EN, 1);
  isFirstDraw = true;
  music->init();
}

void MusicScreen::exit() {
  // music->stop();
}

void MusicScreen::update() { music->update(); }

void MusicScreen::draw(DisplayDriver *display) {
  using namespace MusicLayout;

  int currentIdx = music->getCurrentTrackIndex();
  int vol = music->getVolume();
  LoopMode loopMode = music->getLoopMode();
  bool isPlaying = music->isPlaying();
  uint32_t elapsed = music->getElapsedSeconds();

  if (isFirstDraw) {
    isFirstDraw = false;
    lastTrackIdx = currentIdx;
    lastVol = vol;
    lastLoopMode = loopMode;
    lastIsPlaying = isPlaying;
    lastElapsed = elapsed;
    lastFocusedControl = focusedControl;
    lastScrollOffset = playlistScrollOffset;

    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(COLOR_BG);
      statusBar->draw(display, true);

      // Vertical Divider
      display->display.drawLine(PANEL_LEFT_W, SYS_BAR_H, PANEL_LEFT_W,
                                SCREEN_H - FOOTER_H, COLOR_FG);

      drawLeftPanel(display);
      drawRightPanel(display);
      drawFooter(display);

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
    display->powerOff();
    return;
  }

  // Partial updates
  if (currentIdx != lastTrackIdx || isPlaying != lastIsPlaying) {
    lastTrackIdx = currentIdx;
    lastIsPlaying = isPlaying;
    updatePlaybackInfo(display);
  }

  if (vol != lastVol || loopMode != lastLoopMode) {
    lastVol = vol;
    lastLoopMode = loopMode;
    updateFooterInfo(display);
  }

  if (abs((int)elapsed - (int)lastElapsed) >= 1) {
    lastElapsed = elapsed;
    updateProgress(display);
  }

  if (focusedControl != lastFocusedControl) {
    updateFocus(display, lastFocusedControl, focusedControl);
    lastFocusedControl = focusedControl;
  }

  if (playlistScrollOffset != lastScrollOffset) {
    lastScrollOffset = playlistScrollOffset;
    updatePlaylist(display);
  }
}

void MusicScreen::drawLeftPanel(DisplayDriver *display) {
  using namespace MusicLayout;
  int centerX = PANEL_LEFT_W / 2;
  int centerY = SYS_BAR_H + (LAYOUT_H / 2) - 20;

  // Track Info
  const auto &playlist = music->getPlaylist();
  int currentIdx = music->getCurrentTrackIndex();
  String title = (currentIdx != -1) ? playlist[currentIdx].title : "No Track";
  String artist =
      (currentIdx != -1) ? playlist[currentIdx].artist : "Unknown Artist";

  display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  int tw = display->u8g2Fonts.getUTF8Width(title.c_str());
  if (tw > PANEL_LEFT_W - 40) {
    // Very crude multiline/wrap for title
    display->u8g2Fonts.setCursor(25, centerY - 40);
    display->u8g2Fonts.print(title.substring(0, 10)); // Just a sample wrap
    display->u8g2Fonts.setCursor(25, centerY - 15);
    display->u8g2Fonts.print(title.substring(10));
  } else {
    display->u8g2Fonts.setCursor(25, centerY - 25);
    display->u8g2Fonts.print(title);
  }

  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  display->u8g2Fonts.setCursor(25, centerY + 10);
  display->u8g2Fonts.setForegroundColor(
      GxEPD_BLACK); // Gray simulation if possible
  display->u8g2Fonts.print(artist);
  display->u8g2Fonts.setForegroundColor(COLOR_FG);

  drawPlaybackControls(display);
}

void MusicScreen::drawPlaybackControls(DisplayDriver *display) {
  using namespace MusicLayout;
  // Prev, Play/Pause, Next Icons
  // Using simple text/symbols for now as icons depend on available fonts

  for (int i = 0; i <= 2; i++) {
    UIButton &btn = buttons[i];
    bool focused = (focusedControl == i);
    if (focused)
      display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
    display->display.drawRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);

    display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    int tw = display->u8g2Fonts.getUTF8Width(btn.label);
    display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2,
                                 btn.y + btn.h / 2 + 5);

    if (i == 1) {
      display->u8g2Fonts.print(music->isPlaying() ? "||" : ">");
    } else {
      display->u8g2Fonts.print(btn.label);
    }
  }
}

void MusicScreen::drawRightPanel(DisplayDriver *display) {
  using namespace MusicLayout;
  drawPlaylist(display);

  // Pagination
  int y = SCREEN_H - FOOTER_H - 36;
  display->display.drawLine(PANEL_LEFT_W, y, SCREEN_W, y, COLOR_FG);
  display->display.drawLine(PANEL_LEFT_W + PANEL_RIGHT_W / 2, y,
                            PANEL_LEFT_W + PANEL_RIGHT_W / 2, y + 36, COLOR_FG);

  for (int i = 7; i <= 8; i++) {
    UIButton &btn = buttons[i];
    bool focused = (focusedControl == i);
    if (focused)
      display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
    display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
    int tw = display->u8g2Fonts.getUTF8Width(btn.label);
    display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2,
                                 btn.y + btn.h / 2 + 5);
    display->u8g2Fonts.print(btn.label);
  }
}

void MusicScreen::drawPlaylist(DisplayDriver *display) {
  using namespace MusicLayout;
  const auto &playlist = music->getPlaylist();
  int currentIdx = music->getCurrentTrackIndex();

  int startY = SYS_BAR_H;
  int itemH = 35;
  int itemsPerPage = (LAYOUT_H - 36) / itemH;

  // Auto scroll logic
  if (currentIdx != -1) {
    if (currentIdx < playlistScrollOffset)
      playlistScrollOffset = currentIdx;
    if (currentIdx >= playlistScrollOffset + itemsPerPage)
      playlistScrollOffset = currentIdx - itemsPerPage + 1;
  }

  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  for (int i = 0; i < itemsPerPage; i++) {
    int idx = playlistScrollOffset + i;
    if (idx >= (int)playlist.size())
      break;

    int y = startY + i * itemH;
    bool active = (idx == currentIdx);
    bool focused =
        (focusedControl == 6 && active); // Focus on playlist highlights active

    if (active)
      display->display.fillRect(PANEL_LEFT_W + 1, y, PANEL_RIGHT_W - 1, itemH,
                                COLOR_FG);
    display->display.drawLine(PANEL_LEFT_W, y + itemH, SCREEN_W, y + itemH,
                              COLOR_FG);

    display->u8g2Fonts.setForegroundColor(active ? COLOR_BG : COLOR_FG);
    display->u8g2Fonts.setCursor(PANEL_LEFT_W + 15, y + (itemH / 2) + 5);
    display->u8g2Fonts.print(playlist[idx].title);

    // Duration
    String dur = music->formatTime(playlist[idx].duration);
    int dw = display->u8g2Fonts.getUTF8Width(dur.c_str());
    display->u8g2Fonts.setCursor(SCREEN_W - dw - 15, y + (itemH / 2) + 5);
    display->u8g2Fonts.print(dur);
  }
}

void MusicScreen::drawFooter(DisplayDriver *display) {
  using namespace MusicLayout;
  int y = SCREEN_H - FOOTER_H;
  display->display.drawLine(0, y, SCREEN_W, y, COLOR_FG);

  // Progress Bar
  int progressWrapY = y + 5;
  int progressWrapH = 30;
  int progressX = 15;
  int progressMaxW = SCREEN_W - 140;

  uint32_t elapsed = music->getElapsedSeconds();
  uint32_t total = music->getTotalSeconds();
  float pct = (total > 0) ? (float)elapsed / total : 0;

  display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  display->u8g2Fonts.setCursor(progressX, progressWrapY + 10);
  display->u8g2Fonts.print(music->formatTime(elapsed) + " / " +
                           music->formatTime(total));

  display->display.drawRect(progressX, progressWrapY + 15, progressMaxW, 4,
                            COLOR_FG);
  display->display.fillRect(progressX, progressWrapY + 15,
                            (int)(progressMaxW * pct), 4, COLOR_FG);

  // Tools
  updateVolumeUI(display);

  // Loop Mode
  UIButton &btn = buttons[5];
  bool focused = (focusedControl == 5);
  if (focused)
    display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
  display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setCursor(btn.x + 5, btn.y + 20);
  display->u8g2Fonts.print(
      music->getLoopMode() == LOOP_ALL
          ? "ALL"
          : (music->getLoopMode() == LOOP_ONE ? "ONE" : "OFF"));
}

void MusicScreen::updateVolumeUI(DisplayDriver *display) {
  using namespace MusicLayout;
  int vol = music->getVolume();

  // Vol -
  UIButton &btnDec = buttons[3];
  bool focusedDec = (focusedControl == 3);
  if (focusedDec)
    display->display.fillRect(btnDec.x, btnDec.y, btnDec.w, btnDec.h, COLOR_FG);
  display->u8g2Fonts.setForegroundColor(focusedDec ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setCursor(btnDec.x + 5, btnDec.y + 20);
  display->u8g2Fonts.print("-");

  // Vol Val
  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setCursor(SCREEN_W - 60, SCREEN_H - 18);
  display->u8g2Fonts.print(String(vol));

  // Vol +
  UIButton &btnInc = buttons[4];
  bool focusedInc = (focusedControl == 4);
  if (focusedInc)
    display->display.fillRect(btnInc.x, btnInc.y, btnInc.w, btnInc.h, COLOR_FG);
  display->u8g2Fonts.setForegroundColor(focusedInc ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setCursor(btnInc.x + 5, btnInc.y + 20);
  display->u8g2Fonts.print("+");
}

bool MusicScreen::handleInput(UIKey key) {
  int oldFocused = focusedControl;
  if (key == UI_KEY_LEFT) {
    focusedControl = (focusedControl - 1 + BUTTON_COUNT) % BUTTON_COUNT;
  } else if (key == UI_KEY_RIGHT) {
    focusedControl = (focusedControl + 1) % BUTTON_COUNT;
  } else if (key == UI_KEY_ENTER) {
    switch (focusedControl) {
    case BTN_PREV:
      music->prevTrack();
      break;
    case BTN_PLAY:
      music->togglePlay();
      break;
    case BTN_NEXT:
      music->nextTrack();
      break;
    case BTN_VOL_DEC:
      music->setVolume(music->getVolume() - 1);
      break;
    case BTN_VOL_INC:
      music->setVolume(music->getVolume() + 1);
      break;
    case BTN_LOOP:
      music->toggleLoopMode();
      break;
    case BTN_LIST: /* Select from list - logic needed if we want to enter list
                    */
      break;
    case BTN_PAGE_UP:
      playlistScrollOffset = max(0, playlistScrollOffset - 5);
      break;
    case BTN_PAGE_DOWN:
      playlistScrollOffset =
          min((int)music->getPlaylist().size() - 1, playlistScrollOffset + 5);
      break;
    }
  }

  return (oldFocused != focusedControl || key == UI_KEY_ENTER);
}

bool MusicScreen::onLongPress() {
  if (uiManager)
    uiManager->switchScreen(SCREEN_MENU);
  return false;
}

void MusicScreen::updateProgress(DisplayDriver *display) {
  using namespace MusicLayout;
  uint16_t y = SCREEN_H - FOOTER_H + 5;
  display->display.setPartialWindow(15, y, SCREEN_W - 140, 25);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    uint32_t elapsed = music->getElapsedSeconds();
    uint32_t total = music->getTotalSeconds();
    float pct = (total > 0) ? (float)elapsed / total : 0;

    display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    display->u8g2Fonts.setCursor(15, y + 10);
    display->u8g2Fonts.print(music->formatTime(elapsed) + " / " +
                             music->formatTime(total));

    display->display.drawRect(15, y + 15, SCREEN_W - 140, 4, COLOR_FG);
    display->display.fillRect(15, y + 15, (int)((SCREEN_W - 140) * pct), 4,
                              COLOR_FG);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}

void MusicScreen::updateFocus(DisplayDriver *display, int oldIdx, int newIdx) {
  using namespace MusicLayout;
  // Redraw both buttons
  int indices[] = {oldIdx, newIdx};
  for (int i : indices) {
    if (i < 0 || i >= BUTTON_COUNT)
      continue;
    UIButton &btn = buttons[i];
    display->display.setPartialWindow(btn.x, btn.y, btn.w, btn.h);
    display->display.firstPage();
    do {
      display->display.fillScreen(COLOR_BG);
      bool focused = (focusedControl == i);
      if (focused)
        display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
      display->display.drawRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);

      display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
      display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      int tw = display->u8g2Fonts.getUTF8Width(btn.label);
      display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2,
                                   btn.y + btn.h / 2 + 5);

      if (i == BTN_PLAY) {
        display->u8g2Fonts.print(music->isPlaying() ? "||" : ">");
      } else {
        display->u8g2Fonts.print(btn.label);
      }
      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }
}

void MusicScreen::updatePlaylist(DisplayDriver *display) {
  using namespace MusicLayout;
  display->display.setPartialWindow(PANEL_LEFT_W + 1, SYS_BAR_H,
                                    PANEL_RIGHT_W - 1, LAYOUT_H);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawPlaylist(display);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}

void MusicScreen::updatePlaybackInfo(DisplayDriver *display) {
  using namespace MusicLayout;
  display->display.setPartialWindow(0, SYS_BAR_H, PANEL_LEFT_W - 1, LAYOUT_H);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawLeftPanel(display);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}

void MusicScreen::updateFooterInfo(DisplayDriver *display) {
  using namespace MusicLayout;
  display->display.setPartialWindow(0, SCREEN_H - FOOTER_H + 1, SCREEN_W,
                                    FOOTER_H - 1);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawFooter(display);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}
