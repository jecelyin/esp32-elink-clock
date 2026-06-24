#include "RadioScreen.h"
#include "../../config.h"
#include <Arduino.h>

namespace {
struct DialGeometry {
  float minFreq;
  int totalTicks;
  int viewOffset;
  int pointerX;
};

DialGeometry buildDialGeometry(RadioDriver *radio, float centerFreq, int width,
                               int margin, int tickSpacing) {
  float minFreq = radio->getMinFrequency() / 100.0f;
  float maxFreq = radio->getMaxFrequency() / 100.0f;
  if (minFreq <= 0 || maxFreq <= minFreq) {
    minFreq = 87.5f;
    maxFreq = 108.0f;
  }

  int totalTicks = (int)round((maxFreq - minFreq) * 10);
  int visibleW = width - 2 * margin;
  int totalW = totalTicks * tickSpacing;
  int idealPx = (int)round((centerFreq - minFreq) * 10 * tickSpacing);
  int viewOffset = constrain(idealPx - (visibleW / 2), 0,
                             max(0, totalW - visibleW));

  // 关键逻辑：刻度尺可以平移，但指针必须落在当前可视窗口中，
  // 否则靠近频段边界时会出现频率数字更新、指针却跑出屏幕的错觉。
  DialGeometry geometry = {minFreq, totalTicks, viewOffset,
                           idealPx - viewOffset + margin};
  return geometry;
}

bool isMHzTick(float freq) { return abs(freq - round(freq)) < 0.01f; }

bool isHalfMHzTick(float freq) {
  return !isMHzTick(freq) && abs(freq - (floor(freq) + 0.5f)) < 0.01f;
}

int getTickHeight(float freq) {
  if (isMHzTick(freq)) {
    return 16;
  }
  return isHalfMHzTick(freq) ? 10 : 6;
}

void drawDialLabel(DisplayDriver *display, float freq, int tickX, int y,
                   int width) {
  if (!isMHzTick(freq)) {
    return;
  }

  char numStr[8];
  sprintf(numStr, "%d", (int)round(freq));
  int textW = display->u8g2Fonts.getUTF8Width(numStr);
  int labelX = tickX - textW / 2;
  if (labelX + textW <= 0 || labelX >= width) {
    return;
  }

  display->u8g2Fonts.setCursor(labelX, y + 30);
  display->u8g2Fonts.print(numStr);
}

void drawDialTicks(DisplayDriver *display, const DialGeometry &geometry, int y,
                   int width, int margin, int tickSpacing) {
  int tickY = y + 2;
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);

  for (int i = 0; i <= geometry.totalTicks; i++) {
    float freq = geometry.minFreq + (i * 0.1f);
    int tickX = (i * tickSpacing) - geometry.viewOffset + margin;
    if (tickX < margin - 20 || tickX >= width - margin + 20) {
      continue;
    }

    int tickH = getTickHeight(freq);
    if (tickX >= 0 && tickX < width) {
      display->display.fillRect(tickX, tickY, 1, tickH, Layout::COLOR_FG);
    }
    drawDialLabel(display, freq, tickX, y, width);
  }
}

void drawDialPointer(DisplayDriver *display, int pointerX, int y, int height) {
  int triBaseY = y + height - 2;
  display->display.drawLine(pointerX, y, pointerX, triBaseY - 6,
                            Layout::COLOR_FG);
  display->display.fillTriangle(pointerX, triBaseY - 6, pointerX - 5, triBaseY,
                                pointerX + 5, triBaseY, Layout::COLOR_FG);
}
} // namespace

RadioScreen::RadioScreen(RadioDriver *radio, StatusBar *statusBar,
                         ConfigManager *config)
    : radio(radio), statusBar(statusBar), config(config) {
  focusedControl = 0;
  lastFocusedControl = -1;
  lastFreq = 0;
  lastVol = -1;
  lastRDS = "";
  lastRSSI = -1;
  lastStereo = false;
  smoothedRSSI = 0;
  presetPage = 0;
  isFirstDraw = true;
  radioStatus = "";
  radioStatusExpiresAt = 0;
  initButtons();
}

void RadioScreen::init() {}

void RadioScreen::enter() {
  digitalWrite(AMP_EN, 1);
  digitalWrite(RADIO_EN, 1);
  radio->setup();
  restoreSavedState();
  isFirstDraw = true;
}

void RadioScreen::exit() {
  saveFocusIfChanged();
  radio->powerDown();
  digitalWrite(AMP_EN, 0);
  digitalWrite(RADIO_EN, 0);
}

