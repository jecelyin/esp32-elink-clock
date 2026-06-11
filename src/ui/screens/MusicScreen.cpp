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

namespace MusicGlyphs {
constexpr uint16_t PREV = 'G';
constexpr uint16_t PLAY = 'E';
constexpr uint16_t PAUSE = 'D';
constexpr uint16_t NEXT = 'H';
constexpr uint16_t PAGE_UP = 'O';
constexpr uint16_t PAGE_DOWN = 'L';
constexpr uint16_t LOOP = 'W';
} // namespace MusicGlyphs

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
  buttons[BTN_PREV] = UIButton{25, controlY, 30, 30, "<<"};    // Prev
  buttons[BTN_PLAY] = UIButton{65, controlY - 5, 40, 40, ">"}; // Play/Pause
  buttons[BTN_NEXT] = UIButton{115, controlY, 30, 30, ">>"};   // Next

  // Footer Tools
  int footerY = SCREEN_H - FOOTER_H;
  buttons[BTN_LOOP] =
      UIButton{SCREEN_W - 120, footerY + 5, 30, 30, "L"}; // Loop Mode
  buttons[BTN_VOL_DEC] =
      UIButton{SCREEN_W - 80, footerY + 5, 20, 30, "-"}; // Vol Dec
  buttons[BTN_VOL_INC] =
      UIButton{SCREEN_W - 30, footerY + 5, 20, 30, "+"}; // Vol Inc

  // Right Panel Buttons (Playlist area and Pagination)
  buttons[BTN_LIST] =
      UIButton{PANEL_LEFT_W, SYS_BAR_H, PANEL_RIGHT_W, LAYOUT_H - 36,
               "List"}; // Playlist selection
  buttons[BTN_PAGE_UP] =
      UIButton{PANEL_LEFT_W, SCREEN_H - FOOTER_H - 36, PANEL_RIGHT_W / 2, 36,
               "^"}; // Page Up
  buttons[BTN_PAGE_DOWN] =
      UIButton{PANEL_LEFT_W + PANEL_RIGHT_W / 2, SCREEN_H - FOOTER_H - 36,
               PANEL_RIGHT_W / 2, 36, "v"}; // Page Down
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
  for (int i = 0; i <= 2; i++) {
    UIButton &btn = buttons[i];
    bool focused = (focusedControl == i);
    if (focused)
      display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
    display->display.drawRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
    drawButtonContent(display, i, btn, focused);
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

  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  for (int i = 7; i <= 8; i++) {
    UIButton &btn = buttons[i];
    bool focused = (focusedControl == i);
    if (focused)
      display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
    drawButtonContent(display, i, btn, focused);
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
  drawButtonContent(display, BTN_LOOP, btn, focused);
}

void MusicScreen::updateVolumeUI(DisplayDriver *display) {
  using namespace MusicLayout;
  int vol = music->getVolume();

  // Vol -
  UIButton &btnDec = buttons[3];
  bool focusedDec = (focusedControl == 3);
  if (focusedDec)
    display->display.fillRect(btnDec.x, btnDec.y, btnDec.w, btnDec.h, COLOR_FG);
  drawButtonContent(display, BTN_VOL_DEC, btnDec, focusedDec);

  // Vol Val
  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setCursor(SCREEN_W - 60, SCREEN_H - 18);
  display->u8g2Fonts.print(String(vol));

  // Vol +
  UIButton &btnInc = buttons[4];
  bool focusedInc = (focusedControl == 4);
  if (focusedInc)
    display->display.fillRect(btnInc.x, btnInc.y, btnInc.w, btnInc.h, COLOR_FG);
  drawButtonContent(display, BTN_VOL_INC, btnInc, focusedInc);
}

bool MusicScreen::isSymbolButton(int buttonIndex) const {
  return buttonIndex == BTN_PREV || buttonIndex == BTN_PLAY ||
         buttonIndex == BTN_NEXT || buttonIndex == BTN_LOOP ||
         buttonIndex == BTN_PAGE_UP || buttonIndex == BTN_PAGE_DOWN;
}

const uint8_t *MusicScreen::getButtonFont(int buttonIndex) const {
  if (buttonIndex == BTN_PREV || buttonIndex == BTN_PLAY ||
      buttonIndex == BTN_NEXT) {
    return u8g2_font_open_iconic_play_2x_t;
  }
  if (buttonIndex == BTN_LOOP || buttonIndex == BTN_PAGE_UP ||
      buttonIndex == BTN_PAGE_DOWN) {
    return u8g2_font_open_iconic_arrow_2x_t;
  }
  return u8g2_font_helvB10_tf;
}

uint16_t MusicScreen::getButtonGlyph(int buttonIndex) const {
  using namespace MusicGlyphs;
  // 关键逻辑：u8g2 的 Open Iconic 字形是按官方图标顺序映射到 ASCII，
  // 这里显式绑定每个按钮对应的 glyph，彻底替换此前的几何手绘符号。
  switch (buttonIndex) {
  case BTN_PREV:
    return PREV;
  case BTN_PLAY:
    return music->isPlaying() ? PAUSE : PLAY;
  case BTN_NEXT:
    return NEXT;
  case BTN_LOOP:
    return LOOP;
  case BTN_PAGE_UP:
    return PAGE_UP;
  case BTN_PAGE_DOWN:
    return PAGE_DOWN;
  default:
    return 0;
  }
}

void MusicScreen::drawButtonContent(DisplayDriver *display, int buttonIndex,
                                    const UIButton &btn, bool focused) {
  if (!isSymbolButton(buttonIndex)) {
    drawButtonLabel(display, btn, buttons[buttonIndex].label,
                    u8g2_font_helvB10_tf, focused);
    return;
  }
  if (buttonIndex == BTN_LOOP) {
    drawLoopButton(display, btn, focused);
    return;
  }
  drawButtonGlyph(display, btn, getButtonFont(buttonIndex),
                  getButtonGlyph(buttonIndex), focused);
}

void MusicScreen::drawButtonLabel(DisplayDriver *display, const UIButton &btn,
                                  const char *text, const uint8_t *font,
                                  bool focused, int offsetX, int offsetY) {
  using namespace MusicLayout;
  display->u8g2Fonts.setFontMode(1);
  display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(focused ? COLOR_FG : COLOR_BG);
  display->u8g2Fonts.setFont(font);

  int tw = display->u8g2Fonts.getUTF8Width(text);
  int ascent = display->u8g2Fonts.getFontAscent();
  int descent = display->u8g2Fonts.getFontDescent();
  int baseline = btn.y + (btn.h + ascent + descent) / 2 + offsetY;
  display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2 + offsetX, baseline);
  display->u8g2Fonts.print(text);

  display->u8g2Fonts.setFontMode(1);
  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(COLOR_BG);
}

