#pragma once

#include "../Screen.h"
#include "../UIManager.h"
#include "../../managers/BusManager.h"

class MenuScreen : public Screen {
public:
  MenuScreen() { menuIndex = 0; }

  void draw(DisplayDriver *display) override {
    Serial.println("Drawing Menu Screen");
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);

      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(150, 40);
      display->u8g2Fonts.print("MENU");

      // Icons: Home, Alarm, Settings, Music, Radio, Weather
      // Mapping to Open Iconic:
      // Home: "H" (embedded)
      // Alarm: "A" (embedded)
      // Settings: "G" (embedded)
      // Music: "M" (embedded)
      // Radio: "R" (embedded) - maybe not available, use text or generic
      // Weather: "@" (weather)

      const char *labels[] = {"Home",  "Alarm", "Settings",
                              "Music", "Radio", "Weather"};
      const char *icons[] = {"H", "A", "G", "M", "R", "@"};
      // Note: Need to switch fonts for different icons if they are in different
      // sets For simplicity, let's assume we use embedded for most and weather
      // for weather

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
          display->display.fillRect(x - 10, y - 40, 100, 90, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
          display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        } else {
          display->display.drawRect(x - 10, y - 40, 100, 90, GxEPD_BLACK);
          display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
          display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
        }

        // Draw Icon
        if (i == 5) { // Weather
          display->u8g2Fonts.setFont(u8g2_font_open_iconic_weather_4x_t);
        } else {
          display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_4x_t);
        }
        display->u8g2Fonts.setCursor(x + 20, y);
        display->u8g2Fonts.print(icons[i]);

        // Draw Label
        display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
        display->u8g2Fonts.setCursor(x + 10, y + 30);
        display->u8g2Fonts.print(labels[i]);

        // Reset Colors
        display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
      }

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 2) { // Left
      menuIndex--;
      if (menuIndex < 0)
        menuIndex = 5;
    } else if (key == 3) { // Right
      menuIndex++;
      if (menuIndex > 5)
        menuIndex = 0;
    } else if (key == 1) { // Select
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
    }
  }

private:
  int menuIndex;
};
