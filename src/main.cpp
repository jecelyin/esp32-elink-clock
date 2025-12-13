#include "config.h"
#include "drivers/AudioDriver.h"
#include "drivers/DisplayDriver.h"
#include "drivers/RadioDriver.h"
#include "drivers/RtcDriver.h"
#include "drivers/SensorDriver.h"
#include "drivers/SDCardDriver.h"
#include "managers/AlarmManager.h"
#include "managers/BusManager.h"
#include "managers/ConfigManager.h"
#include "managers/ConnectionManager.h"
#include "managers/WeatherManager.h"
#include "ui/UIManager.h"
#include <Arduino.h>

// Global Objects
DisplayDriver displayDriver;
RtcDriver rtcDriver;
SensorDriver sensorDriver;
RadioDriver radioDriver;
AudioDriver audioDriver;
SDCardDriver sdCardDriver;

ConfigManager configManager;
ConnectionManager connectionManager;
WeatherManager weatherManager;
AlarmManager alarmManager;

UIManager uiManager(&displayDriver, &rtcDriver, &weatherManager,
                    &connectionManager, &alarmManager, &radioDriver,
                    &audioDriver, &configManager);

// #include <nvs_flash.h>

void setup() {
  Serial.begin(115200);
  Serial.println("System Starting...");

  // Initialize NVS
  // esp_err_t ret = nvs_flash_init();
  // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
  //   ESP_ERROR_CHECK(nvs_flash_erase());
  //   ret = nvs_flash_init();
  // }
  // ESP_ERROR_CHECK(ret);

  // Init Bus Manager (Arbitration for SPI/I2C on 18/23)
  BusManager::getInstance().begin();

  pinMode(SD_EN, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_EN, SD_PWD_OFF);
  // Init Drivers
  sdCardDriver.begin();
  displayDriver.init();
  displayDriver.showMessage("Initializing...");
  // i2c 需要拉高CODEC_EN
  pinMode(CODEC_EN, OUTPUT);
  digitalWrite(CODEC_EN, HIGH);
  // Wire.begin() is handled by BusManager on demand
  // scan i2c devices
  // int nDevices = 0;
  // for (int i = 1; i < 127; i++) {
  //   Wire.beginTransmission(i);
  //   if (Wire.endTransmission() == 0) {
  //     Serial.print("Device found at address 0x");
  //     Serial.println(i, HEX);
  //     nDevices++;
  //   }
  // }
  if (!rtcDriver.init()){
    Serial.println("RTC Init Failed");
    return;
  }
  Serial.println("RTC Init Success");
  if (!sensorDriver.init())
    Serial.println("Sensor Init Failed");
  Serial.println("Sensor Init Success");
  radioDriver.init();
  Serial.println("Radio Init Success");
  audioDriver.init();
  Serial.println("Audio Init Success");
  configManager.begin();
  Serial.println("Config Manager Init Success");
  connectionManager.begin(&configManager, &rtcDriver);
  Serial.println("Connection Manager Init Success");
  weatherManager.begin(&configManager);
  Serial.println("Weather Manager Init Success");
  uiManager.init();
  Serial.println("UI Manager Init Success");
  // Check WiFi
  // if (configManager.config.wifi_ssid.length() == 0) {
    // displayDriver.showMessage("Setup WiFi: Connect to ESP32-Clock");
  // }
}

void loop() {
  connectionManager.loop();
  weatherManager.update();
  alarmManager.check(rtcDriver.getTime());
  uiManager.update();
  audioDriver.loop(); // For audio processing

  if (alarmManager.isRinging()) {
    if (!audioDriver.isPlaying()) {
      // Ensure audio is init
      audioDriver.init();
      audioDriver.play("/alarm.mp3");
    }
  }

  // Simple Serial Input for testing
  // if (Serial.available()) {
  //   char c = Serial.read();
  //   if (c == 'm')
  //     uiManager.handleInput(1); // Menu
  // }

  delay(10);
}
