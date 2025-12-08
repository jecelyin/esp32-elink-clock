#pragma once

#include "../../drivers/RtcDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConnectionManager.h"
#include "../../managers/WeatherManager.h"
#include "../Screen.h"
#include "../UIManager.h"

class HomeScreen : public Screen {
public:
  HomeScreen(RtcDriver *rtc, WeatherManager *weather, ConnectionManager *conn)
      : rtc(rtc), weather(weather), conn(conn) {}

  void draw(DisplayDriver *display) override {
    Serial.println("Drawing Home Screen");

    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);

      // Status Bar
      drawStatusBar(display);

      DateTime now = rtc->getTime();

      // Time
      display->u8g2Fonts.setFont(u8g2_font_logisoso58_tn);
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
      int w = display->u8g2Fonts.getUTF8Width(timeStr);
      display->u8g2Fonts.setCursor((400 - w) / 2, 100);
      display->u8g2Fonts.print(timeStr);

      // Date
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      char dateStr[32];
      sprintf(dateStr, "%04d-%02d-%02d Week %d", now.year + 2000, now.month,
              now.day, now.week);
      w = display->u8g2Fonts.getUTF8Width(dateStr);
      display->u8g2Fonts.setCursor((400 - w) / 2, 130);
      display->u8g2Fonts.print(dateStr);

      // Weather Icon (Placeholder for now, using text or simple shape)
      // Ideally use u8g2_font_open_iconic_weather_4x_t
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_weather_4x_t);
      display->u8g2Fonts.setCursor(20, 230);
      display->u8g2Fonts.print("@"); // Sun icon usually

      // Weather Text
      display->u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      display->u8g2Fonts.setCursor(60, 210);
      display->u8g2Fonts.print(weather->data.temp);
      display->u8g2Fonts.print("C");

      display->u8g2Fonts.setCursor(60, 230);
      display->u8g2Fonts.print(weather->data.humidity);
      display->u8g2Fonts.print("%");

      // Todo List
      display->u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      display->u8g2Fonts.setCursor(200, 200);
      display->u8g2Fonts.print("TODO:");
      display->u8g2Fonts.setCursor(200, 220);
      display->u8g2Fonts.print("- Buy milk");
      BusManager::getInstance().requestDisplay();

      // display->display.refresh();
    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu
      uiManager->switchScreen(SCREEN_MENU);
    }
  }

private:
  RtcDriver *rtc;
  WeatherManager *weather;
  ConnectionManager *conn;

  void drawStatusBar(DisplayDriver *display) {
    display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_2x_t);
    display->u8g2Fonts.setCursor(370, 20);
    if (conn->isConnected()) {
      display->u8g2Fonts.print("P"); // WiFi Icon
    } else {
      display->u8g2Fonts.print("Q"); // No WiFi
    }
  }
};
