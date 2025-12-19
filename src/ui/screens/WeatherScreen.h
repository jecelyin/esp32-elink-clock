#pragma once

#include "../../managers/BusManager.h"
#include "../../managers/WeatherManager.h"
#include "../../utils/qweather_fonts.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

class WeatherScreen : public Screen {
public:
  WeatherScreen(WeatherManager *weather, StatusBar *statusBar)
      : weather(weather), statusBar(statusBar) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    BusManager::getInstance().lock();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display, true);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(120, 40);
      display->u8g2Fonts.print("WEATHER");

      // Icon
      // Icon
      display->u8g2Fonts.setFont(u8g2_font_qweather_icon_16);
      display->u8g2Fonts.setCursor(20, 60);
      // char iconStr[2] = {weather->data.icon_char, '\0'};
      display->u8g2Fonts.drawUTF8(
          20, 90,
          weather->data
              .icon_str); // Adjusted Y for UTF8 baseline if needed, but
                          // drawUTF8 usually takes x,y. setCursor + print is
                          // also fine but print(String) works for UTF8?
                          // u8g2.print supports UTF8 if enableUTF8Print is on?
      // SAFE: drawUTF8(x, y, str)

      // Details
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      display->u8g2Fonts.setCursor(50, 120);
      display->u8g2Fonts.print("City: ");
      display->u8g2Fonts.print(weather->data.city);

      display->u8g2Fonts.setCursor(50, 150);
      display->u8g2Fonts.print("Temp: ");
      display->u8g2Fonts.print(weather->data.temp);
      display->u8g2Fonts.print(" C");

      display->u8g2Fonts.setCursor(50, 180);
      display->u8g2Fonts.print("Hum: ");
      display->u8g2Fonts.print(weather->data.humidity);
      display->u8g2Fonts.print(" %");

      display->u8g2Fonts.setCursor(50, 210);
      display->u8g2Fonts.print("Tom: "); // Tomorrow
      display->u8g2Fonts.print(weather->data.forecast_weather);
      display->u8g2Fonts.print(" ");
      display->u8g2Fonts.print(weather->data.forecast_temp_low);
      display->u8g2Fonts.print("/");
      display->u8g2Fonts.print(weather->data.forecast_temp_high);
      display->u8g2Fonts.print("C");

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
  }

private:
  WeatherManager *weather;
  StatusBar *statusBar;
};