void RadioScreen::update() {
  // 关键逻辑：Radio 页会频繁屏刷和响应按键，周期性读取收音机调试信息
  // 会与屏幕/I2C/输入处理抢占时间片，实机上已观察到 TG1WDT reset。
  // 业务需要的日志已经放在 Seek/Scan 操作点，update 只保留 RDS 轮询。
  radio->checkRDS();
  clearExpiredRadioStatus();
}

void RadioScreen::draw(DisplayDriver *display) {
  uint16_t freq = radio->getFrequency();
  int vol = config->config.volume;

  RDA5807M_Info radioInfo;
  radio->getRadioInfo(&radioInfo);
  String rds = buildRDSLabel(radioInfo);
  updateSmoothedRSSI(radioInfo.rssi);

  if (isFirstDraw) {
    drawFullRefresh(display, freq, vol, radioInfo.stereo, radioInfo.rssi,
                    rds.c_str());
    return;
  }

  drawPartialRefresh(display, freq, vol, radioInfo.stereo, rds.c_str());
}

String RadioScreen::buildRDSLabel(const RDA5807M_Info &radioInfo) {
  if (radioStatus.length() > 0) {
    return radioStatus;
  }

  if (radioInfo.rds) {
    const char *ps = radio->getRDSStationName();
    if (ps && ps[0] && strcmp(ps, "        ") != 0) {
      return ps;
    }
  }
  return "";
}

void RadioScreen::updateSmoothedRSSI(int rssi) {
  if (smoothedRSSI == 0) {
    smoothedRSSI = rssi;
  } else {
    smoothedRSSI = (smoothedRSSI * 7 + rssi) / 8;
  }
}

