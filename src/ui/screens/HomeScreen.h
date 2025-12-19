#pragma once

#include <Arduino.h>

#include "../../drivers/RtcDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConnectionManager.h"
#include "../../managers/TodoManager.h"
#include "../../managers/WeatherManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"

#include "../../drivers/SensorDriver.h"
#include "../../utils/qweather_fonts.h"

// sht30温度小图标 width: 16, height: 16
const unsigned char Bitmap_tempSHT30[] PROGMEM = {
    0xfc, 0x3f, 0xf9, 0x9f, 0xfb, 0xdf, 0xf8, 0xdf, 0xfb, 0xdf, 0xf8,
    0xdf, 0xfb, 0xdf, 0xf3, 0xcf, 0xf7, 0xef, 0xe7, 0xe7, 0xef, 0xf7,
    0xef, 0xd7, 0xef, 0xb7, 0xe7, 0x67, 0xf3, 0xcf, 0xf8, 0x1f};

// sht30湿度小图标 width: 16, height: 16
const unsigned char Bitmap_humiditySHT30[] PROGMEM = {
    0xfe, 0x7f, 0xfc, 0x3f, 0xf9, 0x9f, 0xf3, 0xcf, 0xf7, 0xef, 0xe7,
    0xe7, 0xef, 0xf7, 0xcf, 0xf3, 0xdf, 0xfb, 0xd1, 0xfb, 0xd0, 0x1b,
    0xd0, 0x0b, 0xc8, 0x13, 0xe4, 0x27, 0xf3, 0xcf, 0xf8, 0x1f};

class HomeScreen : public Screen {
public:
  HomeScreen(RtcDriver *rtc, WeatherManager *weather, SensorDriver *sensor,
             StatusBar *statusBar, TodoManager *todoMgr)
      : rtc(rtc), weather(weather), sensor(sensor), statusBar(statusBar),
        todoMgr(todoMgr) {}

  void init() override {
    fullRefreshNeeded = true;
    lastMinute = -1;
    lastTasksMinute = -1;
    lastTimeCheck = 0;
    lastSensorCheck = 0;
  }

  void enter() override {
    fullRefreshNeeded = true;
    lastMinute = -1;
    lastTasksMinute = -1;
    lastTimeCheck = 0;
    lastSensorCheck = 0;
  }

  void draw(DisplayDriver *displayDrv) override {
    // Full refresh entry point
    Serial.println("HomeScreen draw called");
    fullRefreshNeeded = true;
    renderAll(displayDrv);
    fullRefreshNeeded = false;
    updateState();
  }

  void update() override {
    if (!uiManager)
      return;
    DisplayDriver *displayDrv = uiManager->getDisplayDriver();
    if (!displayDrv)
      return;

    uint32_t nowMs = millis();

    // Ensure full refresh if requested (e.g. at startup)
    if (fullRefreshNeeded) {
      draw(displayDrv);
      return;
    }

    // Check Time, Weather, and Tasks (every 5 seconds)
    if (nowMs - lastTimeCheck >= 5000) {
      lastTimeCheck = nowMs;
      DateTime now = rtc->getTime();

      // 1. Time
      if (now.minute != lastMinute) {
        renderTimePartial(displayDrv);
        lastMinute = now.minute;
      }

      // 2. Weather
      if (weather->data.temp != lastWeatherTemp ||
          weather->data.weather != lastWeatherStr ||
          String(weather->data.icon_str) != lastWeatherIcon) {
        renderWeatherPartial(displayDrv);
        lastWeatherTemp = weather->data.temp;
        lastWeatherStr = weather->data.weather;
        lastWeatherIcon = weather->data.icon_str;
      }

      // 3. Tasks (Check if minute changed OR task list changed)
      std::vector<TodoItem> tasks = todoMgr->getVisibleTodos(now);
      bool tasksNeedUpdate =
          (now.minute != lastTasksMinute) || (tasks.size() != lastTaskCount);

      if (tasksNeedUpdate) {
        renderTasksPartial(displayDrv);
        lastTaskCount = tasks.size();
        lastTasksMinute = now.minute;
      }
    }

    // Check Sensor (every 60 seconds)
    if (nowMs - lastSensorCheck >= 60000) {
      lastSensorCheck = nowMs;
      float temp, hum;
      sensor->readData(temp, hum);
      if (abs(temp - lastTemp) > 0.4 || abs(hum - lastHum) > 1.0) {
        renderSensorPartial(displayDrv);
        lastTemp = temp;
        lastHum = hum;
      }
    }
  }

  void handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) {
      if (uiManager)
        uiManager->switchScreen(SCREEN_MENU);
    }
  }
  void onLongPress() override {
    if (uiManager)
      uiManager->switchScreen(SCREEN_MENU);
  }

