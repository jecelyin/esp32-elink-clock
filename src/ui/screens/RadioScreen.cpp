#include "RadioScreen.h"
#include "../../config.h"
#include <Arduino.h>

namespace Layout {
const int SCREEN_W = 400;
const int SCREEN_H = 300;
const uint16_t COLOR_BG = GxEPD_WHITE;
const uint16_t COLOR_FG = GxEPD_BLACK;
const int SYSTEM_BAR_H = 24;
const int MAIN_SECTION_Y = 24;
const int MAIN_SECTION_H = 176;
const int VOL_BAR_W = 40;
const int DISPLAY_AREA_W = SCREEN_W - VOL_BAR_W;
const int DIAL_Y = 160;
const int DIAL_H = 40;
const int CONTROLS_Y = 200;
const int CONTROLS_H = 100;
} // namespace Layout

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
  isFirstDraw = true;
  initButtons();
}

void RadioScreen::initButtons() {
  using namespace Layout;
  int cellW = 100;
  int cellH = 50;
  int startY = CONTROLS_Y + 1;

  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (i < 2) {
      // Row 1: SEEK
      buttons[i].x = i * cellW;
      buttons[i].y = startY;
      buttons[i].w = cellW + 1; // +1 for overlap
      buttons[i].h = cellH + 1; // +1 for overlap
      if (i == 0)
        strcpy(buttons[i].label, "<< SEEK");
      else
        strcpy(buttons[i].label, "SEEK >>");
    } else if (i < 5) {
      // Row 2: VOL and BIAS
      int w1 = 66;
      int w2 = 67;
      int w3 = 67;
      if (i == 2) {
        buttons[i].x = 0;
        buttons[i].w = w1 + 1;
      } else if (i == 3) {
        buttons[i].x = w1;
        buttons[i].w = w2 + 1;
      } else if (i == 4) {
        buttons[i].x = w1 + w2;
        buttons[i].w = w3 + 1;
      }
      buttons[i].y = startY + cellH;
      buttons[i].h = cellH + 1;
      if (i == 2)
        strcpy(buttons[i].label, "VOL -");
      else if (i == 3)
        strcpy(buttons[i].label, "VOL +");
      else if (i == 4)
        strcpy(buttons[i].label, radio->getBias() ? "BIAS\nON" : "BIAS\nOFF");
    } else {
      // Presets
      int pIdx = i - 5;
      int pW = 50;
      int pH = 50;
      int startX = 200;
      buttons[i].x = startX + (pIdx % 4) * pW;
      buttons[i].y = startY + (pIdx / 4) * pH;
      buttons[i].w = pW + 1;
      buttons[i].h = pH + 1;
      sprintf(buttons[i].label, "%d", pIdx + 1);
    }
  }
}

void RadioScreen::drawSingleButton(DisplayDriver *display, int idx,
                                   bool isFocused, bool partial) {
  using namespace Layout;
  UIButton &btn = buttons[idx];

  // if (partial) {
  //   setupWindow(display, btn.x, btn.y, btn.w, btn.h, true);
  // }

  if (isFocused)
    display->display.fillRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
  display->display.drawRect(btn.x, btn.y, btn.w, btn.h, COLOR_FG);
  Serial.printf("Draw button %d at (%d,%d) size (%d,%d) %s\n", idx, btn.x,
                btn.y, btn.w, btn.h, isFocused ? "FOCUSED" : "normal");
  display->u8g2Fonts.setForegroundColor(isFocused ? COLOR_BG : COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(isFocused ? COLOR_FG : COLOR_BG);
  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);

  // Handle multiline labels
  char labelCopy[16];
  strncpy(labelCopy, btn.label, 16);
  char *line = strtok(labelCopy, "\n");
  int lineCount = 0;
  char *lines[2]; // Support max 2 lines for now
  while (line != NULL && lineCount < 2) {
    lines[lineCount++] = line;
    line = strtok(NULL, "\n");
  }

  if (lineCount == 1) {
    int tw = display->u8g2Fonts.getUTF8Width(btn.label);
    display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2,
                                 btn.y + btn.h / 2 + 5);
    display->u8g2Fonts.print(btn.label);
  } else if (lineCount == 2) {
    for (int i = 0; i < 2; i++) {
      int tw = display->u8g2Fonts.getUTF8Width(lines[i]);
      // Centering logic for 2 lines:
      // Line 1: y = center - offset, Line 2: y = center + offset
      int yOffset = (i == 0) ? -4 : 12;
      display->u8g2Fonts.setCursor(btn.x + (btn.w - tw) / 2,
                                   btn.y + btn.h / 2 + yOffset);
      display->u8g2Fonts.print(lines[i]);
    }
  }

  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(COLOR_BG);
}

