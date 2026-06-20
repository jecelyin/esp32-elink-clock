#pragma once

#include <Arduino.h>

#include "../../drivers/DisplayDriver.h"
#include "../../drivers/RtcDriver.h"
#include "../../drivers/BatteryDriver.h"
#include "../../managers/ConnectionManager.h"

class StatusBar {
public:
  StatusBar(ConnectionManager *conn, RtcDriver *rtc, BatteryDriver *battery)
      : conn(conn), rtc(rtc), battery(battery) {}

  void draw(DisplayDriver *display, bool showTime) {
    auto &epd = display->display;
    auto &u8g2 = display->u8g2Fonts;
    DateTime now = rtc->getTime();
    bool wifiConnected = conn->isConnected();

    syncBatteryInfo();
    prepareCanvas(epd, u8g2);
    drawConnection(u8g2, wifiConnected);

    if (showTime) {
      drawClock(u8g2, now);
    }

    drawBattery(epd, u8g2);
    restoreCanvas(u8g2);
    recordRenderedState(now, showTime, wifiConnected);
  }

  bool needsRefresh(bool showTime) {
    if (!hasRendered) {
      return true;
    }

    if (showTime != lastRenderedShowTime) {
      return true;
    }

    if (conn->isConnected() != lastRenderedWifiState) {
      return true;
    }

    if (showTime && rtc->getSoftwareTime().minute != lastRenderedMinute) {
      return true;
    }

    return shouldRefreshBatteryInfo();
  }

  void refreshPartial(DisplayDriver *display, bool showTime) {
    auto &epd = display->display;
    epd.setPartialWindow(0, 0, 400, 24);
    epd.firstPage();
    do {
      draw(display, showTime);
    } while (epd.nextPage());
    display->powerOff();
  }

private:
  void syncBatteryInfo() {
    if (!shouldRefreshBatteryInfo()) {
      return;
    }

    lastBatteryInfo = battery->readInfo();
    lastBattUpdate = millis();
  }

  bool shouldRefreshBatteryInfo() const {
    return !lastBatteryInfo.dataValid || millis() - lastBattUpdate >= 60000UL;
  }

  void prepareCanvas(GxEPD2_BW<EPD2_DRV, EPD2_DRV::HEIGHT> &epd,
                     U8G2_FOR_ADAFRUIT_GFX &u8g2) {
    epd.fillRect(0, 0, 400, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
  }

  void drawConnection(U8G2_FOR_ADAFRUIT_GFX &u8g2, bool wifiConnected) {
    u8g2.setFont(u8g2_font_open_iconic_embedded_1x_t);
    u8g2.drawGlyph(10, 17, wifiConnected ? 80 : 64);

    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(24, 17);
    u8g2.print(wifiConnected ? "WIFI" : "OFF");
  }

  void drawClock(U8G2_FOR_ADAFRUIT_GFX &u8g2, const DateTime &now) {
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", now.hour, now.minute);

    u8g2.setFont(u8g2_font_helvB10_tf);
    int textWidth = u8g2.getUTF8Width(timeStr);
    u8g2.setCursor((400 - textWidth) / 2, 17);
    u8g2.print(timeStr);
  }

  void drawBattery(GxEPD2_BW<EPD2_DRV, EPD2_DRV::HEIGHT> &epd,
                   U8G2_FOR_ADAFRUIT_GFX &u8g2) {
    int battLevel = lastBatteryInfo.levelPercent;
    int bodyX = 350;
    int bodyY = 6;
    int bodyW = 16;
    int bodyH = 10;

    u8g2.setFont(u8g2_font_helvB08_tr);
    if (lastBatteryInfo.alert) {
      u8g2.setCursor(340, 15);
      u8g2.print("!");
    }

    u8g2.setCursor(370, 15);
    printBatteryPercent(u8g2, battLevel);

    epd.drawRect(bodyX, bodyY, bodyW, bodyH, GxEPD_WHITE);
    epd.fillRect(bodyX + bodyW, bodyY + 3, 2, 4, GxEPD_WHITE);

    if (battLevel > 0) {
      int fillWidth = (bodyW - 4) * battLevel / 100;
      if (fillWidth < 1) {
        fillWidth = 1;
      }
      epd.fillRect(bodyX + 2, bodyY + 2, fillWidth, bodyH - 4, GxEPD_WHITE);
    }
  }

  void printBatteryPercent(U8G2_FOR_ADAFRUIT_GFX &u8g2, int battLevel) {
    if (battLevel < 0) {
      u8g2.print("--%");
      return;
    }

    u8g2.print(battLevel);
    u8g2.print("%");
  }

  void restoreCanvas(U8G2_FOR_ADAFRUIT_GFX &u8g2) {
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
  }

  void recordRenderedState(const DateTime &now, bool showTime,
                           bool wifiConnected) {
    // 关键逻辑：状态栏是否需要局刷，取决于“上次真正画到屏幕上的值”。
    // 这里在每次 draw 完成后记录渲染快照，UIManager 后续只要比较快照和
    // 当前实时值，就能判断是否必须刷新，避免把分钟轮询逻辑复制到每个页面。
    hasRendered = true;
    lastRenderedMinute = now.minute;
    lastRenderedShowTime = showTime;
    lastRenderedWifiState = wifiConnected;
  }

  ConnectionManager *conn;
  RtcDriver *rtc;
  BatteryDriver *battery;
  BatteryInfo lastBatteryInfo;
  uint32_t lastBattUpdate = 0;
  bool hasRendered = false;
  int lastRenderedMinute = -1;
  bool lastRenderedShowTime = true;
  bool lastRenderedWifiState = false;
};
