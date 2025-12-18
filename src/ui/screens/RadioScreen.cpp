#include "RadioScreen.h"

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
}

RadioScreen::RadioScreen(RadioDriver *radio, StatusBar *statusBar, ConfigManager *config)
    : radio(radio), statusBar(statusBar), config(config) {
  focusedControl = 0;
}

void RadioScreen::init() {
}

void RadioScreen::draw(DisplayDriver *display) {
  uint16_t freq = radio->getFrequency();
  if (freq < radio->getMinFrequency() || freq > radio->getMaxFrequency()) {
    freq = radio->getMinFrequency();
    radio->setFrequency(freq);
  }
  float freqVal = freq / 100.0;
  int vol = config->config.volume;

  String rds = "";
  if (radio->hasRdsInfo()) {
    rds = radio->getRdsStationName();
    rds.trim();
    if (rds.length() == 0) {
      rds = radio->getRdsProgramInformation();
      rds.trim();
    }
  }
  int signal = radio->getSignalStrength();
  int rssi = radio->getRSSI();

  // Periodic Maintenance
  static unsigned long lastClear = 0;
  if (millis() - lastClear > 60000) {
    radio->clearRds();
    lastClear = millis();
  }

  display->display.setFullWindow();
  display->display.firstPage();
  do {
    display->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(display, true);
    drawStaticGrid(display);
    drawHeaderInfo(display, rds.c_str(), signal, rssi, false);
    drawFrequency(display, false);
    drawDial(display, freqVal, false);
    drawVolume(display, vol, false);
    drawRDS(display, rds.c_str(), false);
    drawButtons(display, false);

    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}

void RadioScreen::setupWindow(DisplayDriver *display, int x, int y, int w, int h, bool partial) {
  if (partial) {
    display->display.setPartialWindow(x, y, w, h);
  }
  display->display.fillRect(x, y, w, h, Layout::COLOR_BG);
}

void RadioScreen::drawStaticGrid(DisplayDriver *display) {
  using namespace Layout;

  display->display.drawRect(0, SYSTEM_BAR_H, SCREEN_W, SCREEN_H - SYSTEM_BAR_H, COLOR_FG);
  display->display.drawRect(1, SYSTEM_BAR_H+1, SCREEN_W-2, SCREEN_H - SYSTEM_BAR_H - 2, COLOR_FG);

  display->display.drawLine(0, SYSTEM_BAR_H, SCREEN_W, SYSTEM_BAR_H, COLOR_FG);
  display->display.drawLine(0, SYSTEM_BAR_H+1, SCREEN_W, SYSTEM_BAR_H+1, COLOR_FG);

  display->display.drawLine(0, CONTROLS_Y, SCREEN_W, CONTROLS_Y, COLOR_FG);
  display->display.drawLine(0, CONTROLS_Y+1, SCREEN_W, CONTROLS_Y+1, COLOR_FG);

  display->display.drawLine(DISPLAY_AREA_W, MAIN_SECTION_Y, DISPLAY_AREA_W, CONTROLS_Y, COLOR_FG);
  display->display.drawLine(DISPLAY_AREA_W+1, MAIN_SECTION_Y, DISPLAY_AREA_W+1, CONTROLS_Y, COLOR_FG);

  display->display.drawLine(SCREEN_W/2, CONTROLS_Y, SCREEN_W/2, SCREEN_H, COLOR_FG);
  display->display.drawLine(0, CONTROLS_Y + CONTROLS_H/2, SCREEN_W, CONTROLS_Y + CONTROLS_H/2, COLOR_FG);
  display->display.drawLine(SCREEN_W/2 + (SCREEN_W/4), CONTROLS_Y, SCREEN_W/2 + (SCREEN_W/4), SCREEN_H, COLOR_FG);
  display->display.drawLine(SCREEN_W/4, CONTROLS_Y, SCREEN_W/4, SCREEN_H, COLOR_FG);
}

void RadioScreen::drawFrequency(DisplayDriver *display, bool partial) {
  using namespace Layout;
  int x = 1;
  int y = 54;
  int w = DISPLAY_AREA_W;
  int h = 70;

  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setFont(u8g2_font_logisoso78_tn);
  char* freqStr = radio->getFormattedFrequency();

  int16_t tw = display->u8g2Fonts.getUTF8Width(freqStr);
  int cursorX = (w - tw) / 2;
  int cursorY = y + 60; // Adjusted for 70px window

  display->u8g2Fonts.setCursor(cursorX, cursorY);
  display->u8g2Fonts.print(freqStr);

  // If the library doesn't include MHz, we might need it, but user's snippet just prints it.
  // Actually, logisoso78_tn is numbers only. We might need to switch font or split.
  // Let's check logisoso78_tn coverage. It's usually "tn" for transparent numbers.
  // If it's numbers only, it won't show the dot or MHz.
  // But the previous implementation used it for "101.1".
  // Let's assume the library returns something like "101.10" or "88.0".
}

void RadioScreen::drawDial(DisplayDriver *display, float centerFreq, bool partial) {
  using namespace Layout;
  int x = 0;
  int y = DIAL_Y;
  int w = DISPLAY_AREA_W;
  int h = DIAL_H;

  setupWindow(display, x, y, w, h, partial);
  Serial.print("Center frequency: ");
  Serial.println(centerFreq);

  display->display.drawLine(0, DIAL_Y, DISPLAY_AREA_W, DIAL_Y, COLOR_FG);
  display->display.drawLine(0, DIAL_Y + 1, DISPLAY_AREA_W, DIAL_Y + 1, COLOR_FG);

  float minBandFreq = radio->getMinFrequency() / 100.0;
  float maxBandFreq = radio->getMaxFrequency() / 100.0;
  if (minBandFreq <= 0 || maxBandFreq <= 0 || maxBandFreq <= minBandFreq) {
    minBandFreq = 87.5;
    maxBandFreq = 108.0;
  }

  // Dial Configuration
  const int margin = 20;      // Padding on left/right to prevent labels being cut off
  const int visibleW = w - 2 * margin;
  const int tickSpacing = 8;  // Pixels per 0.1MHz step
  const int tickH01 = 6;      // Height for 0.1MHz tick
  const int tickH05 = 10;     // Height for 0.5MHz tick
  const int tickH10 = 16;     // Height for 1.0MHz tick

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
        display->display.fillRect(tickX - (tickWidth / 2), tickY, tickWidth, tickH, COLOR_FG);
      }

      // Draw 1MHz Label
      if (is1MHz) {
        char numStr[8];
        sprintf(numStr, "%d", (int)round(f));
        int nw = display->u8g2Fonts.getUTF8Width(numStr);
        int labelX = tickX - nw / 2;
        
        if (labelX + nw > 0 && labelX < w) {
           display->u8g2Fonts.setCursor(labelX, y + 30); // Moved up to make room for triangle
           display->u8g2Fonts.print(numStr);
        }
      }
    }
  }

  // Pointer: 竖线 (Vertical Line) + 三角形 (Triangle at bottom)
  int triBaseY = y + h - 2; 
  display->display.drawLine(pointerX, y, pointerX, triBaseY - 6, COLOR_FG);
  display->display.fillTriangle(pointerX, triBaseY - 6, pointerX - 5, triBaseY, pointerX + 5, triBaseY, COLOR_FG);
}