void RadioScreen::updateButtonFocus(DisplayDriver *display, int oldIdx,
                                    int newIdx) {
  // Update old focus
  if (oldIdx != -1 && oldIdx != newIdx) {
    UIButton &btn = buttons[oldIdx];
    display->display.setPartialWindow(btn.x + 1, btn.y + 1, btn.w - 2,
                                      btn.h - 2);
    display->display.firstPage();
    do {
      drawSingleButton(display, oldIdx, false, true);
      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
    display->powerOff();
  }

  // Update new focus
  UIButton &btn = buttons[newIdx];
  display->display.setPartialWindow(btn.x + 1, btn.y + 1, btn.w - 2, btn.h - 2);
  display->display.firstPage();
  do {
    drawSingleButton(display, newIdx, true, true);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::init() {}

void RadioScreen::enter() {
  digitalWrite(AMP_EN, 1);
  digitalWrite(RADIO_EN, 1);
  radio->setup();
  isFirstDraw = true;
}

void RadioScreen::exit() {
  radio->powerDown();
  digitalWrite(AMP_EN, 0);
  digitalWrite(RADIO_EN, 0);
}

void RadioScreen::update() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 2000) {
    lastLog = millis();
    radio->debugRadioInfo();
  }
  radio->checkRDS();
}

void RadioScreen::draw(DisplayDriver *display) {
  using namespace Layout;
  // radio->setFrequency(10520); // For testing
  uint16_t freq = radio->getFrequency();
  float freqVal = freq / 100.0;
  int vol = config->config.volume;
  radio->setVolume(vol);
  RDA5807M_Info radioInfo;
  radio->getRadioInfo(&radioInfo);

  String rds = "";
  if (radioInfo.rds) {
    const char *ps = radio->getRDSStationName();
    const char *rt = radio->getRDSText();
    if (ps && ps[0] && strcmp(ps, "        ") != 0) {
      rds = ps;
    } else {
      rds = "RDS found!";
    }
  }
  int rssi = radioInfo.rssi;

  // Smooth RSSI
  if (smoothedRSSI == 0)
    smoothedRSSI = rssi;
  else
    smoothedRSSI = (smoothedRSSI * 7 + rssi) / 8;

  bool isStereo = radioInfo.stereo;

  if (isFirstDraw) {
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
      drawStaticGrid(display);
      drawHeaderInfo(display, vol, isStereo, rssi, false);
      drawFrequency(display, false);
      drawDial(display, freqVal, false);
      drawSignal(display, smoothedRSSI, false);
      drawRDS(display, rds.c_str(), false);
      drawButtons(display, false);
      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
    display->powerOff();
    return;
  }

  // Partial updates
  if (freq != lastFreq) {
    lastFreq = freq;
    updateFrequency(display);
  }

  if (vol != lastVol || isStereo != lastStereo) {
    lastVol = vol;
    lastStereo = isStereo;
    updateHeader(display, vol, isStereo,
                 smoothedRSSI); // RSSI needed for header? No, only stereo/vol
  }

  if (abs(smoothedRSSI - lastRSSI) >= 5) { // Allow small RSSI variation
    lastRSSI = smoothedRSSI;
    updateSignal(display, smoothedRSSI);
  }

  if (rds != lastRDS || radio->hasRDSChanged()) {
    lastRDS = rds;
    updateRDS(display, rds.c_str());
  }

  if (focusedControl != lastFocusedControl) {
    updateButtonFocus(display, lastFocusedControl, focusedControl);
    lastFocusedControl = focusedControl;
  }
}

void RadioScreen::setupWindow(DisplayDriver *display, int x, int y, int w,
                              int h, bool partial) {
  if (partial) {
    display->display.setPartialWindow(x, y, w, h);
  }
  display->display.fillRect(x, y, w, h, Layout::COLOR_BG);
}

void RadioScreen::drawStaticGrid(DisplayDriver *display) {
  using namespace Layout;

  // display->display.drawRect(0, SYSTEM_BAR_H, SCREEN_W, SCREEN_H -
  // SYSTEM_BAR_H,
  //                           COLOR_FG);

  // display->display.drawLine(0, SYSTEM_BAR_H, SCREEN_W, SYSTEM_BAR_H,
  // COLOR_FG);

  // display->display.drawLine(0, CONTROLS_Y, SCREEN_W, CONTROLS_Y, COLOR_FG);

  // display->display.drawLine(SCREEN_W / 2, CONTROLS_Y, SCREEN_W / 2, SCREEN_H,
  //                           COLOR_FG);
  // display->display.drawLine(SCREEN_W / 2 + (SCREEN_W / 4), CONTROLS_Y,
  //                           SCREEN_W / 2 + (SCREEN_W / 4), SCREEN_H,
  //                           COLOR_FG);
  // display->display.drawLine(SCREEN_W / 4, CONTROLS_Y, SCREEN_W / 4, SCREEN_H,
  //                           COLOR_FG);
}

void RadioScreen::drawFrequency(DisplayDriver *display, bool partial) {
  using namespace Layout;
  int x = 2; // Stay inside grid lines at 0, 1
  int y = 54;
  int w = DISPLAY_AREA_W - 4; // Stay inside divider line at 360, 361
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

  // Dial Configuration
  const int margin =
      20; // Padding on left/right to prevent labels being cut off
  int x = margin / 2;
  int y = DIAL_Y;
  int w = DISPLAY_AREA_W - 4;
  int h = DIAL_H;
  const int visibleW = w - 2 * margin;
  const int tickSpacing = 8; // Pixels per 0.1MHz step
  const int tickH01 = 6;     // Height for 0.1MHz tick
  const int tickH05 = 10;    // Height for 0.5MHz tick
  const int tickH10 = 16;    // Height for 1.0MHz tick

  setupWindow(display, x, y, w, h, partial);

  display->display.drawLine(0, DIAL_Y, DISPLAY_AREA_W, DIAL_Y, COLOR_FG);

  float minBandFreq = radio->getMinFrequency() / 100.0;
  float maxBandFreq = radio->getMaxFrequency() / 100.0;
  if (minBandFreq <= 0 || maxBandFreq <= 0 || maxBandFreq <= minBandFreq) {
    minBandFreq = 87.5;
    maxBandFreq = 108.0;
  }

  int totalTicks = (int)round((maxBandFreq - minBandFreq) * 10);
  int totalW = totalTicks * tickSpacing;

  // Calculate potential pointer position in the full dial width
  int idealPx = (int)round((centerFreq - minBandFreq) * 10 * tickSpacing);

  // Calculate view offset to keep pointer centered if possible
  int viewOffset = idealPx - (visibleW / 2);
  viewOffset = constrain(viewOffset, 0, max(0, totalW - visibleW));

  // Final positions relative to the window, shifted by margin
  int pointerX = idealPx - viewOffset + margin;

  // Draw Ticks
  int tickY = y + 2;
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);

  for (int i = 0; i <= totalTicks; i++) {
    float f = minBandFreq + (i * 0.1f);
    int tickX = (i * tickSpacing) - viewOffset + margin;

    if (tickX >= margin - 20 && tickX < w - margin + 20) {
      bool is1MHz = (abs(f - round(f)) < 0.01f);
      bool is05MHz = !is1MHz && (abs(f - (floor(f) + 0.5f)) < 0.01f);

      int tickH = is1MHz ? tickH10 : (is05MHz ? tickH05 : tickH01);
      int tickWidth = 1;

      if (tickX >= 0 && tickX < w) {
        display->display.fillRect(tickX - (tickWidth / 2), tickY, tickWidth,
                                  tickH, COLOR_FG);
      }

      // Draw 1MHz Label
      if (is1MHz) {
        char numStr[8];
        sprintf(numStr, "%d", (int)round(f));
        int nw = display->u8g2Fonts.getUTF8Width(numStr);
        int labelX = tickX - nw / 2;

        if (labelX + nw > 0 && labelX < w) {
          display->u8g2Fonts.setCursor(
              labelX, y + 30); // Moved up to make room for triangle
          display->u8g2Fonts.print(numStr);
        }
      }
    }
  }

  // Pointer: 竖线 (Vertical Line) + 三角形 (Triangle at bottom)
  int triBaseY = y + h - 2;
  display->display.drawLine(pointerX, y, pointerX, triBaseY - 6, COLOR_FG);
  display->display.fillTriangle(pointerX, triBaseY - 6, pointerX - 5, triBaseY,
                                pointerX + 5, triBaseY, COLOR_FG);
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

  int y = MAIN_SECTION_Y + MAIN_SECTION_H;
  int w = DISPLAY_AREA_W;
  int h = 20;

  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int tw = display->u8g2Fonts.getUTF8Width(text);
  int x = (w - tw) / 2;
  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setCursor(x, y + 15);
  display->u8g2Fonts.print(text);
}

