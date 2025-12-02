#include "DisplayDriver.h"

// Constructor: Initialize the display object with pin definitions
// GxEPD2_420(int16_t cs, int16_t dc, int16_t rst, int16_t busy)
DisplayDriver::DisplayDriver()
    : display(GxEPD2_420_SSD1619(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)) {}

void DisplayDriver::init() {
  // SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS); // Initialize SPI if needed
  // SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  // SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SPISettings spisettings(4000000, MSBFIRST, SPI_MODE0);
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(SPI, spisettings);
  // explicitly, GxEPD2 usually handles it if standard pins
  // display.init(115200);
  display.init(0, true, 10, false);
  display.setRotation(0); // Adjust as needed

  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1); // Transparent
  u8g2Fonts.setFontDirection(0);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void DisplayDriver::clear() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
}

void DisplayDriver::update() {
  // This might be used for partial updates or triggering a refresh
  display.display();
}

void DisplayDriver::showMessage(const char *msg) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    int16_t x = (display.width() - u8g2Fonts.getUTF8Width(msg)) / 2;
    int16_t y = display.height() / 2;
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(msg);
  } while (display.nextPage());
}
