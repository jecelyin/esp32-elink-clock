#include "DisplayDriver.h"
#include "SharedSPIBus.h"

namespace {
void prepareDisplayTransfer() {
  // 关键逻辑：屏幕与 SD 卡共用 SPI，总线访问前必须先释放其他片选；
  // 否则上一次 SD 卡访问可能让屏幕初始化或局部刷新收不到正确命令。
  SharedSPIBus::prepareDisplay();
}
} // namespace

// 构造屏幕对象，参数顺序为 CS、DC、RST、BUSY。
DisplayDriver::DisplayDriver()
    : display(EPD2_DRV(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)) {}

void DisplayDriver::init() {
  SPISettings spisettings(4000000, MSBFIRST, SPI_MODE0);
  prepareDisplayTransfer();
  display.epd2.selectSPI(SharedSPIBus::bus(), spisettings);
  display.init(0, true, 10, true);
  // 关键逻辑：UI 坐标保持正常方向，左右镜像在 SSD1619A RAM 写入层完成；
  // 不使用 GxEPD2 mirror()，因为它不会同步镜像局刷窗口坐标。
  display.setRotation(2); // 0, 90, 180 or 270 degrees

  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setFontDirection(0);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void DisplayDriver::clear() {
  prepareDisplayTransfer();
  // 关键逻辑：SSD1619A 清屏按参考工程直接执行一次全刷清屏；
  // 走分页绘制会触发 GxEPD2 的二阶段缓存同步，导致启动时多余刷新。
  display.clearScreen(0xFF);
  powerOff();
}

void DisplayDriver::update() {
  prepareDisplayTransfer();
  display.display();
  powerOff();
}

void DisplayDriver::showMessage(const char *msg) {
  prepareDisplayTransfer();
  Serial.print("Display Message: ");
  Serial.println(msg);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    int16_t x = (display.width() - u8g2Fonts.getUTF8Width(msg)) / 2;
    int16_t y = display.height() / 2;
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(msg);
  } while (display.nextPage());
  powerOff();
}

void DisplayDriver::showStatus(const char *msg, int line) {
  prepareDisplayTransfer();
  int16_t lineHeight = 24;
  int16_t y = 30 + (line * lineHeight);
  int16_t h = lineHeight;

  display.setPartialWindow(0, y - lineHeight + 4, display.width(),
                           h);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2Fonts.setCursor(10, y);
    u8g2Fonts.print(msg);
  } while (display.nextPage());
  powerOff();
}

void DisplayDriver::powerOff() { display.powerOff(); }
