#pragma once

#include "../Screen.h"
#include "../UIManager.h"
#include "../../managers/BusManager.h"
#include "../components/StatusBar.h"

class MenuScreen : public Screen {
public:
  MenuScreen(StatusBar *statusBar) : statusBar(statusBar) { menuIndex = 0; }

  void draw(DisplayDriver *display) override {
    Serial.println("Drawing Menu Screen");
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);

      statusBar->draw(display, true);

      struct MenuItem {
        const char *label;
        const uint8_t *font;
        uint16_t icon;
      };

      MenuItem items[] = {
          {"Home", u8g2_font_open_iconic_embedded_4x_t, 'D'},
          {"Alarm", u8g2_font_open_iconic_embedded_4x_t, 'A'},
          {"Music", u8g2_font_open_iconic_play_4x_t, 'C'},
          {"Radio", u8g2_font_open_iconic_embedded_4x_t, 'F'},
          {"Weather", u8g2_font_open_iconic_weather_4x_t, '@'},
          {"Settings", u8g2_font_open_iconic_embedded_4x_t, 'B'}};

      int x_start = 40;
      int y_start = 100;
      int x_gap = 120;
      int y_gap = 100;

      for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = x_start + col * x_gap;
        int y = y_start + row * y_gap;

        // Draw Selection Box
        if (i == menuIndex) {
          display->display.fillRect(x - 10, y - 50, 100, 90, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
          display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        } else {
          display->display.drawRect(x - 10, y - 50, 100, 90, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
          display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        }

        // Draw Icon
        // Box Specs: x start: x-10, width: 100.
        // Center X = (x-10) + (100/2) = x + 40.
        int boxCenterX = x + 40;

        // Draw Icon
        display->u8g2Fonts.setFont(items[i].font);
        int iconWidth = u8g2_GetGlyphWidth(&(display->u8g2Fonts.u8g2), items[i].icon);
        display->u8g2Fonts.drawGlyph(boxCenterX - (iconWidth / 2), y, items[i].icon);

        // Draw Label
        display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
        int labelWidth = display->u8g2Fonts.getUTF8Width(items[i].label);
        display->u8g2Fonts.setCursor(boxCenterX - (labelWidth / 2), y + 22);
        display->u8g2Fonts.print(items[i].label);

        // Reset Colors
        display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
      }

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(UIKey key) override {
    bool updateNeeded = false;
    if (key == UI_KEY_LEFT) { // Left (KEY_LEFT)
      menuIndex--;
      if (menuIndex < 0)
        menuIndex = 5;
      updateNeeded = true;
    } else if (key == UI_KEY_RIGHT) { // Right (KEY_RIGHT)
      menuIndex++;
      if (menuIndex > 5)
        menuIndex = 0;
      updateNeeded = true;
    } else if (key == UI_KEY_ENTER) { // Select (KEY_ENTER Short)
      switch (menuIndex) {
      case 0:
        uiManager->switchScreen(SCREEN_HOME);
        break;
      case 1:
        uiManager->switchScreen(SCREEN_ALARM);
        break;
      case 2:
        uiManager->switchScreen(SCREEN_SETTINGS);
        break;
      case 3:
        uiManager->switchScreen(SCREEN_MUSIC);
        break;
      case 4:
        uiManager->switchScreen(SCREEN_RADIO);
        break;
      case 5:
        uiManager->switchScreen(SCREEN_WEATHER);
        break;
      }
      return; // Switch screen handles draw
    }
    
    // Only redraw if selection changed
    if (updateNeeded) {
        // We could implement partial refresh for selection box moving?
        // For simplicity, full refresh or just let UIManager call draw() which does full page loop.
        // UIManager calls draw() after handleInput returns.
    }
  }

private:
  int menuIndex;
  StatusBar *statusBar;
};