void MusicScreen::drawButtonGlyph(DisplayDriver *display, const UIButton &btn,
                                  const uint8_t *font, uint16_t glyph,
                                  bool focused, int offsetX, int offsetY) {
  using namespace MusicLayout;
  display->u8g2Fonts.setFontMode(1);
  display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(focused ? COLOR_FG : COLOR_BG);
  display->u8g2Fonts.setFont(font);

  int glyphWidth = u8g2_GetGlyphWidth(&(display->u8g2Fonts.u8g2), glyph);
  int ascent = display->u8g2Fonts.getFontAscent();
  int descent = display->u8g2Fonts.getFontDescent();
  int baseline = btn.y + (btn.h + ascent + descent) / 2 + offsetY;
  int x = btn.x + (btn.w - glyphWidth) / 2 + offsetX;
  display->u8g2Fonts.drawGlyph(x, baseline, glyph);

  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(COLOR_BG);
}

void MusicScreen::drawLoopButton(DisplayDriver *display, const UIButton &btn,
                                 bool focused) {
  // 关键逻辑：循环按钮的三态统一复用同一个 loop 图标，
  // 再用小号字体叠加 “1” 或 “/”，保证整个按钮仍然是字体方案，
  // 同时避免退回 ALL/ONE/OFF 这类文本。
  drawButtonGlyph(display, btn, u8g2_font_open_iconic_arrow_2x_t,
                  MusicGlyphs::LOOP, focused);

  if (music->getLoopMode() == LOOP_ONE) {
    drawButtonLabel(display, btn, "1", u8g2_font_5x8_tf, focused, 8, 7);
    return;
  }
  if (music->getLoopMode() == LOOP_NONE) {
    drawButtonLabel(display, btn, "/", u8g2_font_helvB08_tf, focused, 0, 1);
  }
}