void RadioScreen::drawVolume(DisplayDriver *display, int vol, bool partial) {
  using namespace Layout;
  int x = DISPLAY_AREA_W + 2;
  int y = MAIN_SECTION_Y;
  int w = VOL_BAR_W - 4;
  int h = MAIN_SECTION_H;

  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(x + 4, y + 15);
  display->u8g2Fonts.print("VOL");

  int segH = (h - 25) / 16;
  int startY = y + h - 5;

  for (int i = 0; i < 15; i++) {
    int boxY = startY - ((i + 1) * segH);
    display->display.drawRect(x + 5, boxY, w - 10, segH - 2, COLOR_FG);
    if (i < vol) {
      display->display.fillRect(x + 5, boxY, w - 10, segH - 2, COLOR_FG);
    }
  }
}

void RadioScreen::drawHeaderInfo(DisplayDriver *display, const char* rds, int signal, int rssi, bool partial) {
  using namespace Layout;
  int x = 0;
  int y = 24;
  int w = DISPLAY_AREA_W;
  int h = 30;

  setupWindow(display, x, y, w, h, partial);

  display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  display->u8g2Fonts.setCursor(x + 5, y + 20);
  display->u8g2Fonts.print("FM RADIO");

  // Show RSSI in dB
  char rssiStr[12];
  sprintf(rssiStr, "%ddB", rssi);
  int rw = display->u8g2Fonts.getUTF8Width(rssiStr);
  display->u8g2Fonts.setCursor(w - 70 - rw, y + 20); 
  display->u8g2Fonts.print(rssiStr);

  int sigX = w - 30;
  for (int i = 0; i < 4; i++) {
    int barH = 4 + (i * 3);
    int barX = sigX + (i * 5);
    int barY = y + 20 - barH; 
    if (i < signal) {
      display->display.fillRect(barX, barY, 3, barH, COLOR_FG);
    } else {
      display->display.drawRect(barX, barY, 3, barH, COLOR_FG);
    }
  }
}