void RadioScreen::drawFullRefresh(DisplayDriver *display, uint16_t freq,
                                  int vol, bool isStereo, int rssi,
                                  const char *rds) {
  isFirstDraw = false;
  lastFreq = freq;
  lastVol = vol;
  lastRDS = rds;
  lastRSSI = smoothedRSSI;
  lastStereo = isStereo;
  lastFocusedControl = focusedControl;

  display->display.setFullWindow();
  display->display.firstPage();
  do {
    display->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(display, true);
    drawHeaderInfo(display, vol, isStereo, rssi, false);
    drawFrequency(display, false);
    drawDial(display, freq / 100.0f, false);
    drawSignal(display, smoothedRSSI, false);
    drawRDS(display, rds, false);
    drawButtons(display, false);
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::drawPartialRefresh(DisplayDriver *display, uint16_t freq,
                                     int vol, bool isStereo, const char *rds) {
  if (freq != lastFreq) {
    lastFreq = freq;
    updateFrequency(display);
  }

  if (vol != lastVol || isStereo != lastStereo) {
    lastVol = vol;
    lastStereo = isStereo;
    updateHeader(display, vol, isStereo, smoothedRSSI);
  }

  if (abs(smoothedRSSI - lastRSSI) >= 5) { // Allow small RSSI variation
    lastRSSI = smoothedRSSI;
    updateSignal(display, smoothedRSSI);
  }

  if (String(rds) != lastRDS || radio->hasRDSChanged()) {
    lastRDS = rds;
    updateRDS(display, rds);
  }

  if (focusedControl != lastFocusedControl) {
    updateButtonFocus(display, lastFocusedControl, focusedControl);
    lastFocusedControl = focusedControl;
  }
}

void RadioScreen::setupWindow(DisplayDriver *display, int x, int y, int w,
                              int h, bool partial) {
  if (partial) {
    setAlignedPartialWindow(display, x, y, w, h);
  }
  display->display.fillRect(x, y, w, h, Layout::COLOR_BG);
}

void RadioScreen::setAlignedPartialWindow(DisplayDriver *display, int x, int y,
                                          int w, int h) {
  using namespace Layout;
  // 关键逻辑：SSD1619A 的 RAM 横向按 8 像素一字节寻址。局刷窗口若从
  // 非字节边界开始，驱动虽然会补齐刷新范围，但 UI 层分页缓存仍按原窗口
  // 绘制，容易在大字号频率和按钮边缘留下错位残影。
  int alignedX = max(0, (x / 8) * 8);
  int right = min(SCREEN_W, x + w);
  int alignedRight = min(SCREEN_W, ((right + 7) / 8) * 8);
  display->display.setPartialWindow(alignedX, y, alignedRight - alignedX, h);
}

void RadioScreen::restoreSavedState() {
  radio->setVolume(config->config.volume);
  presetPage = constrain(config->config.radio_preset_page, 0,
                         getPresetPageCount() - 1);
  updatePresetButtonLabels();
  focusedControl = constrain(config->config.radio_focus_index, 0,
                             BUTTON_COUNT - 1);
  lastFocusedControl = -1;
  radioStatus = "";
  radioStatusExpiresAt = 0;
}

void RadioScreen::saveFocusIfChanged() {
  if (config->config.radio_focus_index == focusedControl &&
      config->config.radio_preset_page == presetPage) {
    return;
  }

  // 关键逻辑：焦点位置和预设页共同决定底部按钮的真实电台编号；
  // 只保存焦点会让切回页面后同一个按钮加载到另一页的电台。
  config->config.radio_focus_index = focusedControl;
  config->config.radio_preset_page = presetPage;
  config->save();
}

bool RadioScreen::hasExpiredRadioStatus(uint32_t now) const {
  return radioStatus.length() > 0 &&
         (int32_t)(now - radioStatusExpiresAt) >= 0;
}

void RadioScreen::clearExpiredRadioStatus() {
  if (!hasExpiredRadioStatus(millis())) {
    return;
  }

  radioStatus = "";
  radioStatusExpiresAt = 0;

  DisplayDriver *display =
      uiManager == nullptr ? nullptr : uiManager->getDisplayDriver();
  if (display == nullptr) {
    return;
  }

  // 关键逻辑：提示消息和 RDS 共用同一块屏幕区域；提示过期后必须立刻
  // 用当前 RDS 文本重绘，若 RDS 为空则传空串清掉整块区域。
  RDA5807M_Info radioInfo;
  radio->getRadioInfo(&radioInfo);
  String rds = buildRDSLabel(radioInfo);
  lastRDS = rds;
  updateRDS(display, rds.c_str());
}

void RadioScreen::drawFrequency(DisplayDriver *display, bool partial) {
  using namespace Layout;
  int x = 0;
  int y = 54;
  int w = DISPLAY_AREA_W;
  int h = MAIN_SECTION_H - DIAL_H;

  display->u8g2Fonts.setFont(u8g2_font_logisoso78_tn);
  char freqStr[12];
  radio->getFormattedFrequency(freqStr, sizeof(freqStr));
  int16_t tw = display->u8g2Fonts.getUTF8Width(freqStr);
  int cursorX = x + (w - tw) / 2;
  int cursorY = y + 66;
  // Use fixed full width to ensure reliable clearing of old text
  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setCursor(cursorX, cursorY);
  display->u8g2Fonts.print(freqStr);
}

void RadioScreen::drawDial(DisplayDriver *display, float centerFreq,
                           bool partial) {
  using namespace Layout;
  const int margin = 20;
  const int tickSpacing = 8;
  int x = 0;
  int y = DIAL_Y;
  int w = DISPLAY_AREA_W;
  int h = DIAL_H;

  setupWindow(display, x, y, w, h, partial);
  display->display.drawLine(0, DIAL_Y, DISPLAY_AREA_W - 1, DIAL_Y, COLOR_FG);

  DialGeometry geometry = buildDialGeometry(radio, centerFreq, w, margin,
                                            tickSpacing);
  drawDialTicks(display, geometry, y, w, margin, tickSpacing);
  drawDialPointer(display, geometry.pointerX, y, h);
}

void RadioScreen::drawSignal(DisplayDriver *display, int rssi, bool partial) {
  using namespace Layout;
  int x = DISPLAY_AREA_W + 2;
  int y = MAIN_SECTION_Y + 2;
  int w = VOL_BAR_W - 4;
  int h = MAIN_SECTION_H;

  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);

  // Display specific signal strength (RSSI) value and unit
  char valStr[16];
  sprintf(valStr, "%d", rssi);
  int tw = display->u8g2Fonts.getUTF8Width(valStr);
  display->u8g2Fonts.setCursor(x + (w - tw) / 2, y + 30);
  display->u8g2Fonts.print(valStr);

  const char *unit = "dB";
  int uw = display->u8g2Fonts.getUTF8Width(unit);
  display->u8g2Fonts.setCursor(x + (w - uw) / 2, y + 43);
  display->u8g2Fonts.print(unit);

  int topMargin = 40;
  int segH = (h - topMargin - 10) / 20;
  int startY = y + h - 5;

  static const uint8_t rssi_thresholds[20] = {10, 12, 14, 16, 18, 20, 22,
                                              24, 26, 28, 30, 32, 34, 36,
                                              38, 40, 42, 45, 48, 51};

  int bars = 0;
  for (int i = 19; i >= 0; i--) {
    if (rssi >= rssi_thresholds[i]) {
      bars = i + 1;
      break;
    }
  }

  for (int i = 0; i < 20; i++) {
    int boxY = startY - ((i + 1) * segH);
    display->display.drawRect(x + 5, boxY, w - 10, segH - 2, COLOR_FG);
    if (i < bars) {
      display->display.fillRect(x + 5, boxY, w - 10, segH - 2, COLOR_FG);
    }
  }
}

void RadioScreen::drawHeaderInfo(DisplayDriver *display, int vol, bool isStereo,
                                 int rssi, bool partial) {
  using namespace Layout;
  int x = 5;
  int y = 24;
  int w = DISPLAY_AREA_W - 4;
  int h = 30;

  // Calculate area to refresh for ST/MO and Volume
  // Offset a bit from left (e.g. 10px)
  int infoX = x + 8;
  int infoY = y + 5;
  int infoW = 80; // Enough for ST icon + VOL: xx
  int infoH = 20;

  setupWindow(display, infoX, infoY, infoW, infoH, partial);

  // Draw Stereo/Mono icon
  int iconX = infoX + 10;
  int iconY = infoY + 10;
  if (isStereo) {
    display->display.drawCircle(iconX - 3, iconY, 6, COLOR_FG);
    display->display.drawCircle(iconX + 3, iconY, 6, COLOR_FG);
  } else {
    display->display.drawCircle(iconX, iconY, 6, COLOR_FG);
  }

  // Draw Volume value
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  char volStr[5];
  sprintf(volStr, "%d", vol);
  display->u8g2Fonts.setCursor(iconX + 15, infoY + 15);
  display->u8g2Fonts.print(volStr);
}

void RadioScreen::drawRDS(DisplayDriver *display, const char *text,
                          bool partial) {
  using namespace Layout;

  int x = 0;
  int y = MAIN_SECTION_Y + MAIN_SECTION_H;
  int w = DISPLAY_AREA_W;
  int h = 20;

  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int tw = display->u8g2Fonts.getUTF8Width(text);
  int textX = x + (w - tw) / 2;

  display->u8g2Fonts.setCursor(textX, y + 15);
  display->u8g2Fonts.print(text);
}

void RadioScreen::updateFrequency(DisplayDriver *display) {
  using namespace Layout;
  uint16_t freq = radio->getFrequency();
  float freqVal = freq / 100.0;

  int x = 0;
  int y = 54;
  int w = DISPLAY_AREA_W;
  int h = DIAL_Y + DIAL_H - y;

  // 关键逻辑：频率数字和刻度尺在视觉上是一组状态，拆成两个局刷窗口时，
  // 大字号数字残影和刻度指针会在窗口交界处不同步。这里合并为一个连续
  // 且按 8 像素对齐的窗口，让清屏、重绘、刷屏使用同一块区域。
  setAlignedPartialWindow(display, x, y, w, h);
  display->display.firstPage();
  do {
    display->display.fillRect(x, y, w, h, Layout::COLOR_BG);
    drawFrequency(display, false);
    drawDial(display, freqVal, false);
  } while (display->display.nextPage());

  display->powerOff();
}

void RadioScreen::updateHeader(DisplayDriver *display, int vol, bool isStereo,
                               int rssi) {
  using namespace Layout;
  // Same rect as drawHeaderInfo
  int x = 5;
  int y = 24;
  // int w = DISPLAY_AREA_W - 4;
  // int h = 30;

  // We only need to update the specific info area:
  int infoX = x + 8;
  int infoY = y + 5;
  int infoW = 80;
  int infoH = 20;

  setAlignedPartialWindow(display, infoX, infoY, infoW, infoH);
  display->display.firstPage();
  do {
    drawHeaderInfo(display, vol, isStereo, rssi, false);
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::updateSignal(DisplayDriver *display, int rssi) {
  using namespace Layout;
  int x = DISPLAY_AREA_W + 2;
  int y = MAIN_SECTION_Y + 2;
  int w = VOL_BAR_W - 4;
  int h = MAIN_SECTION_H;

  setAlignedPartialWindow(display, x, y, w, h);
  display->display.firstPage();
  do {
    drawSignal(display, rssi, false);
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::updateRDS(DisplayDriver *display, const char *text) {
  using namespace Layout;
  int y = MAIN_SECTION_Y + MAIN_SECTION_H;
  int w = DISPLAY_AREA_W;
  int h = 20;
  int x = 0;

  setAlignedPartialWindow(display, x, y, w, h);
  display->display.firstPage();
  do {
    drawRDS(display, text, false);
  } while (display->display.nextPage());
  display->powerOff();
}
