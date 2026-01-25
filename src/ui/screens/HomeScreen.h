#pragma once

#include <Arduino.h>

#include "../../drivers/RtcDriver.h"
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
             StatusBar *statusBar, TodoManager *todoMgr,
             ConnectionManager *conn)
      : rtc(rtc), weather(weather), sensor(sensor), statusBar(statusBar),
        todoMgr(todoMgr), conn(conn) {}

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

      // 4. Status Bar (WiFi status change or Minute change for time)
      bool wifiState = conn->isConnected();
      if (wifiState != lastWifiState || now.minute != lastStatusBarMinute) {
        renderStatusBarPartial(displayDrv);
        lastWifiState = wifiState;
        lastStatusBarMinute = now.minute;
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

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER) {
      if (uiManager)
        uiManager->switchScreen(SCREEN_MENU);
    }
    return false;
  }
  bool onLongPress() override {
    if (uiManager)
      uiManager->switchScreen(SCREEN_MENU);
    return false;
  }

private:
  RtcDriver *rtc;
  WeatherManager *weather;
  SensorDriver *sensor;
  StatusBar *statusBar;
  TodoManager *todoMgr;
  ConnectionManager *conn;

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
  bool lastWifiState = false;
  int lastStatusBarMinute = -1;

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
    lastWifiState = conn->isConnected();
    lastStatusBarMinute = now.minute;
  }

  void renderAll(DisplayDriver *displayDrv) {
    Serial.println("Drawing Home Screen (Full)");
    auto &display = displayDrv->display;
    auto &u8g2 = displayDrv->u8g2Fonts;

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
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderTimePartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Time");
    auto &display = displayDrv->display;
    // Time Area: Left side, top part.
    // Static Lines: y=199/200, x=280.
    // Safe Rect: (0, 50, 280, 145) -> y[50, 194], x[0, 279]
    display.setPartialWindow(0, 50, 280, 145);
    display.firstPage();
    do {
      display.fillRect(0, 50, 280, 145, GxEPD_WHITE);
      drawTimeSection(displayDrv);
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderStatusBarPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Status Bar");
    auto &display = displayDrv->display;
    // Status Bar Area: y=0 to 24, x=0 to 400
    display.setPartialWindow(0, 0, 400, 24);
    display.firstPage();
    do {
      // StatusBar::draw internally handles fillRect(0, 0, 400, 24, GxEPD_BLACK)
      statusBar->draw(displayDrv, false);
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderSensorPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Sensor");
    auto &display = displayDrv->display;
    // Sensor Area:
    // x=288 to 400 (w=112) is 8-pixel aligned and avoids the line at 280
    // y=25 to 81 (h=56) avoids lines at 24 and 82
    display.setPartialWindow(288, 25, 112, 56);
    display.firstPage();
    do {
      display.fillRect(288, 25, 112, 56, GxEPD_WHITE);
      drawSensorSection(displayDrv);
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderWeatherPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Weather");
    auto &display = displayDrv->display;

    // Split into Today and Tomorrow to avoid byte alignment issues clearing
    // lines Today Area: y=83 to 139 (avoids lines at 82 and 140) x=288 to 400
    // (w=112) is 8-pixel aligned and avoids the line at 280
    display.setPartialWindow(288, 83, 112, 56);
    display.firstPage();
    do {
      display.fillRect(288, 83, 112, 56, GxEPD_WHITE);
      drawTodayWeather(displayDrv);
    } while (display.nextPage());
    displayDrv->powerOff();

    // Tomorrow Area: y=141 to 198 (avoids lines at 140 and 199)
    display.setPartialWindow(288, 141, 112, 57);
    display.firstPage();
    do {
      display.fillRect(288, 141, 112, 57, GxEPD_WHITE);
      drawTomorrowWeather(displayDrv);
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderTasksPartial(DisplayDriver *displayDrv) {
    Serial.println("Partial Update: Tasks");
    auto &display = displayDrv->display;
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
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void drawContent(DisplayDriver *displayDrv) {
    statusBar->draw(displayDrv, false);
    drawTimeSection(displayDrv);
    drawSensorSection(displayDrv);
    drawTodayWeather(displayDrv);
    drawTomorrowWeather(displayDrv);
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
    u8g2.setCursor((280 - timeWidth) / 2 - 4, 150);
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
    // Shifted from 285 to 290 (+5)
    display.drawInvertedBitmap(290, iconY, Bitmap_tempSHT30, 16, 16,
                               GxEPD_BLACK);

    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(308, iconY + 14); // 303 -> 308
    u8g2.print(indoorTemp, 1);

    int tWidth = u8g2.getUTF8Width(String(indoorTemp, 1).c_str());
    u8g2.drawGlyph(308 + tWidth + 2, iconY + 12, 176); // Degree

    int humX = 345; // 340 -> 345
    display.drawInvertedBitmap(humX, iconY, Bitmap_humiditySHT30, 16, 16,
                               GxEPD_BLACK);
    u8g2.setCursor(humX + 18, iconY + 14);
    u8g2.print((int)indoorHum);
    u8g2.print("%");
  }

  void drawTodayWeather(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
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
  }

  void drawTomorrowWeather(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
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