private:
  RtcDriver *rtc;
  WeatherManager *weather;
  SensorDriver *sensor;
  StatusBar *statusBar;
  TodoManager *todoMgr;

  // State for change detection
  bool fullRefreshNeeded = true;
  int lastMinute = -1;
  int lastTasksMinute = -1;
  float lastTemp = -999;
  float lastHum = -999;
  uint32_t lastTimeCheck = 0;
  uint32_t lastSensorCheck = 0;
  int lastWeatherTemp = -999;
  String lastWeatherStr = "";
  String lastWeatherIcon = "";
  int lastTaskCount = -1;

  void updateState() {
    DateTime now = rtc->getTime();
    lastMinute = now.minute;
    lastTasksMinute = now.minute;
    lastTimeCheck = millis();
    lastSensorCheck = millis();

    float t, h;
    sensor->readData(t, h);
    lastTemp = t;
    lastHum = h;

    lastWeatherTemp = weather->data.temp;
    lastWeatherStr = weather->data.weather;
    lastWeatherIcon = weather->data.icon_str;

    lastTaskCount = todoMgr->getVisibleTodos(now).size();
  }

  void renderAll(DisplayDriver *displayDrv) {
    Serial.println("Drawing Home Screen (Full)");
    auto &display = displayDrv->display;
    auto &u8g2 = displayDrv->u8g2Fonts;

    BusManager::getInstance().requestDisplay();
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      // Static Lines
      display.drawLine(0, 199, 400, 199, GxEPD_BLACK);
      display.drawLine(0, 200, 400, 200, GxEPD_BLACK);
      display.drawLine(280, 24, 280, 200, GxEPD_BLACK); // Vertical
      display.drawLine(280, 82, 400, 82, GxEPD_BLACK);
      display.drawLine(280, 140, 400, 140, GxEPD_BLACK);
      display.drawLine(0, 220, 400, 220, GxEPD_BLACK);

      drawContent(displayDrv);
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
  }

  void renderTimePartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Time");
    auto &display = displayDrv->display;
    BusManager::getInstance().requestDisplay(); // Request before setup
    // Time Area: Left side, top part.
    // Static Lines: y=199/200, x=280.
    // Safe Rect: (0, 50, 280, 145) -> y[50, 194], x[0, 279]
    display.setPartialWindow(0, 50, 280, 145);
    display.firstPage();
    do {
      display.fillRect(0, 50, 280, 145, GxEPD_WHITE);
      drawTimeSection(displayDrv);
      BusManager::getInstance()
          .requestDisplay(); // Must be called before nextPage
    } while (display.nextPage());
  }

  void renderSensorPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Sensor");
    auto &display = displayDrv->display;
    BusManager::getInstance().requestI2C();
    // Sensor Area:
    // Static vertical line at x=280. Partial must start > 280. 281 safe.
    // Static horizontal line at y=82. Partial must end < 82.
    // y=24, h=57 -> ends 80. Safe.
    display.setPartialWindow(281, 24, 119, 57);
    display.firstPage();
    do {
      display.fillRect(281, 24, 119, 57, GxEPD_WHITE);
      drawSensorSection(displayDrv);
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
  }

  void renderWeatherPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Weather");
    auto &display = displayDrv->display;
    BusManager::getInstance().requestDisplay();
    // Weather Area:
    // Static vertical line x=280 -> start 281.
    // Static horiz line y=82 -> start 83.
    // Static horiz line y=199 -> end 198.
    // y=83, h=115 -> ends 197. Safe.
    display.setPartialWindow(281, 83, 119, 115);
    display.firstPage();
    do {
      display.fillRect(281, 83, 119, 115, GxEPD_WHITE);
      display.drawLine(281, 140, 400, 140,
                       GxEPD_BLACK); // internal line, start 281
      drawWeatherSection(displayDrv);
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
  }

  void renderTasksPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Tasks");
    auto &display = displayDrv->display;
    BusManager::getInstance().requestDisplay();
    // Tasks Area:
    // Static lines y=199, 200.
    // Start y=202. h=98 -> ends 299. Safe.
    display.setPartialWindow(0, 202, 400, 98);
    display.firstPage();
    do {
      display.fillRect(0, 202, 400, 98, GxEPD_WHITE);
      // Header parts
      // Static line at 220 is inside 202-300 range, MUST redraw it.
      display.drawLine(0, 220, 400, 220, GxEPD_BLACK);
      drawTasksSection(displayDrv);
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
  }

  void drawContent(DisplayDriver *displayDrv) {
    statusBar->draw(displayDrv, false);
    drawTimeSection(displayDrv);
    drawSensorSection(displayDrv);
    drawWeatherSection(displayDrv);
    drawTasksSection(displayDrv);
  }

  // --- Section Drawers (No page loop, just drawing commands) ---

  void drawTimeSection(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    auto &display = displayDrv->display;
    DateTime now = rtc->getTime();

    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);

    // Time
    u8g2.setFont(u8g2_font_logisoso92_tn);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
    int timeWidth = u8g2.getUTF8Width(timeStr);
    u8g2.setCursor((280 - timeWidth) / 2, 150);
    u8g2.print(timeStr);

    // Date
    char dateStr[18];
    sprintf(dateStr, "%04d年%02d月%02d日", now.year + 2000, now.month, now.day);
    u8g2.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2.setCursor(22, 183);
    u8g2.print(dateStr);

    // Weekday
    char *weekdayStrs[] = {"日", "一", "二", "三", "四", "五", "六"};
    char weekdayStr[7];
    sprintf(weekdayStr, "周%s", weekdayStrs[now.week]);
    int w = u8g2.getUTF8Width(weekdayStr);
    display.fillRect(190, 167, 60, 20, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setCursor(190 + w / 2, 182);
    u8g2.print(weekdayStr);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  }

  void drawSensorSection(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    auto &display = displayDrv->display;
    float indoorTemp = 0, indoorHum = 0;
    sensor->readData(indoorTemp, indoorHum);

    int iconY = 45;
    display.drawInvertedBitmap(285, iconY, Bitmap_tempSHT30, 16, 16,
                               GxEPD_BLACK);

    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(303, iconY + 14);
    u8g2.print(indoorTemp, 1);

    int tWidth = u8g2.getUTF8Width(String(indoorTemp, 1).c_str());
    u8g2.drawGlyph(303 + tWidth + 2, iconY + 12, 176); // Degree

    int humX = 340;
    display.drawInvertedBitmap(humX, iconY, Bitmap_humiditySHT30, 16, 16,
                               GxEPD_BLACK);
    u8g2.setCursor(humX + 18, iconY + 14);
    u8g2.print((int)indoorHum);
    u8g2.print("%");
  }

  void drawWeatherSection(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    auto &display = displayDrv->display; // needed?

    // Today
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(305, 100);
    u8g2.print("TODAY");

    u8g2.setFont(u8g2_font_qweather_icon_16);
    u8g2.drawUTF8(290, 130, weather->data.icon_str);

    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setCursor(325, 124);
    u8g2.print(weather->data.weather);
    u8g2.print(" ");
    u8g2.print(weather->data.temp);
    u8g2.print("°");

    // Tomorrow
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(300, 160);
    u8g2.print("TOMORROW");

    u8g2.setFont(u8g2_font_qweather_icon_16);
    u8g2.drawUTF8(290, 190, weather->data.forecast_icon_str);

    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setCursor(325, 183);
    u8g2.print(weather->data.forecast_weather);
    u8g2.print(" ");
    u8g2.print(weather->data.forecast_temp_low);
    u8g2.print("°-");
    u8g2.print(weather->data.forecast_temp_high);
    u8g2.print("°");
  }

  void drawTasksSection(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    auto &display = displayDrv->display;
    DateTime now = rtc->getTime();
    std::vector<TodoItem> tasks = todoMgr->getVisibleTodos(now);

    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(10, 215);
    u8g2.print("UPCOMING TASKS");

    display.fillRect(370, 205, 20, 14, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setCursor(377, 216);
    u8g2.print(tasks.size());
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);

    int startY = 221;
    int rowHeight = 26;
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);

    for (int i = 0; i < 3 && i < tasks.size(); i++) {
      int itemY = startY + (i * rowHeight);

      if (tasks[i].highPriority) {
        display.fillRect(5, itemY + 4, 4, 18, GxEPD_BLACK);
      } else {
        display.drawRect(5, itemY + 4, 4, 18, GxEPD_BLACK);
      }

      u8g2.setCursor(18, itemY + 18);
      u8g2.print(tasks[i].time);

      display.drawLine(55, itemY + 4, 55, itemY + 22, GxEPD_BLACK);

      u8g2.setCursor(65, itemY + 18);
      u8g2.print(tasks[i].content);

      int cdWidth = u8g2.getUTF8Width(tasks[i].countdown.c_str());
      u8g2.setCursor(390 - cdWidth, itemY + 18);
      u8g2.print(tasks[i].countdown);

      if (i < 2)
        display.drawLine(0, itemY + rowHeight, 400, itemY + rowHeight,
                         GxEPD_BLACK);
    }
  }
};
