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

namespace {
constexpr uint32_t FULL_BUSY_TIMEOUT_MS = 16000;
constexpr uint32_t PARTIAL_BUSY_TIMEOUT_MS = 8000;
constexpr uint32_t POWER_BUSY_TIMEOUT_MS = 4000;
constexpr uint32_t BUSY_POLL_INTERVAL_MS = 2;

uint8_t readBitmapByte(const uint8_t bitmap[], int16_t idx, bool pgm)
{
  if (pgm) {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
    return pgm_read_byte(&bitmap[idx]);
#else
    return bitmap[idx];
#endif
  }
  return bitmap[idx];
}

uint8_t reverseBits(uint8_t value)
{
  value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  return ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
}
} // namespace

GxEPD2_420_SSD1619A::GxEPD2_420_SSD1619A(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, HIGH, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
  _grayScaleLevel = 0; // default B/W
}

void GxEPD2_420_SSD1619A::clearScreen(uint8_t value)
{
  // 关键逻辑：参考工程清屏只做一次全刷，刷新完成后再写一次 RAM
  // 保持控制器缓存一致，但不再次触发局刷。
  _Init_Full();
  _writeFullScreenBuffer(value);
  _Update_Full();
  _initial_write = false;
  _initial_refresh = false;
  _writeFullScreenBuffer(value);
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
  _writeFullScreenBuffer(value);
}

void GxEPD2_420_SSD1619A::_writeFullScreenBuffer(uint8_t value)
{
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  _writeCommand(0x24);
  _writeRepeatedData(value, uint32_t(WIDTH) * uint32_t(HEIGHT) / 8);
}

void GxEPD2_420_SSD1619A::_writeRepeatedData(uint8_t value, uint32_t count)
{
  _startTransfer();
  for (uint32_t i = 0; i < count; i++)
  {
    _transfer(value);
  }
  _endTransfer();
}

void GxEPD2_420_SSD1619A::_writeMirroredXImageData(
    const uint8_t bitmap[], const ImageTransferSpec &spec)
{
  // 关键逻辑：左右镜像不能只反算 RAM 窗口；SSD1619A 按 X 递增接收字节，
  // 所以每行必须从逻辑右侧字节开始写，并反转字节内 bit 顺序。
  // 这样全刷和任意局刷窗口都会在物理屏上同步左右翻转。
  _startTransfer();
  for (int16_t i = 0; i < spec.outputRows; i++)
  {
    int16_t row = spec.mirrorY ? spec.sourceHeight - 1 - (spec.baseY + i)
                               : spec.baseY + i;
    int16_t rowBase = row * spec.sourceWidthBytes;
    for (int16_t j = 0; j < spec.outputBytes; j++)
    {
      int16_t col = spec.baseXBytes + spec.outputBytes - 1 - j;
      uint8_t data = readBitmapByte(bitmap, rowBase + col, spec.pgm);
      if (spec.invert) data = ~data;
      _transfer(reverseBits(data));
    }
  }
  _endTransfer();
}

