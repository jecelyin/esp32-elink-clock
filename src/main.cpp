#include "config.h"
#include "drivers/AudioDriver.h"
#include "drivers/DisplayDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/RadioDriver.h"
#include "drivers/RtcDriver.h"
#include "drivers/SDCardDriver.h"
#include "drivers/SensorDriver.h"
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
InputDriver inputDriver;
SDCardDriver sdCardDriver;

ConfigManager configManager;
ConnectionManager connectionManager;
WeatherManager weatherManager;
AlarmManager alarmManager;

UIManager uiManager(&displayDriver, &rtcDriver, &weatherManager, &sensorDriver,
                    &connectionManager, &alarmManager, &radioDriver,
                    &audioDriver, &configManager);

// #include <nvs_flash.h>

bool networkStarted = false;

void networkTask(void *pvParameters) {
  while (true) {
    connectionManager.loop();

    // Only update weather on Home or Weather screens
    if (uiManager.getCurrentState() == SCREEN_HOME ||
        uiManager.getCurrentState() == SCREEN_WEATHER) {
      weatherManager.update();
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Run every 100ms
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("System Starting...");

  // Init Bus Manager (Arbitration for SPI/I2C on 18/23)
  BusManager::getInstance().begin();

  pinMode(EPD_BUSY, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_CS, OUTPUT);
  pinMode(SD_EN, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_EN, SD_PWD_OFF);

  // Init Drivers
  inputDriver.begin();

  sdCardDriver.begin();
  displayDriver.init();
  displayDriver.showMessage("Initializing...");

  // i2c 需要拉高CODEC_EN
  pinMode(CODEC_EN, OUTPUT);
  digitalWrite(CODEC_EN, HIGH);

  if (!rtcDriver.init()) {
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

  // Create background network task
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
  uint32_t t_start = millis();
  alarmManager.check(rtcDriver.getTime());
  // Serial.printf("Alarm check: %ums\n", millis() - t_start);

  t_start = millis();
  uiManager.update();
  // Serial.printf("UI update: %ums\n", millis() - t_start);

  // Signal network startup after first UI draw
  if (!networkStarted) {
    networkStarted = true;
    connectionManager.enableNetwork(true);
  }

  t_start = millis();
  audioDriver.loop(); // For audio processing
  // Serial.printf("Audio loop: %ums\n", millis() - t_start);

  if (alarmManager.isRinging()) {
    if (!audioDriver.isPlaying()) {
      // Ensure audio is init
      audioDriver.init();
      audioDriver.play("/alarm.mp3");
    }
  }

  // Handle Input
  t_start = millis();
  ButtonEvent btn = inputDriver.loop();
  // Serial.printf("Input loop: %ums\n", millis() - t_start);

  if (btn != BTN_NONE) {
    Serial.print("Button Event: ");
    Serial.println((int)btn);
    if (btn == BTN_ENTER_LONG) {
      uiManager.onLongPressEnter();
    } else {
      // Map other buttons to UI Manager inputs
      UIKey key = UI_KEY_NONE;
      if (btn == BTN_ENTER_SHORT)
        key = UI_KEY_ENTER;
      if (btn == BTN_LEFT_CLICK)
        key = UI_KEY_LEFT;
      if (btn == BTN_RIGHT_CLICK)
        key = UI_KEY_RIGHT;

      if (key != UI_KEY_NONE)
        uiManager.handleInput(key);
    }
  }

  delay(1); // Give some time for background tasks
}
