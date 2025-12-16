#pragma once


#include "config.h"
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
// #define USE_GxEPD2_420_Z96
#define USE_GxEPD2_420_SSD1619A

#ifdef USE_GxEPD2_420_Z96
#include "GxEPD2_420_Z96.h"
#define EPD2_DRV GxEPD2_420_Z96
#elifdef USE_GxEPD2_420_SSD1619A
#include "GxEPD2_420_SSD1619A.h"
#define EPD2_DRV GxEPD2_420_SSD1619A
#else
#include "GxEPD2_420_SSD1619.h"
#define EPD2_DRV GxEPD2_420_SSD1619
#endif

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

  // Expose the display object for drawing
  GxEPD2_BW<EPD2_DRV, EPD2_DRV::HEIGHT> display;

  U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

private:
};
