#pragma once

#include "config.h"
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// 关键逻辑：当前硬件已经确认是 SSD1619A，显示入口固定到对应驱动，
// 避免旧 Z96/SSD1619 适配在编译期被误选导致屏幕完全无响应。
#define USE_GxEPD2_420_SSD1619A
#include "GxEPD2_420_SSD1619A.h"
#define EPD2_DRV GxEPD2_420_SSD1619A

// Select the display class. 4.2" 400x300 b/w
// GxEPD2_420 display(GxEPD2::GDEW042T2, EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
// We will use the display class in the cpp file to avoid multiple definitions
// if not careful, but usually it's fine to have the object extern.

class DisplayDriver {
public:
  DisplayDriver();
  void init();
  void clear();
  void update();
  void showMessage(const char *msg);
  void showStatus(const char *msg, int line);
  void powerOff();

  // Expose the display object for drawing
  GxEPD2_BW<EPD2_DRV, EPD2_DRV::HEIGHT> display;

  U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

private:
};