void RadioScreen::drawRDS(DisplayDriver *display, const char* text, bool partial) {
  using namespace Layout;
  int x = 0;
  int y = 124;
  int w = DISPLAY_AREA_W;
  int h = 20;

  setupWindow(display, x, y, w, h, partial);
  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int tw = display->u8g2Fonts.getUTF8Width(text);
  display->u8g2Fonts.setCursor((w - tw) / 2, y + 15);
  display->u8g2Fonts.print(text);
}

void RadioScreen::drawButtons(DisplayDriver *display, bool partial) {
  using namespace Layout;
  int cellW = 100;
  int cellH = 50;

  auto drawBtn = [&](int idx, int bx, int by, int bw, int bh, const char* t) {
    bool focused = (focusedControl == idx);
    if (focused) display->display.fillRect(bx, by, bw, bh, COLOR_FG);
    display->display.drawRect(bx, by, bw, bh, COLOR_FG);
    display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
    display->u8g2Fonts.setBackgroundColor(focused ? COLOR_FG : COLOR_BG);
    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    int tw = display->u8g2Fonts.getUTF8Width(t);
    display->u8g2Fonts.setCursor(bx + (bw - tw)/2, by + bh/2 + 5);
    display->u8g2Fonts.print(t);
  };

  drawBtn(0, 0, CONTROLS_Y, cellW, cellH, "<< SEEK");
  drawBtn(1, cellW, CONTROLS_Y, cellW, cellH, "SEEK >>");
  drawBtn(2, 0, CONTROLS_Y + cellH, cellW, cellH, "VOL -");
  drawBtn(3, cellW, CONTROLS_Y + cellH, cellW, cellH, "VOL +");

  int pW = 50;
  int pH = 50;
  int startX = 200;
  for(int i=0; i<8; i++) {
    int r = i / 4;
    int c = i % 4;
    int bx = startX + (c * pW);
    int by = CONTROLS_Y + (r * pH);
    bool focused = (focusedControl == 4 + i);
    if (focused) display->display.fillRect(bx, by, pW, pH, COLOR_FG);
    display->display.drawRect(bx, by, pW, pH, COLOR_FG);
    display->u8g2Fonts.setForegroundColor(focused ? COLOR_BG : COLOR_FG);
    display->u8g2Fonts.setBackgroundColor(focused ? COLOR_FG : COLOR_BG);
    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    String pStr = String(i + 1);
    int tw = display->u8g2Fonts.getUTF8Width(pStr.c_str());
    display->u8g2Fonts.setCursor(bx + (pW - tw)/2, by + pH/2 + 5);
    display->u8g2Fonts.print(pStr);
  }
  display->u8g2Fonts.setForegroundColor(COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(COLOR_BG);
}

void RadioScreen::handleInput(UIKey key) {
  if (key == UI_KEY_LEFT) {
    focusedControl = (focusedControl - 1 + 12) % 12;
  } else if (key == UI_KEY_RIGHT) {
    focusedControl = (focusedControl + 1) % 12;
  } else if (key == UI_KEY_ENTER) {
    if (focusedControl == 0) radio->seekDown();
    else if (focusedControl == 1) radio->seekUp();
    else if (focusedControl == 2) {
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
    } else if (focusedControl >= 4 && focusedControl <= 11) {
      loadPreset(focusedControl - 4);
    }
  }
}

void RadioScreen::onLongPress() {
  if (focusedControl >= 4 && focusedControl <= 11) {
    savePreset(focusedControl - 4);
  } else {
    if (uiManager) uiManager->switchScreen(SCREEN_MENU);
  }
}

void RadioScreen::savePreset(int index) {
  uint16_t freq = radio->getFrequency();
  config->config.radio_presets[index] = freq;
  config->save();
}

void RadioScreen::loadPreset(int index) {
  uint16_t freq = config->config.radio_presets[index];
  if(freq > 0) radio->setFrequency(freq);
}


