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
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);
      statusBar->draw(display, true); // Top 24px

      // Vertical Split Line
      display->display.drawLine(260, 24, 260, 300, GxEPD_BLACK);

      // --- LEFT PANEL (x: 0, w: 260) ---

      // 1. Top Bar (h: 40px, y: 24-64)
      display->display.fillRect(
          0, 24, 260, 40,
          0xF79E); // Light gray if supported, else WHITE. GxEPD_WHITE is fine.
      display->display.drawLine(0, 64, 260, 64, GxEPD_BLACK);

      display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      display->u8g2Fonts.setCursor(10, 52);
      display->u8g2Fonts.print(weather->data.city);

      // Date Badge Styling: Black background, rounded corners, padding: 2px 6px
      display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      int tw = display->u8g2Fonts.getUTF8Width(weather->data.obs_time.c_str());
      int bw = tw + 12; // 6px padding left + 6px right
      int bh = 20;      // 16px font + 2px top + 2px bottom padding
      int bx = 255 - bw;
      int by = 34; // Keep roughly same y position

      display->display.fillRoundRect(bx, by, bw, bh, 4, GxEPD_BLACK);

      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
      // Center text in badge: bx + 6 (padding), by + 16 (approx baseline)
      display->u8g2Fonts.drawUTF8(bx + 6, by + 16,
                                  weather->data.obs_time.c_str());
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

      // 2. Hero Section (y: 64 - 230)
      // Icon
      display->u8g2Fonts.setFont(
          u8g2_font_qweather_icon_16); // Need a bigger one? 16 is small.
      // Wait, u8g2_font_qweather_icon_16 is likely a 16pt font.
      // If I want 80x80, I might need a bigger font or draw bitmap.
      // Let's see if there's a larger font.
      display->u8g2Fonts.drawUTF8(30, 120, weather->data.icon_str);

      // 天气文本
      display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
      display->u8g2Fonts.setCursor(30, 150);
      display->u8g2Fonts.print(weather->data.weather);

      display->u8g2Fonts.setFont(u8g2_font_logisoso62_tn);
      display->u8g2Fonts.setCursor(110, 165);
      display->u8g2Fonts.print(weather->data.temp);

      // Fixed degree symbol for large temp - custom circle for larger size
      int tWidth =
          display->u8g2Fonts.getUTF8Width(String(weather->data.temp).c_str());
      int degreeX = 110 + tWidth + 12;
      int degreeY = 115;
      // display->display.drawCircle(degreeX, degreeY, 6, GxEPD_BLACK);
      display->display.drawCircle(degreeX, degreeY, 4, GxEPD_BLACK); // Thicker

      // Warning Box
      if (weather->data.warning_text.length() > 0) {
        display->display.fillRect(10, 185, 240, 40, GxEPD_BLACK);
        display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
        display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
        display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
        display->u8g2Fonts.setCursor(15, 200);
        display->u8g2Fonts.print("预警: ");
        display->u8g2Fonts.print(weather->data.warning_title);
        display->u8g2Fonts.setCursor(15, 218);
        String shortWarning =
            weather->data.warning_text.substring(0, 20) + "...";
        display->u8g2Fonts.print(shortWarning);
        display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
      }

      // 3. Hourly Strip (y: 230 - 300)
      display->display.drawLine(0, 230, 260, 230, GxEPD_BLACK);
      int itemW = 260 / 6; // Show 6 items
      display->u8g2Fonts.setFont(u8g2_font_6x10_tf);
      for (int i = 0; i < 6 && i < weather->data.hourly.size(); i++) {
        int x = i * itemW;
        display->u8g2Fonts.setCursor(x + 5, 245);
        display->u8g2Fonts.print(weather->data.hourly[i].time);

        display->u8g2Fonts.setFont(u8g2_font_qweather_icon_16);
        display->u8g2Fonts.drawUTF8(x + 10, 270,
                                    weather->data.hourly[i].icon_str);

        display->u8g2Fonts.setFont(u8g2_font_helvR08_tf);
        display->u8g2Fonts.setCursor(x + 10, 290);
        display->u8g2Fonts.print(weather->data.hourly[i].temp);
        display->u8g2Fonts.print("°");

        if (i > 0)
          display->display.drawLine(x, 230, x, 300, GxEPD_BLACK);
      }

      // --- RIGHT PANEL (x: 260, w: 140) ---
      // Header
      display->display.drawLine(260, 64, 400, 64, GxEPD_BLACK);
      display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      display->u8g2Fonts.setCursor(280, 50);
      display->u8g2Fonts.print("7-DAY FORECAST");

      // Daily List
      int rowH = (300 - 64) / 7;
      display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
      for (int i = 0; i < 7 && i < weather->data.daily.size(); i++) {
        int y = 64 + i * rowH;
        display->u8g2Fonts.setCursor(265, y + 22);
        display->u8g2Fonts.print(weather->data.daily[i].day);

        display->u8g2Fonts.setFont(u8g2_font_qweather_icon_16);
        display->u8g2Fonts.drawUTF8(305, y + 25,
                                    weather->data.daily[i].icon_str);

        display->u8g2Fonts.setFont(u8g2_font_helvR08_tf);
        display->u8g2Fonts.setCursor(340, y + 22);
        display->u8g2Fonts.print(weather->data.daily[i].temp_min);
        // display->u8g2Fonts.drawGlyph(display->u8g2Fonts.getCursorX(),
        //                              display->u8g2Fonts.getCursorY(), 176);
        display->u8g2Fonts.print("° / ");
        display->u8g2Fonts.print(weather->data.daily[i].temp_max);
        display->u8g2Fonts.print("°");
        // display->u8g2Fonts.drawGlyph(display->u8g2Fonts.getCursorX(),
        //                              display->u8g2Fonts.getCursorY(), 176);

        if (i < 6)
          display->display.drawLine(260, y + rowH, 400, y + rowH, GxEPD_BLACK);
      }

      BusManager::getInstance().requestDisplay();
    } while (display->display.nextPage());
    display->powerOff();
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
    return false;
  }

private:
  WeatherManager *weather;
  StatusBar *statusBar;
};
