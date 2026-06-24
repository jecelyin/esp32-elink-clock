#include "SettingsScreen.h"
#include "../../managers/ConfigPortal.h"
#include "../../utils/HardwareCheck.h"
#include "../UIManager.h"
#include <WiFi.h>

namespace {
constexpr int SIDEBAR_W = 128;
constexpr int CONTENT_X = SIDEBAR_W + 14;
constexpr int CONTENT_W = 400 - CONTENT_X - 10;
constexpr int BODY_Y = 24;
constexpr int QR_SIZE = 112;
const char *MENU_LABELS[] = {"检查器件", "网络配置", "系统设置"};
} // namespace

SettingsScreen::SettingsScreen(ConfigManager *config, StatusBar *statusBar,
                               BatteryDriver *battery,
                               ConnectionManager *conn)
    : config(config), statusBar(statusBar), battery(battery), conn(conn) {}

void SettingsScreen::enter() {
  selectedItem = MENU_HARDWARE;
  redrawAfterInput = true;
}

void SettingsScreen::exit() {
  // 系统配置服务只在设置页存活，避免离开页面后与音乐/闹铃争用 SD。
  if (conn->isSystemPortalActive()) {
    conn->enableNetwork(false);
  }
}

void SettingsScreen::draw(DisplayDriver *display) {
  BatteryInfo info = battery->readInfo();
  display->display.setFullWindow();
  display->display.firstPage();
  do {
    drawPage(display, info);
  } while (display->display.nextPage());
  display->powerOff();
}

bool SettingsScreen::onInput(UIKey key) {
  redrawAfterInput = true;
  if (key == UI_KEY_LEFT) {
    selectedItem = (selectedItem + MENU_COUNT - 1) % MENU_COUNT;
    return true;
  }
  if (key == UI_KEY_RIGHT) {
    selectedItem = (selectedItem + 1) % MENU_COUNT;
    return true;
  }
  if (key != UI_KEY_ENTER) {
    return false;
  }

  if (selectedItem == MENU_HARDWARE) {
    redrawAfterInput = false;
    runManualHardwareCheck();
  } else if (selectedItem == MENU_NETWORK) {
    conn->startAP();
  } else {
    conn->startSystemAP();
  }
  return true;
}

bool SettingsScreen::shouldDrawAfterInput() const {
  return redrawAfterInput;
}

void SettingsScreen::drawPage(DisplayDriver *display,
                              const BatteryInfo &info) {
  display->display.fillScreen(GxEPD_WHITE);
  statusBar->draw(display, true);
  drawSidebar(display);
  drawContent(display, info);
}

void SettingsScreen::drawSidebar(DisplayDriver *display) {
  auto &epd = display->display;
  epd.drawLine(SIDEBAR_W, BODY_Y, SIDEBAR_W, 299, GxEPD_BLACK);
  drawText(display, 16, 48, "设置", u8g2_font_wqy16_t_gb2312);
  for (uint8_t i = 0; i < MENU_COUNT; ++i) {
    drawMenuItem(display, i, 70 + i * 58);
  }
  drawText(display, 32, 266, "左右选择", u8g2_font_wqy12_t_gb2312);
  drawText(display, 32, 288, "确认进入", u8g2_font_wqy12_t_gb2312);
}

