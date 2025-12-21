// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Controller: IL3820 : http://www.e-paper-display.com/download_detail/downloadsId=540.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_420_SSD1619A.h"
#include <esp_debug_helpers.h>

GxEPD2_420_SSD1619A::GxEPD2_420_SSD1619A(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, HIGH, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
  _grayScaleLevel = 0; // default B/W
}

void GxEPD2_420_SSD1619A::clearScreen(uint8_t value)
{
  _initial_write = false; // initial full screen buffer clean done
  if (_initial_refresh)
  {
    _Init_Full();
    _setPartialRamArea(0, 0, WIDTH, HEIGHT);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
    {
      _writeData(value);
    }
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
  else
  {
    if (!_using_partial_mode) _Init_Part();
    _setPartialRamArea(0, 0, WIDTH, HEIGHT);
    _writeCommand(0x24);
    for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
    {
      _writeData(value);
    }
    _Update_Part();
  }
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  _writeCommand(0x24);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(value);
  }
  _Update_Part();
}

void GxEPD2_420_SSD1619A::writeScreenBuffer(uint8_t value)
{
  _initial_write = false; // initial full screen buffer clean done
  // this controller has no command to write "old data"
  if (_initial_refresh) clearScreen(value);
  else _writeScreenBuffer(value);
}

void GxEPD2_420_SSD1619A::_writeScreenBuffer(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  _writeCommand(0x24);
  _startTransfer();
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _transfer(value);
  }
  _endTransfer();
}

void GxEPD2_420_SSD1619A::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  int16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
  x -= x % 8; // byte boundary
  w = wb * 8; // byte boundary
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(0x24);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb, h of bitmap for index!
      int16_t idx = mirror_y ? j + dx / 8 + ((h - 1 - (i + dy))) * wb : j + dx / 8 + (i + dy) * wb;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_420_SSD1619A::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  int16_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
  x_part -= x_part % 8; // byte boundary
  w = w_bitmap - x_part < w ? w_bitmap - x_part : w; // limit
  h = h_bitmap - y_part < h ? h_bitmap - y_part : h; // limit
  x -= x % 8; // byte boundary
  w = 8 * ((w + 7) / 8); // byte boundary, bitmaps are padded
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(0x24);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      int16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_420_SSD1619A::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_420_SSD1619A::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_420_SSD1619A::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_420_SSD1619A::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                               int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                               int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _Init_Full();
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
}

void GxEPD2_420_SSD1619A::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (_initial_refresh) return refresh(false); // initial update needs be full update
  // intersection with screen
  int16_t w1 = x < 0 ? w + x : w; // reduce
  int16_t h1 = y < 0 ? h + y : h; // reduce
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  w1 = x1 + w1 < int16_t(WIDTH) ? w1 : int16_t(WIDTH) - x1; // limit
  h1 = y1 + h1 < int16_t(HEIGHT) ? h1 : int16_t(HEIGHT) - y1; // limit
  if ((w1 <= 0) || (h1 <= 0)) return;
  // make x1, w1 multiple of 8
  w1 += x1 % 8;
  if (w1 % 8 > 0) w1 += 8 - w1 % 8;
  x1 -= x1 % 8;
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(x1, y1, w1, h1);
  _Update_Part();
}

void GxEPD2_420_SSD1619A::powerOff(void)
{
  _PowerOff();
}

void GxEPD2_420_SSD1619A::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x10); // deep sleep mode
    _writeData(0x1);     // enter deep sleep
    _hibernating = true;
  }
}

void GxEPD2_420_SSD1619A::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  _writeCommand(0x11); // set ram entry mode
  _writeData(0x03);    // x increase, y increase : normal mode
  _writeCommand(0x44);
  _writeData(x / 8);
  _writeData((x + w - 1) / 8);
  _writeCommand(0x45);
  _writeData(y % 256);
  _writeData(y / 256);
  _writeData((y + h - 1) % 256);
  _writeData((y + h - 1) / 256);
  _writeCommand(0x4e);
  _writeData(x / 8);
  _writeCommand(0x4f);
  _writeData(y % 256);
  _writeData(y / 256);
}

