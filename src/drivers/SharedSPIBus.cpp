#include "SharedSPIBus.h"
#include "config.h"
#include <Arduino.h>

namespace {
void deselectSharedSPIDevices() {
  digitalWrite(EPD_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
}

void configureSharedSPIPins() {
  pinMode(EPD_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(SD_EN, OUTPUT);
  deselectSharedSPIDevices();
}
} // namespace

SPIClass &SharedSPIBus::bus() { return SPI; }

void SharedSPIBus::begin() {
  configureSharedSPIPins();
  digitalWrite(SD_EN, SD_PWD_OFF);

  // 关键逻辑：屏幕和 SD 卡物理上共用 GPIO18/19/23 时，必须绑定到同一套
  // SPI 控制器；否则后初始化的外设会重写 GPIO Matrix，导致先初始化的设备失联。
  bus().begin(EPD_SCK, SPI_MISO, EPD_MOSI);
}

void SharedSPIBus::prepareDisplay() {
  // 关键逻辑：访问屏幕前先确保 SD 卡处于未选中状态，避免共享 SPI 时被另一个
  // 从设备错误响应，造成墨水屏初始化或刷屏卡死。
  deselectSharedSPIDevices();
}

void SharedSPIBus::prepareSDCard() {
  deselectSharedSPIDevices();
  digitalWrite(SD_EN, SD_PWD_ON);
  delay(20);
}

void SharedSPIBus::releaseSDCard() {
  deselectSharedSPIDevices();
  digitalWrite(SD_EN, SD_PWD_OFF);
}
