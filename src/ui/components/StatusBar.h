#pragma once

#include "../../drivers/DisplayDriver.h"
#include "../../drivers/RtcDriver.h"
#include "../../managers/ConnectionManager.h"

class StatusBar {
public:
  StatusBar(ConnectionManager *conn, RtcDriver *rtc) : conn(conn), rtc(rtc) {}

  void draw(DisplayDriver *display) {
    auto u8g2 = display->u8g2Fonts;
    // ==========================================
    // 区域 1: 顶部状态栏 (反色: 黑底白字)
    // ==========================================
    display->display.fillRect(0, 0, 400, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);

    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t); // 图标字体

    // Wifi Icon & Text
    u8g2.drawGlyph(
        10, 17,
        80); // Wifi icon code 'P' in open_iconic usually, adjust per font map
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(24, 17);
    u8g2.print("WIFI");

    // Sync Icon
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(80, 17, 65); // Refresh/Sync icon
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(94, 17);
    u8g2.print("SYNC");

    // Battery (Right aligned)
    u8g2.setCursor(370, 17);
    u8g2.print("82%");
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(355, 17, 73); // Battery icon

    // 恢复默认颜色
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  }

private:
  ConnectionManager *conn;
  RtcDriver *rtc;
};
