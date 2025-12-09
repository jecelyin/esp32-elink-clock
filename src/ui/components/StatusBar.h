#pragma once

#include "../../drivers/DisplayDriver.h"
#include "../../managers/ConnectionManager.h"
#include "../../drivers/RtcDriver.h"

class StatusBar {
public:
    StatusBar(ConnectionManager* conn, RtcDriver* rtc) : conn(conn), rtc(rtc) {}

    void draw(DisplayDriver* display) {
        // WiFi Icon
        display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_2x_t);
        display->u8g2Fonts.setCursor(370, 20);
        if (conn->isConnected()) {
            display->u8g2Fonts.print("P"); // WiFi Icon
        } else {
            display->u8g2Fonts.print("Q"); // No WiFi
        }

        // Small Time (useful for screens other than Home)
        // Position it to the left of WiFi icon
        DateTime now = rtc->getTime();
        display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", now.hour, now.minute);
        int w = display->u8g2Fonts.getUTF8Width(timeStr);
        display->u8g2Fonts.setCursor(370 - w - 10, 20); // 10px padding
        display->u8g2Fonts.print(timeStr);
    }

private:
    ConnectionManager* conn;
    RtcDriver* rtc;
};