void RadioScreen::drawButtons(DisplayDriver *display, bool partial) {
  for (int i = 0; i < BUTTON_COUNT; i++) {
    drawSingleButton(display, i, (focusedControl == i), partial);
  }
}

bool RadioScreen::handleInput(UIKey key) {
  bool needsRedraw = true;
  if (key == UI_KEY_LEFT) {
    focusedControl = (focusedControl - 1 + BUTTON_COUNT) % BUTTON_COUNT;
  } else if (key == UI_KEY_RIGHT) {
    focusedControl = (focusedControl + 1) % BUTTON_COUNT;
  } else if (key == UI_KEY_ENTER) {
    if (focusedControl == 0) {
      radio->seekDown();
    } else if (focusedControl == 1) {
      radio->seekUp();
    } else if (focusedControl == 2) {
      if (config->config.volume > 0) {
        config->config.volume--;
        radio->setVolume(config->config.volume);
        config->save();
      }
    } else if (focusedControl == 3) {
      if (config->config.volume < 15) {
        config->config.volume++;
        radio->setVolume(config->config.volume);
        config->save();
      }
    } else if (focusedControl == 4) {
      bool currentBias = radio->getBias();
      radio->setBias(!currentBias);
      strcpy(buttons[4].label, !currentBias ? "BIAS\nON" : "BIAS\nOFF");
      // Trigger a redraw of this button
      isFirstDraw = false; // Ensure we do a partial if possible, though
                           // RadioScreen::draw handles it
    } else if (focusedControl >= 5 && focusedControl <= 12) {
      loadPreset(focusedControl - 5);
    }
  }
  return needsRedraw;
}

