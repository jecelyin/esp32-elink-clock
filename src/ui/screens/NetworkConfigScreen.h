#pragma once

#include "../../drivers/RtcDriver.h"
#include "../../managers/BusManager.h"
#include "../../managers/ConnectionManager.h"
#include "../Screen.h"
#include "../UIManager.h"
#include "../components/StatusBar.h"
#include <WiFi.h>

/**
 * NetworkConfigScreen
 * Reference: React design with partial refresh for time.
 * Note: Status bar area (y < 24) is handled by StatusBar component.
 */
class NetworkConfigScreen : public Screen {
public:
  NetworkConfigScreen(RtcDriver *rtc, ConnectionManager *conn,
                      StatusBar *statusBar)
      : rtc(rtc), conn(conn), statusBar(statusBar) {}

  void init() override {
    fullRefreshNeeded = true;
    lastMinute = -1;
  }

  void enter() override {
    fullRefreshNeeded = true;
    lastMinute = -1;
  }

  void draw(DisplayDriver *displayDrv) override {
    Serial.println("NetworkConfigScreen draw called");
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

    if (fullRefreshNeeded) {
      draw(displayDrv);
      return;
    }

    // Update time every 5 seconds if minute changed
    if (nowMs - lastTimeCheck >= 5000) {
      lastTimeCheck = nowMs;
      DateTime now = rtc->getTime();
      if (now.minute != lastMinute) {
        renderTimePartial(displayDrv);
        lastMinute = now.minute;
      }
    }
  }

  bool handleInput(UIKey key) override {
    if (key == UI_KEY_ENTER || key == UI_KEY_LEFT) {
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
  ConnectionManager *conn;
  StatusBar *statusBar;

  bool fullRefreshNeeded = true;
  int lastMinute = -1;
  uint32_t lastTimeCheck = 0;

  void updateState() {
    DateTime now = rtc->getTime();
    lastMinute = now.minute;
    lastTimeCheck = millis();
  }

  void renderAll(DisplayDriver *displayDrv) {
    auto &display = displayDrv->display;
    BusManager::getInstance().requestDisplay();
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      // Status bar handles itself at y=0-24
      drawContent(displayDrv);
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void renderTimePartial(DisplayDriver *displayDrv) {
    auto &display = displayDrv->display;
    BusManager::getInstance().requestDisplay();
    // Time area in status bar (usually x=150 to 250 range for center time)
    // Or just partial refresh the whole status bar height (24px)
    display.setPartialWindow(0, 0, 400, 24);
    display.firstPage();
    do {
      // StatusBar draws its own black background
      statusBar->draw(displayDrv, true); // true to show time in status bar
      BusManager::getInstance().requestDisplay();
    } while (display.nextPage());
    displayDrv->powerOff();
  }

  void drawContent(DisplayDriver *displayDrv) {
    auto &u8g2 = displayDrv->u8g2Fonts;
    auto &display = displayDrv->display;

    // Call status bar first (y=0..23)
    statusBar->draw(displayDrv, true);

    // Main Card Box (mimicking the React UI), starting below status bar
    int cardX = 10;
    int cardY = 35;
    int cardW = 380;
    int cardH = 170;
    display.drawRect(cardX, cardY, cardW, cardH, GxEPD_BLACK);
    display.drawRect(cardX + 1, cardY + 1, cardW - 2, cardH - 2, GxEPD_BLACK);

    // Mock QR Code Area
    int qrSize = 130;
    int qrX = cardX + 15;
    int qrY = cardY + 15;
    drawMockQRCode(displayDrv, qrX, qrY, qrSize);

    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(qrX + 15, qrY + qrSize + 15);
    u8g2.print("SCAN TO CONNECT");

    // Hotspot Info
    int infoX = qrX + qrSize + 25;
    int infoY = cardY + 25;

    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setForegroundColor(GxEPD_BLACK);

    // SSID
    u8g2.setCursor(infoX, infoY);
    u8g2.print("AP SSID");
    u8g2.setFont(u8g2_font_helvB14_tr);
    u8g2.setCursor(infoX, infoY + 25);
    u8g2.print("ESP32-Clock");

    // Password
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(infoX, infoY + 60);
    u8g2.print("PASSWORD");
    // Background for password to mimic React UI
    int pwW = 80;
    int pwH = 20;
    display.fillRect(infoX, infoY + 70, pwW, pwH, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setCursor(infoX + 5, infoY + 85);
    u8g2.print("No Pass");
    u8g2.setForegroundColor(GxEPD_BLACK);

    // Gateway IP
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(infoX, infoY + 115);
    u8g2.print("GATEWAY IP");
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(infoX, infoY + 135);
    u8g2.print(WiFi.softAPIP().toString().c_str());

    // Uplink WiFi Section (Footer part)
    int footerY = 225;
    display.drawLine(10, footerY, 390, footerY, GxEPD_BLACK);
    display.drawLine(10, footerY + 1, 390, footerY + 1, GxEPD_BLACK);

    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.setCursor(15, footerY + 20);
    u8g2.print("UPLINK WIFI");

    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(15, footerY + 42);
    if (WiFi.status() == WL_CONNECTED) {
      u8g2.print(WiFi.SSID().c_str());
      u8g2.print(" (Active)");
    } else {
      u8g2.print("Not Connected");
    }

    // Bottom Logo
    u8g2.setFont(u8g2_font_helvB08_tr);
    int logoW = u8g2.getUTF8Width("PAPER-LIKE DISPLAY");
    u8g2.setCursor((400 - logoW) / 2, 292);
    u8g2.print("PAPER-LIKE DISPLAY");
  }

  void drawMockQRCode(DisplayDriver *displayDrv, int x, int y, int size) {
    auto &display = displayDrv->display;
    display.drawRect(x, y, size, size, GxEPD_BLACK);

    // Position markers (top-left, top-right, bottom-left)
    int boxSize = 35;
    int offset = 8;

    auto drawMarker = [&](int mx, int my) {
      display.fillRect(mx + offset, my + offset, boxSize, boxSize, GxEPD_BLACK);
      display.fillRect(mx + offset + 5, my + offset + 5, boxSize - 10,
                       boxSize - 10, GxEPD_WHITE);
      display.fillRect(mx + offset + 10, my + offset + 10, boxSize - 20,
                       boxSize - 20, GxEPD_BLACK);
    };

    drawMarker(x, y);                               // TL
    drawMarker(x + size - boxSize - offset * 2, y); // TR
    drawMarker(x, y + size - boxSize - offset * 2); // BL

    // Simulated data bits
    for (int i = 0; i < 8; i++) {
      display.fillRect(x + 50 + (i * 12), y + 55 + (i % 3) * 10, 8, 8,
                       GxEPD_BLACK);
      display.fillRect(x + 70 + (i % 4) * 15, y + 85 + (i / 4) * 12, 6, 6,
                       GxEPD_BLACK);
    }
    display.fillRect(x + size / 2, y + size / 2, 20, 20, GxEPD_BLACK);
  }
};