void GxEPD2_420_SSD1619A::_PowerOn()
{
  Serial.println("GxEPD2_420_SSD1619A::_PowerOn");
  if (!_power_is_on)
  {
    _writeCommand(0x22);
    _writeData(0xc0);
    _writeCommand(0x20);
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_420_SSD1619A::_PowerOff()
{
  Serial.println("GxEPD2_420_SSD1619A::_PowerOff");
  _writeCommand(0x22);
  _writeData(0xc3);
  _writeCommand(0x20);
  _waitWhileBusy("_PowerOff", power_off_time);
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_420_SSD1619A::_InitDisplay()
{
  // if (_hibernating) _reset();
  _reset();
  _writeCommand(0x74);
  _writeData(0x54);
  _writeCommand(0x7E);
  _writeData(0x3B);

  _writeCommand(0x01);
  _writeData(0x2B);
  _writeData(0x01);
  _writeData(0x00);

  _writeCommand(0x0C);
  _writeData(0x8B);
  _writeData(0x9C);
  _writeData(0xD6);
  _writeData(0x0F);

  _writeCommand(0x3A);
  _writeData(0x21);
  _writeCommand(0x3B);
  _writeData(0x06);
  _writeCommand(0x3C);
  _writeData(0x03);

  _writeCommand(0x11);
  _writeData(0x01);

  _writeCommand(0x2C);
  _writeData(0x00);

  _writeCommand(0x37);
  _writeData(0x00);
  _writeData(0x00);
  _writeData(0x00);
  _writeData(0x00);
  _writeData(0x80);

  _writeCommand(0x21);
  _writeData(0x40);
  _writeCommand(0x22);
  _writeData(0xC7);
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
}

const uint8_t GxEPD2_420_SSD1619A::LUTDefault_full[] PROGMEM =
{
  0x32, // command

  0x08,0x00,0x48,0x40,0x00,0x00,0x00,//L0 B low-high-low-high
  0x20,0x00,0x12,0x20,0x00,0x00,0x00,//L1 W low-high-low
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,//L4 VCOM
  0x05,0x20,0x20,0x05,0x00,
  0x0f,0x00,0x00,0x00,0x00,
  0x20,0x40,0x20,0x20,0x00,
  0x20,0x00,0x00,0x00,0x00,
  0x05,0x00,0x00,0x00,0x01,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
};

const uint8_t GxEPD2_420_SSD1619A::LUTDefault_part[] PROGMEM =
{
  0x32, // command
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L0 BB R0 B/W 0
  0x82,0x00,0x00,0x00,0x00,0x00,0x00,  //L1 BW R0 B/W 1
  0x50,0x00,0x00,0x00,0x00,0x00,0x00,  //L2 WB R1 B/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L3 WW R0 W/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L4 VCOM
  //b1w1 b2          w2
  0x08,0x08,0x00,0x08,0x01,
  0x00,0x00,0x00,0x00,0x01,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
};


//4灰度刷新lut
static const uint8_t LUTDefault_part_4gray[] = {
  0x32, // command

  0x05,0x00,0x10,0x10,0x10,0x00,0x01,//L0 BB R0 B/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,   //L1 BW R0 B/W 1
  0x05,0x00,0x05,0x10,0x10,0x00,0x01, //L2 WB R1 B/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L3 WW R0 W/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L4 VCOM

  0x00,0x00,0x00,0x02,0x01,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  };


//16灰度刷新lut
static const uint8_t LUTDefault_part_16gray[] = {
  0x32, // command

  0x05,0x00,0x10,0x10,0x10,0x00,0x01,//L0 BB R0 B/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,   //L1 BW R0 B/W 1
  0x05,0x00,0x05,0x10,0x10,0x00,0x01, //L2 WB R1 B/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L3 WW R0 W/W 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,  //L4 VCOM

  0x00,0x00,0x00,0x01,0x01,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,
  };


void GxEPD2_420_SSD1619A::_Init_Full()
{
  Serial.println("Full Init");
  // esp_backtrace_print(100);
  _InitDisplay();
  _writeCommandDataPGM(LUTDefault_full, sizeof(LUTDefault_full));
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_420_SSD1619A::_Init_Part()
{
  Serial.println("Partial Init");
  _InitDisplay();
  _writeCommand(0x21);
  _writeData(0x00);
  if (_grayScaleLevel == 4)
    _writeCommandDataPGM(LUTDefault_part_4gray, sizeof(LUTDefault_part_4gray));
  else if (_grayScaleLevel == 16)
    _writeCommandDataPGM(LUTDefault_part_16gray, sizeof(LUTDefault_part_16gray));
  else
    _writeCommandDataPGM(LUTDefault_part, sizeof(LUTDefault_part));
  _PowerOn();
  _using_partial_mode = true;
}

void GxEPD2_420_SSD1619A::setGrayscale(uint8_t gray)
{
  // 0=BW, 1=4Gray, 2=16Gray
  _grayScaleLevel = gray;
}

void GxEPD2_420_SSD1619A::_Update_Full()
{
  Serial.println("Full Update");
  // esp_backtrace_print(100);
  _writeCommand(0x22);
  _writeData(0xc4);
  _writeCommand(0x20);
  _waitWhileBusy("_Update_Full", full_refresh_time);
  _writeCommand(0xff);
}

void GxEPD2_420_SSD1619A::_Update_Part()
{
  Serial.println("Partial Update");
  _writeCommand(0x22);
  _writeData(0x04);
  _writeCommand(0x20);
  _waitWhileBusy("_Update_Part", partial_refresh_time);
  _writeCommand(0xff);
}
