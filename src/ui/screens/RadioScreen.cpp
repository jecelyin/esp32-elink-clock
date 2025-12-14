#include "RadioScreen.h"

RadioScreen::RadioScreen(RadioDriver *radio, StatusBar *statusBar, ConfigManager *config)
    : radio(radio), statusBar(statusBar), config(config) {
  focusedControl = 0;
  showVolumePopup = false;
  tempVolume = 10;
}

void RadioScreen::init() {
    // Ensure volume is synced with config on start? 
    // RadioDriver might already be init, but we can set volume again to be sure if needed.
    // tempVolume = config->config.volume; 
}

void RadioScreen::draw(DisplayDriver *display) {
  display->display.setFullWindow();
  display->display.firstPage();
  do {
    display->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(display, true);

    // Header
    display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    const char *title = "FM RADIO";
    int titleWidth = display->u8g2Fonts.getUTF8Width(title);
    display->u8g2Fonts.setCursor((400 - titleWidth) / 2, 50); // Centered
    display->u8g2Fonts.print(title);

    // Current Frequency Large Display
    uint16_t freq = radio->getFrequency();
    char freqStr[10];
    sprintf(freqStr, "%.1f", freq / 100.0);
    
    display->u8g2Fonts.setFont(u8g2_font_logisoso42_tn);
    int freqWidth = display->u8g2Fonts.getUTF8Width(freqStr);
    display->u8g2Fonts.setCursor((400 - freqWidth) / 2 - 20, 110);
    display->u8g2Fonts.print(freqStr);

    display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    display->u8g2Fonts.print(" MHz");

    drawScale(display);
    drawControls(display);

    if (showVolumePopup) {
      drawVolumePopup(display);
    }

    BusManager::getInstance().requestDisplay();
  } while (display->display.nextPage());
}

void RadioScreen::drawScale(DisplayDriver *display) {
  // Simple scale simulation
  // Center is current freq. Range +/- 2MHz?
  int centerX = 200;
  int startY = 130;
  int scaleWidth = 300;
  
  display->display.drawFastHLine(centerX - scaleWidth/2, startY, scaleWidth, GxEPD_BLACK);
  
  // Draw ticks - this is a static representation for now as a real moving scale needs more math
  // Let's just draw a static scale with a pointer for now as requested "scale below title"
  // Actually, user said: "frequency scale dial and indicator below title"
  // Let's draw a dial arc or linear scale. Linear is easier for e-ink.
  
  // Let's make it a linear scale representing 87-108 MHz
  int xStart = 50;
  int xEnd = 350;
  int yLine = 140;
  
  display->display.drawFastHLine(xStart, yLine, xEnd - xStart, GxEPD_BLACK);
  
  // Ticks every 5MHz? 88, 93, 98, 103, 108
  // 87.5 to 108.
  // Range = 20.5 MHz. Width = 300px. ~14.6 px per MHz.
  
  uint16_t currentFreq = radio->getFrequency(); // e.g. 10110
  float freqVal = currentFreq / 100.0;

  for (int f = 88; f <= 108; f += 2) {
      float pos = (f - 87.0) / (108.0 - 87.0);
      int x = xStart + pos * (xEnd - xStart);
      display->display.drawFastVLine(x, yLine, 10, GxEPD_BLACK);
      
      display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312); // Tiny font
      // display->u8g2Fonts.setCursor(x - 5, yLine + 20);
      // display->u8g2Fonts.print(f);
  }

  // Indicator
  float currentPos = (freqVal - 87.0) / (108.0 - 87.0);
  if(currentPos < 0) currentPos = 0;
  if(currentPos > 1) currentPos = 1;
  
  int indicatorX = xStart + currentPos * (xEnd - xStart);
  display->display.fillTriangle(indicatorX, yLine - 2, indicatorX - 5, yLine - 8, indicatorX + 5, yLine - 8, GxEPD_BLACK);
}

