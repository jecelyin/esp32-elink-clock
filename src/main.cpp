#include "config.h"
#include "drivers/AudioDriver.h"
#include "drivers/DisplayDriver.h"
#include "drivers/RadioDriver.h"
#include "drivers/RtcDriver.h"
#include "drivers/SensorDriver.h"
#include "drivers/SDCardDriver.h"
#include "managers/AlarmManager.h"
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

void setup() {
  Serial.begin(115200);
  Serial.println("System Starting...");

  pinMode(SD_EN, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_EN, SD_PWD_OFF);
  // Init Drivers
  displayDriver.init();
  displayDriver.showMessage("Initializing...");
  // sdCardDriver.begin();
  // Wire.begin(I2C_SDA, I2C_SCL);
  // if (!rtcDriver.init()){
  //   Serial.println("RTC Init Failed");
  //   return;
  // }
  // if (!sensorDriver.init())
  //   Serial.println("Sensor Init Failed");
  // Radio and Audio init might need to be delayed or handled carefully
  // radioDriver.init();
  // audioDriver.init();

  // Init Managers
  // configManager.begin();
  // connectionManager.begin(&configManager, &rtcDriver);
  // weatherManager.begin(&configManager);

  // uiManager.init();

  // Check WiFi
  // if (configManager.config.wifi_ssid.length() == 0) {
    // displayDriver.showMessage("Setup WiFi: Connect to ESP32-Clock");
  // }
}

void loop() {
  // connectionManager.loop();
  // weatherManager.update();
  // alarmManager.check(rtcDriver.getTime());
  // uiManager.update();
  // audioDriver.loop(); // For audio processing

  // Handle Alarm Ringing
  // if (alarmManager.isRinging()) {
  //   if (!audioDriver.isPlaying()) {
  //     // Ensure audio is init
  //     audioDriver.init();
  //     audioDriver.play("/alarm.mp3");
  //   }
  // }

  // Simple Serial Input for testing
  // if (Serial.available()) {
  //   char c = Serial.read();
  //   if (c == 'm')
  //     uiManager.handleInput(1); // Menu
  // }

  delay(10);
}