void MusicScreen::setAlignedPartialWindow(DisplayDriver *display, int x, int y,
                                          int w, int h) {
  using namespace MusicLayout;
  int alignedX = max(0, (x / 8) * 8);
  int right = min(SCREEN_W, x + w);
  int alignedRight = min(SCREEN_W, ((right + 7) / 8) * 8);
  display->display.setPartialWindow(alignedX, y, alignedRight - alignedX, h);
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
  setAlignedPartialWindow(display, 15, y, SCREEN_W - 140, 25);
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
  } while (display->display.nextPage());
}

void MusicScreen::updateFocus(DisplayDriver *display, int oldIdx, int newIdx) {
  bool needsLeftRefresh =
      (oldIdx >= BTN_PREV && oldIdx <= BTN_NEXT) ||
      (newIdx >= BTN_PREV && newIdx <= BTN_NEXT);
  bool needsFooterRefresh =
      (oldIdx >= BTN_VOL_DEC && oldIdx <= BTN_LOOP) ||
      (newIdx >= BTN_VOL_DEC && newIdx <= BTN_LOOP);
  bool needsRightRefresh =
      (oldIdx >= BTN_LIST && oldIdx <= BTN_PAGE_DOWN) ||
      (newIdx >= BTN_LIST && newIdx <= BTN_PAGE_DOWN);

  // 关键逻辑：Music 页不同控件的完整绘制方式并不一致。
  // 例如播放列表区域并不是一个普通按钮，循环按钮展示的也是动态文案 ALL/ONE/OFF。
  // 如果焦点变化时直接按“通用按钮文本”局刷，会把列表区域误画成 List，
  // 也会把循环模式错误覆盖成 L，最终表现为按钮文本乱码或内容错乱。
  // 这里改为按功能区域重绘，确保局刷与首屏全量渲染走同一套绘制逻辑。
  if (needsLeftRefresh) {
    updatePlaybackInfo(display);
  }
  if (needsFooterRefresh) {
    updateFooterInfo(display);
  }
  if (needsRightRefresh) {
    updatePlaylist(display);
  }
}

void MusicScreen::updatePlaylist(DisplayDriver *display) {
  using namespace MusicLayout;
  setAlignedPartialWindow(display, PANEL_LEFT_W + 1, SYS_BAR_H,
                          PANEL_RIGHT_W - 1, LAYOUT_H);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawRightPanel(display);
  } while (display->display.nextPage());
}

void MusicScreen::updatePlaybackInfo(DisplayDriver *display) {
  using namespace MusicLayout;
  setAlignedPartialWindow(display, 0, SYS_BAR_H, PANEL_LEFT_W - 1, LAYOUT_H);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawLeftPanel(display);
  } while (display->display.nextPage());
}

void MusicScreen::updateFooterInfo(DisplayDriver *display) {
  using namespace MusicLayout;
  setAlignedPartialWindow(display, 0, SCREEN_H - FOOTER_H + 1, SCREEN_W,
                          FOOTER_H - 1);
  display->display.firstPage();
  do {
    display->display.fillScreen(COLOR_BG);
    drawFooter(display);
  } while (display->display.nextPage());
}
