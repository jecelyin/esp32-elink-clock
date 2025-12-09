#pragma once

#include "../../managers/WeatherManager.h"
#include "../../managers/BusManager.h"
#include "../components/StatusBar.h"
#include "../Screen.h"
#include "../UIManager.h"

class WeatherScreen : public Screen {
public:
  WeatherScreen(WeatherManager *weather, StatusBar *statusBar) : weather(weather), statusBar(statusBar) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(120, 40);
      display->u8g2Fonts.print("WEATHER");

      // Icon
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_weather_4x_t);
      display->u8g2Fonts.setCursor(20, 60);
      display->u8g2Fonts.print("@");

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
      display->u8g2Fonts.print("Forecast: ");
      display->u8g2Fonts.print(weather->data.forecast_weather);

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
  }

private:
  WeatherManager *weather;
  StatusBar *statusBar;
};