void RadioScreen::drawControls(DisplayDriver *display) {
    int startY = 180;
    
    // Seek Down | Seek Up | Volume
    // 0: Seek-
    // 1: Seek+
    // 2: Volume
    
    display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    
    // Seek Down
    display->display.drawRect(30, startY, 80, 30, GxEPD_BLACK);
    if(focusedControl == 0) display->display.fillRect(30, startY, 80, 30, GxEPD_BLACK);
    
    display->u8g2Fonts.setForegroundColor(focusedControl == 0 ? GxEPD_WHITE : GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(focusedControl == 0 ? GxEPD_BLACK : GxEPD_WHITE);
    display->u8g2Fonts.setCursor(45, startY + 20);
    display->u8g2Fonts.print("<<");
    
    // Seek Up
    display->display.drawRect(120, startY, 80, 30, GxEPD_BLACK);
    if(focusedControl == 1) display->display.fillRect(120, startY, 80, 30, GxEPD_BLACK);
    
    display->u8g2Fonts.setForegroundColor(focusedControl == 1 ? GxEPD_WHITE : GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(focusedControl == 1 ? GxEPD_BLACK : GxEPD_WHITE);
    display->u8g2Fonts.setCursor(135, startY + 20);
    display->u8g2Fonts.print(">>");

    // Volume
    display->display.drawRect(210, startY, 80, 30, GxEPD_BLACK);
    if(focusedControl == 2) display->display.fillRect(210, startY, 80, 30, GxEPD_BLACK);
    
    display->u8g2Fonts.setForegroundColor(focusedControl == 2 ? GxEPD_WHITE : GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(focusedControl == 2 ? GxEPD_BLACK : GxEPD_WHITE);
    display->u8g2Fonts.setCursor(225, startY + 20);
    display->u8g2Fonts.print("Vol");
    
    // Reset colors
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    // Presets Grid
    int presetStartY = 230;
    int presetW = 85; // slightly wider to fit freq
    int presetH = 25;
    int gapX = 5;
    int gapY = 5;
    
    for(int i=0; i<8; i++) {
        int r = i / 4;
        int c = i % 4;
        int x = 25 + c * (presetW + gapX);
        int y = presetStartY + r * (presetH + gapY);
        
        // Control index for presets: 3 + i
        bool focused = (focusedControl == 3 + i);
        
        display->display.drawRect(x, y, presetW, presetH, GxEPD_BLACK);
        if(focused) display->display.fillRect(x, y, presetW, presetH, GxEPD_BLACK);
        
        display->u8g2Fonts.setForegroundColor(focused ? GxEPD_WHITE : GxEPD_BLACK);
        display->u8g2Fonts.setBackgroundColor(focused ? GxEPD_BLACK : GxEPD_WHITE);
        
        display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
        display->u8g2Fonts.setCursor(x + 5, y + 17);
        
        uint16_t pFreq = config->config.radio_presets[i];
        if(pFreq == 0) {
             display->u8g2Fonts.print(String(i+1) + " ----");
        } else {
             display->u8g2Fonts.print(String(i+1) + " " + String(pFreq/100.0, 1));
        }
    }
    
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void RadioScreen::drawVolumePopup(DisplayDriver *display) {
    // Popup Window
    display->display.fillRect(50, 80, 300, 140, GxEPD_WHITE);
    display->display.drawRect(50, 80, 300, 140, GxEPD_BLACK);
    display->display.drawRect(52, 82, 296, 136, GxEPD_BLACK); // Thicker border
    
    display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    display->u8g2Fonts.setCursor(110, 120);
    display->u8g2Fonts.print("Volume: " + String(tempVolume));
    
    // Bar
    int barW = 200;
    int barH = 20;
    int barX = 100;
    int barY = 140;
    
    display->display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
    int fillW = (tempVolume * barW) / 15; // Max vol usually 15 or 30? assumed 15 for RDA5807 usually, let's check driver later.
    // Driver setVolume takes uint8_t.
    // For now assume 0-15 scale.
    if (fillW > barW) fillW = barW;
    display->display.fillRect(barX, barY, fillW, barH, GxEPD_BLACK);

    display->u8g2Fonts.setFont(u8g2_font_helvR10_tf);
    display->u8g2Fonts.setCursor(80, 200);
    display->u8g2Fonts.print("Use Left/Right to adjust, Enter to confirm");
}

void RadioScreen::handleInput(UIKey key) {
    if (showVolumePopup) {
        if (key == UI_KEY_LEFT) {
            if(tempVolume > 0) tempVolume--;
            config->config.volume = tempVolume;
            radio->setVolume(tempVolume);
        } else if (key == UI_KEY_RIGHT) {
            if(tempVolume < 15) tempVolume++;
            config->config.volume = tempVolume;
            radio->setVolume(tempVolume);
        } else if (key == UI_KEY_ENTER) {
            config->save();
            showVolumePopup = false;
        }
    } else {
        if (key == UI_KEY_LEFT) {
            focusedControl--;
            if(focusedControl < 0) focusedControl = 10; // Wrap around
        } else if (key == UI_KEY_RIGHT) {
            focusedControl++;
            if(focusedControl > 10) focusedControl = 0; // Wrap around
        } else if (key == UI_KEY_ENTER) {
            if (focusedControl == 0) { // Seek Down
                radio->seekDown();
            } else if (focusedControl == 1) { // Seek Up
                radio->seekUp();
            } else if (focusedControl == 2) { // Volume
                tempVolume = config->config.volume;
                showVolumePopup = true;
            } else if (focusedControl >= 3 && focusedControl <= 10) { // Presets
                int pIndex = focusedControl - 3;
                loadPreset(pIndex);
            }
        }
    }
}

void RadioScreen::onLongPress() {
    if (focusedControl >= 3 && focusedControl <= 10) {
        int pIndex = focusedControl - 3;
        savePreset(pIndex);
    } else {
        // Fallback for other controls? Return to menu?
        // User said: "In volume confirm... In saved freq button click enter switch... Long press enter save current freq".
        // It implies long press save only works on preset buttons.
        // If long press on other buttons, maybe default to Menu?
        // But UIManager logic is generic. 
        // Let's explicitly go to menu if not on preset.
        if (uiManager) uiManager->switchScreen(SCREEN_MENU);
    }
}

void RadioScreen::savePreset(int index) {
    uint16_t freq = radio->getFrequency();
    config->config.radio_presets[index] = freq;
    config->save();
    // Force redraw to update preset label
    // UIManager calls draw() after handleInput, but onLongPress is separate.
    // We should ensure draw triggers.
    // UIManager::onLongPressEnter calls currentScreenObj->onLongPress()
    // It doesn't explicitly call draw() after onLongPress!
    // We should probably request display or just rely on loop update?
    // UIManager::handleInput calls draw(). UIManager::onLongPressEnter does NOT.
    // So we need to trigger it.
    // But Screen::draw is called by UIManager::update() periodically or we can invoke it?
    // UIManager.cpp:73 calls draw() after handleInput.
    // We should probably add a draw() call in UIManager::onLongPressEnter or return true/false to trigger it.
    // Alternatively, just mark update needed.
    // Since we are inside RadioScreen, we can just requestDisplay() but that only sets flag for loop?
    // Usually UIManager handles the main loop.
    // If I look at UIManager::update(), it calls currentScreenObj->update().
    // RadioScreen::update() is default empty.
    // Let's just modify UIManager::onLongPressEnter to also draw.
}

void RadioScreen::loadPreset(int index) {
    uint16_t freq = config->config.radio_presets[index];
    if(freq > 0) {
        radio->setFrequency(freq);
    }
}