void GxEPD2_420_SSD1619A::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if (!_using_partial_mode) _Init_Part();
  _writeImageData(bitmap, x, y, w, h, invert, mirror_y, pgm);
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_420_SSD1619A::writeImageForFullRefresh(
    const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
    bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) _initial_write = false;
  if (_using_partial_mode || _hibernating || !_power_is_on) _Init_Full();
  _writeImageData(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_420_SSD1619A::_writeImageData(const uint8_t bitmap[], int16_t x,
                                          int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y,
                                          bool pgm)
{
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
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(0x24);
  ImageTransferSpec spec = {wb, h, int16_t(dx / 8), dy, int16_t(w1 / 8),
                            h1, invert, mirror_y, pgm};
  _writeMirroredXImageData(bitmap, spec);
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
  ImageTransferSpec spec = {wb_bitmap, h_bitmap, int16_t(x_part / 8 + dx / 8),
                            int16_t(y_part + dy), int16_t(w1 / 8), h1,
                            invert, mirror_y, pgm};
  _writeMirroredXImageData(bitmap, spec);
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_420_SSD1619A::writeImageAgain(const uint8_t bitmap[], int16_t x,
                                          int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y, bool pgm)
{
  // 关键逻辑：刷新后的缓存同步只写 RAM，不应重新执行局刷初始化；
  // 参考工程在 EPD_Update 后也会再次写入图像缓存但不再次刷新。
  bool previousPartialMode = _using_partial_mode;
  if (!_hibernating) _using_partial_mode = true;
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  _using_partial_mode = previousPartialMode;
}

void GxEPD2_420_SSD1619A::writeImagePartAgain(
    const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap,
    int16_t h_bitmap, int16_t x, int16_t y, int16_t w, int16_t h, bool invert,
    bool mirror_y, bool pgm)
{
  bool previousPartialMode = _using_partial_mode;
  if (!_hibernating) _using_partial_mode = true;
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h,
                 invert, mirror_y, pgm);
  _using_partial_mode = previousPartialMode;
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
}

void GxEPD2_420_SSD1619A::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint16_t physicalX = WIDTH - (x + w);
  uint16_t physicalXEnd = physicalX + w - 1;
  uint16_t yStart = HEIGHT - 1 - y;
  uint16_t yEnd = HEIGHT - (y + h);

  // 关键逻辑：SSD1619A 参考工程使用 0x01，表示 X 递增、Y 递减；
  // RAM 窗口和指针必须同步做 Y 翻转，否则内容会错位或不可见。
  // 当前面板物理方向还需要左右镜像，因此只在控制器 RAM 层反算 X 窗口；
  // UI 层坐标保持正常，局刷窗口与实际写入区域才能一起镜像。
  _writeCommand(0x11);
  _writeData(0x01);
  _writeCommand(0x44);
  _writeData(physicalX / 8);
  _writeData(physicalXEnd / 8);
  _writeCommand(0x45);
  _writeData(yStart % 256);
  _writeData(yStart / 256);
  _writeData(yEnd % 256);
  _writeData(yEnd / 256);
  _writeCommand(0x4e);
  _writeData(physicalX / 8);
  _writeCommand(0x4f);
  _writeData(yStart % 256);
  _writeData(yStart / 256);
}

bool GxEPD2_420_SSD1619A::_waitUntilIdle(const char *comment,
                                         uint32_t timeoutMs)
{
  if (_busy < 0) {
    delay(timeoutMs);
    return true;
  }

  uint32_t startMs = millis();
  while (digitalRead(_busy) != LOW) {
    delay(BUSY_POLL_INTERVAL_MS);
    if (millis() - startMs > timeoutMs) {
      Serial.print(comment);
      Serial.println(" Busy Timeout!");
      return false;
    }
    yield();
  }
  return true;
}

void GxEPD2_420_SSD1619A::_PowerOn()
{
  // SSD1619A 的 0x22 更新控制由初始化阶段写入，刷新时只发 0x20。
  // 这里保留状态位，避免旧逻辑用 0xC0 覆盖参考工程的 0xC7。
  _power_is_on = true;
}

void GxEPD2_420_SSD1619A::_PowerOff()
{
  if (_hibernating) return;
  _waitUntilIdle("_PowerOff", POWER_BUSY_TIMEOUT_MS);
  _writeCommand(0x10);
  _writeData(0x01);
  _power_is_on = false;
  _using_partial_mode = false;
  _hibernating = true;
}

void GxEPD2_420_SSD1619A::_InitDisplay()
{
  // 关键逻辑：参考工程每次初始化都用 RST 唤醒控制器；进入 deep sleep 后
  // 也必须通过 reset 清除休眠状态，否则后续命令不会被可靠接收。
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
  _power_is_on = true;
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
  _InitDisplay();
  _writeCommandDataPGM(LUTDefault_full, sizeof(LUTDefault_full));
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_420_SSD1619A::_Init_Part()
{
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
  // 关键逻辑：参考工程初始化阶段已经写入 0x22=0xC7；
  // 刷新阶段只发送 0x20，不能再写 0xC4 覆盖更新控制位。
  _writeCommand(0x20);
  _waitUntilIdle("_Update_Full", FULL_BUSY_TIMEOUT_MS);
  _initial_refresh = false;
}

void GxEPD2_420_SSD1619A::_Update_Part()
{
  // 关键逻辑：局刷同样沿用初始化阶段的 0x22=0xC7，和参考工程一致。
  _writeCommand(0x20);
  _waitUntilIdle("_Update_Part", PARTIAL_BUSY_TIMEOUT_MS);
}