void SettingsScreen::drawMenuItem(DisplayDriver *display, uint8_t index,
                                  int y) {
  bool selected = selectedItem == index;
  uint16_t background = selected ? GxEPD_BLACK : GxEPD_WHITE;
  uint16_t foreground = selected ? GxEPD_WHITE : GxEPD_BLACK;
  display->display.fillRect(8, y, 112, 42, background);
  display->display.drawRect(8, y, 112, 42, GxEPD_BLACK);
  display->u8g2Fonts.setForegroundColor(foreground);
  display->u8g2Fonts.setBackgroundColor(background);
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(24, y + 27);
  display->u8g2Fonts.print(MENU_LABELS[index]);
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void SettingsScreen::drawContent(DisplayDriver *display,
                                 const BatteryInfo &info) {
  if (selectedItem == MENU_HARDWARE) {
    drawHardwareContent(display, info);
  } else if (selectedItem == MENU_NETWORK) {
    drawNetworkContent(display);
  } else {
    drawSystemContent(display);
  }
}

void SettingsScreen::drawHardwareContent(DisplayDriver *display,
                                         const BatteryInfo &info) {
  drawText(display, CONTENT_X, 52, "检查器件",
           u8g2_font_wqy16_t_gb2312);
  drawText(display, CONTENT_X, 78, "按确认键开始完整硬件检测",
           u8g2_font_wqy12_t_gb2312);
  for (uint8_t i = 0; i < HardwareCheck::DEVICE_COUNT; ++i) {
    drawText(display, CONTENT_X + 8, 110 + i * 24,
             HardwareCheck::DEVICES[i].name, u8g2_font_helvR10_tf);
  }

  char batteryLine[48];
  if (info.dataValid) {
    snprintf(batteryLine, sizeof(batteryLine), "Battery: %.2fV  %.0f%%",
             info.voltage, info.socPercent);
  } else {
    snprintf(batteryLine, sizeof(batteryLine), "Battery: --");
  }
  drawText(display, CONTENT_X, 250, batteryLine, u8g2_font_helvR10_tf);
}

void SettingsScreen::drawNetworkContent(DisplayDriver *display) {
  String address = "http://" + getGatewayIp();
  drawPortalContent(display, ConfigPortal::buildWiFiQrPayload(), "网络配置",
                    address, conn->isConfigPortalActive());
}

void SettingsScreen::drawSystemContent(DisplayDriver *display) {
  String address = ConfigPortal::buildSystemUrl(getGatewayIp());
  drawPortalContent(display, ConfigPortal::buildWiFiQrPayload(), "系统设置",
                    address,
                    conn->isSystemPortalActive());
}

void SettingsScreen::drawPortalContent(DisplayDriver *display,
                                       const String &payload,
                                       const char *title,
                                       const String &address, bool active) {
  drawText(display, CONTENT_X, 52, title, u8g2_font_wqy16_t_gb2312);
  drawQrCode(display, payload, CONTENT_X, 66, QR_SIZE);
  drawText(display, CONTENT_X + 126, 82, active ? "已开启" : "按确认键开启",
           u8g2_font_wqy12_t_gb2312);
  drawText(display, CONTENT_X + 126, 112, "SSID",
           u8g2_font_helvB08_tr);
  drawText(display, CONTENT_X + 126, 132, ConfigPortal::AP_SSID,
           u8g2_font_helvR10_tf);
  drawText(display, CONTENT_X + 126, 158, "PASSWORD",
           u8g2_font_helvB08_tr);
  String password = ConfigPortal::getAPPassword();
  drawText(display, CONTENT_X + 126, 178, password.c_str(),
           u8g2_font_helvR10_tf);
  drawText(display, CONTENT_X, 208, "访问地址",
           u8g2_font_wqy12_t_gb2312);
  drawText(display, CONTENT_X, 230, address.c_str(),
           u8g2_font_helvR10_tf);
  drawText(display, CONTENT_X, 268,
           selectedItem == MENU_SYSTEM ? "连接热点后用浏览器访问"
                                       : "连接热点后打开配置地址",
           u8g2_font_wqy12_t_gb2312);
}

void SettingsScreen::drawQrCode(DisplayDriver *display, const String &payload,
                                int x, int y, int boxSize) {
  SimpleQRCode qr;
  if (!qr.encodeText(payload.c_str())) {
    drawQrFallback(display, x, y, boxSize);
    return;
  }
  int moduleSize = boxSize / (qr.size() + 8);
  if (moduleSize <= 0) {
    drawQrFallback(display, x, y, boxSize);
    return;
  }
  drawQrModules(display, qr, x, y, boxSize, moduleSize);
}

void SettingsScreen::drawQrModules(DisplayDriver *display,
                                   const SimpleQRCode &qr, int x, int y,
                                   int boxSize, int moduleSize) {
  int qrPixels = (qr.size() + 8) * moduleSize;
  int originX = x + (boxSize - qrPixels) / 2 + 4 * moduleSize;
  int originY = y + (boxSize - qrPixels) / 2 + 4 * moduleSize;
  display->display.fillRect(x, y, boxSize, boxSize, GxEPD_WHITE);
  for (uint8_t row = 0; row < qr.size(); ++row) {
    for (uint8_t col = 0; col < qr.size(); ++col) {
      if (qr.isDark(col, row)) {
        display->display.fillRect(originX + col * moduleSize,
                                  originY + row * moduleSize, moduleSize,
                                  moduleSize, GxEPD_BLACK);
      }
    }
  }
}

void SettingsScreen::drawQrFallback(DisplayDriver *display, int x, int y,
                                    int boxSize) {
  display->display.drawRect(x, y, boxSize, boxSize, GxEPD_BLACK);
  drawText(display, x + 30, y + boxSize / 2, "QR ERR",
           u8g2_font_helvB10_tf);
}

void SettingsScreen::drawText(DisplayDriver *display, int x, int y,
                              const char *text, const uint8_t *font) {
  display->u8g2Fonts.setFont(font);
  display->u8g2Fonts.setCursor(x, y);
  display->u8g2Fonts.print(text);
}

String SettingsScreen::getGatewayIp() const {
  IPAddress ip = WiFi.softAPIP();
  if (ip == IPAddress(0, 0, 0, 0)) {
    return ConfigPortal::DEFAULT_GATEWAY;
  }
  return ip.toString();
}

void SettingsScreen::runManualHardwareCheck() {
  DisplayDriver *display = uiManager->getDisplayDriver();
  display->showStatus("Manual Check...", 0);
  HardwareCheck::setPowerEnabled(true);
  bool allOk = runDeviceChecks(display);
  HardwareCheck::setPowerEnabled(false);
  finishManualHardwareCheck(display, allOk);
}

bool SettingsScreen::runDeviceChecks(DisplayDriver *display) {
  bool allOk = true;
  for (uint8_t i = 0; i < HardwareCheck::DEVICE_COUNT; ++i) {
    const HardwareCheck::Device &device = HardwareCheck::DEVICES[i];
    bool ok = HardwareCheck::checkDevice(device, battery);
    allOk = ok && allOk;
    showDeviceCheckResult(display, device.shortName, ok, i + 1);
    delay(200);
  }
  return allOk;
}

void SettingsScreen::showDeviceCheckResult(DisplayDriver *display,
                                           const char *name, bool ok,
                                           uint8_t line) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%s: %s", name, ok ? "OK" : "FAIL");
  display->showStatus(buffer, line);
}

void SettingsScreen::finishManualHardwareCheck(DisplayDriver *display,
                                               bool allOk) {
  if (allOk) {
    config->config.hw_checked = true;
    config->config.hw_check_version = HardwareCheck::REQUIRED_VERSION;
    config->saveHardwareCheck();
    display->showStatus("All Devices OK", 0);
  } else {
    display->showStatus("Check Failed!", 0);
  }
  delay(2000);
  draw(display);
}
