#pragma once

#include "../../drivers/RtcDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConnectionManager.h"
#include "../../managers/WeatherManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"
#include "../../managers/TodoManager.h"

class HomeScreen : public Screen {
public:
  HomeScreen(RtcDriver *rtc, WeatherManager *weather, StatusBar *statusBar, TodoManager *todoMgr)
      : rtc(rtc), weather(weather), statusBar(statusBar), todoMgr(todoMgr) {}

  void draw(DisplayDriver *displayDrv) override {
    Serial.println("Drawing Home Screen");
    auto &display = displayDrv->display;
    auto &u8g2 = displayDrv->u8g2Fonts;
    display.setFullWindow();
    display.firstPage();
    do {
      // Status Bar
      statusBar->draw(displayDrv);

      DateTime now = rtc->getTime();

      u8g2.setForegroundColor(GxEPD_BLACK);
      u8g2.setBackgroundColor(GxEPD_WHITE);

      // ==========================================
      // 区域 2: 时间与天气 (y: 24 -> 199)
      // ==========================================

      // 分割线
      display.drawLine(0, 199, 400, 199, GxEPD_BLACK);
      display.drawLine(0, 200, 400, 200, GxEPD_BLACK); // 双线加粗

      // 左侧：时间 (x: 0-280)
      // -------------------

      // 时间 (Logisoso 字体非常适合做大数字)
      u8g2.setFont(u8g2_font_logisoso92_tn);
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
      int timeWidth = u8g2.getUTF8Width(timeStr);
      u8g2.setCursor((280 - timeWidth) / 2, 150); // 居中
      u8g2.print(timeStr);

      // 日期 (中文)
      char dateStr[18];
      sprintf(dateStr, "%04d年%02d月%02d日", now.year + 2000, now.month,
              now.day);
      int16_t w = u8g2.getUTF8Width(dateStr);
      u8g2.setFont(u8g2_font_wqy16_t_gb2312); // 文泉驿 16pt 中文
      u8g2.setCursor(22, 183);
      u8g2.print(dateStr);

      // 星期 (黑底白字胶囊样式)
      char *weekdayStrs[] = {"日", "一", "二", "三", "四", "五", "六"};
      char weekdayStr[7];
      sprintf(weekdayStr, "周%s", weekdayStrs[now.week]);
      w = u8g2.getUTF8Width(weekdayStr);
      display.fillRect(190, 167, 60, 20, GxEPD_BLACK);
      u8g2.setForegroundColor(GxEPD_WHITE);
      u8g2.setBackgroundColor(GxEPD_BLACK);
      u8g2.setCursor(190 + w / 2, 182);
      u8g2.print(weekdayStr);
      u8g2.setForegroundColor(GxEPD_BLACK);
      u8g2.setBackgroundColor(GxEPD_WHITE);

      // 右侧：天气 (x: 280-400)
      // ---------------------
      display.drawLine(280, 24, 280, 200, GxEPD_BLACK); // 竖向分割线

      // 1. 室内 (y: 24-82)
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setCursor(300, 45);
      u8g2.print("INDOOR");
      u8g2.setFont(u8g2_font_helvB14_tr);
      u8g2.setCursor(310, 70);
      u8g2.print("24");
      u8g2.drawGlyph(335, 70, 176); // Degree symbol

      display.drawLine(280, 82, 400, 82, GxEPD_BLACK);

      // 2. 今日 (y: 82-140)
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setCursor(305, 100);
      u8g2.print("TODAY");

      u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
      u8g2.drawGlyph(290, 130, 69); // Sun icon

      u8g2.setFont(u8g2_font_helvB14_tr);
      u8g2.setCursor(325, 128);
      u8g2.print("18");
      u8g2.drawGlyph(350, 128, 176);

      display.drawLine(280, 140, 400, 140, GxEPD_BLACK);

      // 3. 明日 (y: 140-200)
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setForegroundColor(
          GxEPD_DARKGREY); // 如果屏幕支持灰阶，不支持则显示黑
      u8g2.setCursor(300, 160);
      u8g2.print("TOMORROW");

      u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
      u8g2.drawGlyph(290, 190, 67); // Rain icon

      u8g2.setFont(u8g2_font_helvB12_tr);
      u8g2.setCursor(325, 188);
      u8g2.print("15");
      u8g2.setForegroundColor(GxEPD_BLACK);

      // ==========================================
      // 区域 3: TODO 列表 (y: 200 -> 300)
      // ==========================================

      // ==========================================
      
      std::vector<TodoItem> tasks = todoMgr->getVisibleTodos(now);

      // 列表头
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setCursor(10, 215);
      u8g2.print("UPCOMING TASKS");

      // 数量 Badge
      display.fillRect(370, 205, 20, 14, GxEPD_BLACK);
      u8g2.setForegroundColor(GxEPD_WHITE);
      u8g2.setBackgroundColor(GxEPD_BLACK);
      u8g2.setCursor(377, 216);
      u8g2.print(tasks.size());
      u8g2.setForegroundColor(GxEPD_BLACK);
      u8g2.setBackgroundColor(GxEPD_WHITE);

      display.drawLine(0, 220, 400, 220, GxEPD_BLACK);

      // 渲染列表项
      int startY = 221;
      int rowHeight = 26; // (300 - 221) / 3 approx

      u8g2.setFont(u8g2_font_wqy12_t_gb2312); // 中文小字体

      for (int i = 0; i < 3 && i < tasks.size(); i++) {
        int itemY = startY + (i * rowHeight);

        // 斑马纹
        // (偶数行画浅灰色背景，由于通常E-Ink不支持浅灰，这里模拟为白色，奇数行不变或用黑色抖动)
        // 注意：如果你有灰阶屏幕 (4-gray)，可以使用 GxEPD_LIGHTGREY
        if (i % 2 != 0) {
          // 简单的斑马纹效果在纯黑白屏上可以忽略，或者用点阵模拟
          // display.fillRect(0, itemY, 400, rowHeight, GxEPD_LIGHTGREY);
        }

        // 优先级条 (左侧)
        if (tasks[i].highPriority) {
          display.fillRect(5, itemY + 4, 4, 18, GxEPD_BLACK);
        } else {
          display.drawRect(5, itemY + 4, 4, 18, GxEPD_BLACK);
        }

        // 时间
        u8g2.setCursor(18, itemY + 18);
        u8g2.print(tasks[i].time);

        // 垂直分割线
        display.drawLine(55, itemY + 4, 55, itemY + 22, GxEPD_BLACK);

        // 内容
        u8g2.setCursor(65, itemY + 18);
        u8g2.print(tasks[i].content);

        // 倒计时 (右侧)
        // 简单计算一下右对齐
        int cdWidth = u8g2.getUTF8Width(tasks[i].countdown.c_str());
        u8g2.setCursor(390 - cdWidth, itemY + 18);
        u8g2.print(tasks[i].countdown);

        // 分割线
        if (i < 2)
          display.drawLine(0, itemY + rowHeight, 400, itemY + rowHeight,
                           GxEPD_BLACK);
      }

      BusManager::getInstance().requestDisplay();

      // display->display.refresh();
    } while (display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu
      uiManager->switchScreen(SCREEN_MENU);
    }
  }

private:
  RtcDriver *rtc;
  WeatherManager *weather;
  StatusBar *statusBar;
  TodoManager *todoMgr;
};