bool RadioScreen::onLongPress() {
  if (focusedControl >= 5 && focusedControl <= 12) {
    savePreset(focusedControl - 5);
    return true; // Update UI to show saved state or just refresh
  } else {
    if (uiManager)
      uiManager->switchScreen(SCREEN_MENU);
    return false; // Switch screen handles draw
  }
}

void RadioScreen::savePreset(int index) {
  uint16_t freq = radio->getFrequency();
  config->config.radio_presets[index] = freq;
  config->save();
}

void RadioScreen::loadPreset(int index) {
  uint16_t freq = config->config.radio_presets[index];
  if (freq > 0)
    radio->setFrequency(freq);
}

void RadioScreen::updateFrequency(DisplayDriver *display) {
  using namespace Layout;
  // Frequency area + Dial area
  // We update both as they are visually connected and large updates might be
  // better together Or we can do them separately. Let's do both in one partial
  // window if possible, but they are vertically separated (Freq at 54, Dial at
  // 160). Separate updates might be cleaner or one large update. Let's do
  // separate updates for simplicity of rects.

  uint16_t freq = radio->getFrequency();
  float freqVal = freq / 100.0;

  // 1. Update Frequency Text
  int x = 2;
  int y = 54;
  int w = DISPLAY_AREA_W - 4;
  int h = MAIN_SECTION_H - DIAL_H; // This covers the text area

  display->display.setPartialWindow(x, y, w, h);
  display->display.firstPage();
  do {
    drawFrequency(display, false); // partial=false because we handled window
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());

  // 2. Update Dial
  // Dial Y=160, H=40
  // Margin=10
  int dx = 10;
  int dw = DISPLAY_AREA_W - 20; // Matches visibleW in drawDial roughly
  // Actually drawDial uses margin=20/2 => x=10.
  // drawDial does: setupWindow(display, x, y, w, h, partial);
  // where x=10, y=160, w=356, h=40

  const int margin = 20;
  int dialX = margin / 2;
  int dialY = DIAL_Y;
  int dialW = DISPLAY_AREA_W - 4;
  int dialH = DIAL_H;

  display->display.setPartialWindow(dialX, dialY, dialW, dialH);
  display->display.firstPage();
  do {
    drawDial(display, freqVal, false);
    BusManager::getInstance().requestDisplay();
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

  display->display.setPartialWindow(infoX, infoY, infoW, infoH);
  display->display.firstPage();
  do {
    drawHeaderInfo(display, vol, isStereo, rssi, false);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::updateSignal(DisplayDriver *display, int rssi) {
  using namespace Layout;
  int x = DISPLAY_AREA_W + 2;
  int y = MAIN_SECTION_Y + 2;
  int w = VOL_BAR_W - 4;
  int h = MAIN_SECTION_H;

  display->display.setPartialWindow(x, y, w, h);
  display->display.firstPage();
  do {
    drawSignal(display, rssi, false);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::updateRDS(DisplayDriver *display, const char *text) {
  using namespace Layout;
  int y = MAIN_SECTION_Y + MAIN_SECTION_H;
  int w = DISPLAY_AREA_W;
  int h = 20;
  int x = 0; // drawRDS calculates x centered, but clearing needs full width

  display->display.setPartialWindow(x, y, w, h);
  display->display.firstPage();
  do {
    drawRDS(display, text, false);
    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
  display->powerOff();
}

void RadioScreen::updateButtons(DisplayDriver *display) {
  // This might be used if we need to redraw all buttons
  // Currently we only use updateButtonFocus
}
