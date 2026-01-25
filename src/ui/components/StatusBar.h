#pragma once

#include "../../drivers/DisplayDriver.h"
#include "../../drivers/RtcDriver.h"
#include "../../managers/ConnectionManager.h"

#include "../../drivers/SensorDriver.h"

class StatusBar {
public:
  StatusBar(ConnectionManager *conn, RtcDriver *rtc, SensorDriver *sensor)
      : conn(conn), rtc(rtc), sensor(sensor) {}

  void draw(DisplayDriver *display, bool showTime) {
    auto u8g2 = display->u8g2Fonts;
    // ==========================================
    // 区域 1: 顶部状态栏 (反色: 黑底白字)
    // ==========================================
    display->display.fillRect(0, 0, 400, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);

    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t); // 图标字体

    // Wifi Icon & Text
    if (conn->isConnected()) {
      u8g2.drawGlyph(10, 17, 80); // Wifi icon
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setCursor(24, 17);
      u8g2.print("WIFI");
    } else {
      u8g2.drawGlyph(10, 17, 64); // "Prohibited" icon indicating disconnected
      u8g2.setFont(u8g2_font_helvB08_tr);
      u8g2.setCursor(24, 17);
      u8g2.print("OFF");
    }

    // Center Time (if requested)
    if (showTime) {
      DateTime now = rtc->getTime();
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
      u8g2.setFont(u8g2_font_helvB10_tf);
      int w = u8g2.getUTF8Width(timeStr);
      u8g2.setCursor((400 - w) / 2, 17);
      u8g2.print(timeStr);
    }

    // Battery (Right aligned)
    // 每分钟更新一次电池电量以节省功耗
    if (millis() - lastBattUpdate >= 60000 || lastBattLevel == -1) {
      lastBattLevel = sensor->getBatteryLevel();
      lastBattUpdate = millis();
    }

    int battLevel = lastBattLevel;
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(370, 15);
    u8g2.print(battLevel);
    u8g2.print("%");

    // Draw Custom Battery Icon
    // x=350, y=6, w=16, h=10
    int bx = 350;
    int by = 6;
    int bw = 16;
    int bh = 10;

    // Outline
    display->display.drawRect(bx, by, bw, bh, GxEPD_WHITE);
    // Nub
    display->display.fillRect(bx + bw, by + 3, 2, 4, GxEPD_WHITE);
    // Fill
    if (battLevel > 0) {
      int fillW = (bw - 4) * battLevel / 100;
      if (fillW < 1)
        fillW = 1;
      // Use fillRect for white bar
      display->display.fillRect(bx + 2, by + 2, fillW, bh - 4, GxEPD_WHITE);
    }

    // 恢复默认颜色
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  }

private:
  ConnectionManager *conn;
  RtcDriver *rtc;
  SensorDriver *sensor;
  int lastBattLevel = -1;
  uint32_t lastBattUpdate = 0;
};
