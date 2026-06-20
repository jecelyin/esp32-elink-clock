#include "config.h"
#include "drivers/AudioDriver.h"
#include "drivers/BatteryDriver.h"
#include "drivers/DisplayDriver.h"
#include "drivers/InputDriver.h"
#include "drivers/RadioDriver.h"
#include "drivers/RtcDriver.h"
#include "drivers/SDCardDriver.h"
#include "drivers/SensorDriver.h"
#include "drivers/SharedSPIBus.h"
#include "managers/AlarmManager.h"
#include "managers/ConfigManager.h"
#include "managers/ConnectionManager.h"
#include "managers/WeatherManager.h"
#include "ui/UIManager.h"
#include "utils/HardwareCheck.h"
#include "utils/I2CBus.h"
#include "utils/SleepLogger.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <SPIFFS.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

// Global Objects
DisplayDriver displayDriver;
RtcDriver rtcDriver;
SensorDriver sensorDriver;
BatteryDriver batteryDriver;
RadioDriver radioDriver;
AudioDriver audioDriver;
InputDriver inputDriver;
SDCardDriver sdCardDriver;

ConfigManager configManager;
ConnectionManager connectionManager;
WeatherManager weatherManager;
AlarmManager alarmManager;
MusicManager musicManager(&audioDriver, &sdCardDriver, &configManager);

UIManager uiManager(&displayDriver, &rtcDriver, &weatherManager, &sensorDriver,
                    &batteryDriver, &connectionManager, &alarmManager,
                    &radioDriver, &audioDriver, &musicManager, &configManager);

// #include <nvs_flash.h>

TaskHandle_t networkTaskHandle = NULL;

namespace {
constexpr uint32_t WIFI_SYNC_TIMEOUT_MS = 120000UL;
constexpr uint32_t BUTTON_WAKE_GRACE_MS = 120UL;
constexpr uint32_t USER_ACTIVITY_GRACE_MS = 2000UL;
constexpr uint32_t ALARM_IDLE_CHECK_INTERVAL_MS = 3600000UL;
constexpr uint32_t ACTIVE_LOOP_DELAY_MS = 50UL;
uint32_t g_lastButtonWakeMs = 0;
uint32_t g_lastUserActivityMs = 0;

void startSerialDebug() {
#if ENABLE_SERIAL_DEBUG
  Serial.begin(115200);
  delay(1000);
  Serial.println("System Starting with Power Optimization...");
#endif
}

void configurePowerSaving() {
  setCpuFrequencyMhz(80);
  btStop();

#if defined(CONFIG_PM_ENABLE) && CONFIG_PM_ENABLE
  esp_pm_config_t pm_config = {.max_freq_mhz = 80,
                               .min_freq_mhz = 40,
                               .light_sleep_enable = false};
  esp_pm_configure(&pm_config);
#endif
}

void configureBootPins() {
  pinMode(EPD_BUSY, INPUT);
  SharedSPIBus::begin();

  pinMode(CODEC_EN, OUTPUT);
  pinMode(AMP_EN, OUTPUT);
  pinMode(RADIO_EN, OUTPUT);

  digitalWrite(AMP_EN, LOW);
  digitalWrite(CODEC_EN, LOW);
  digitalWrite(RADIO_EN, LOW);
}

void initBootDrivers() {
  I2CBus::begin();
  delay(100);
  inputDriver.begin();
  configManager.begin();
  Serial.println("Config Manager Init Success");
  batteryDriver.begin();
  displayDriver.init();
  displayDriver.clear();
}

bool isHardwareCheckCurrent() {
  return configManager.config.hw_checked &&
         configManager.config.hw_check_version >= HardwareCheck::REQUIRED_VERSION;
}

void showStartupDeviceResult(const HardwareCheck::Device &device, bool ok,
                             uint8_t line) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%s: %s", device.name, ok ? "OK" : "FAIL");
  displayDriver.showStatus(buffer, line);
}

bool checkStartupDevice(const HardwareCheck::Device &device, uint8_t line) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "Checking %s...", device.name);
  displayDriver.showStatus(buffer, line);
  bool ok = HardwareCheck::checkDevice(device, &batteryDriver);
  showStartupDeviceResult(device, ok, line);
  delay(200);
  return ok;
}

bool runStartupDeviceChecks() {
  bool allOk = true;
  for (uint8_t i = 0; i < HardwareCheck::DEVICE_COUNT; i++) {
    const HardwareCheck::Device &device = HardwareCheck::DEVICES[i];
    allOk = checkStartupDevice(device, i + 1) && allOk;
  }
  return allOk;
}

void finishStartupHardwareCheck(bool allOk) {
  if (allOk) {
    displayDriver.showStatus("Hardware Check OK", 0);
    configManager.config.hw_checked = true;
    configManager.config.hw_check_version = HardwareCheck::REQUIRED_VERSION;
    configManager.save();
    delay(1000);
    return;
  }

  displayDriver.showStatus("Hardware Check Failed!", 0);
  while (1) {
    delay(10);
  }
}

void runStartupHardwareCheckIfNeeded() {
  if (isHardwareCheckCurrent())
    return;

  displayDriver.showStatus("Checking Hardware...", 0);
  HardwareCheck::setPowerEnabled(true);
  bool allOk = runStartupDeviceChecks();
  HardwareCheck::setPowerEnabled(false);
  finishStartupHardwareCheck(allOk);
}

void initRtcSensorAndStorage() {
  if (!rtcDriver.init()) {
    Serial.println("RTC Init Failed");
  } else {
    Serial.println("RTC Init Success");
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mount Success");
  }

  if (!sensorDriver.init()) {
    Serial.println("Sensor Init Failed");
  } else {
    Serial.println("Sensor Init Success");
  }
}

void initOptionalDrivers() {
  if (!batteryDriver.init()) {
    Serial.println("Battery Gauge Init Failed, ADC fallback enabled");
  } else {
    Serial.println("Battery Gauge Init Success");
  }

  radioDriver.init();
  Serial.println("Radio Init Success");
  Serial.println("Audio Driver Lazy Init Enabled");
}

void initManagers() {
  connectionManager.begin(&configManager, &rtcDriver);
  Serial.println("Connection Manager Init Success");
  alarmManager.begin();
  Serial.println("Alarm Manager Init Success");
  weatherManager.begin(&configManager);
  Serial.println("Weather Manager Init Success");
  Serial.println("UI Manager Init Starting...");
  uiManager.init();
  Serial.println("UI Manager Init Success");
}

bool isButtonHeld() {
  return digitalRead(KEY_LEFT) == LOW || digitalRead(KEY_RIGHT) == LOW ||
         digitalRead(KEY_ENTER) == LOW;
}

bool isWithinGracePeriod(uint32_t lastMs, uint32_t graceMs) {
  return lastMs != 0 && millis() - lastMs < graceMs;
}

void markUserActivity() { g_lastUserActivityMs = millis(); }

uint32_t getAlarmWakeDelayMs(uint32_t nowMs) {
  if (alarmManager.isRinging()) {
    return ALARM_IDLE_CHECK_INTERVAL_MS;
  }

  return alarmManager.getNextWakeDelayMs(rtcDriver.getSoftwareTime(), nowMs);
}

uint32_t getIdleSleepDelayMs() {
  uint32_t nowMs = millis();
  uint32_t nextDelayMs = uiManager.getIdleSleepIntervalMs();
  uint32_t alarmDelayMs = getAlarmWakeDelayMs(nowMs);
  uint32_t networkDelayMs =
      connectionManager.getNextScheduledWorkDelayMs(nowMs);
  if (alarmDelayMs < nextDelayMs) {
    nextDelayMs = alarmDelayMs;
  }
  if (networkDelayMs < nextDelayMs) {
    nextDelayMs = networkDelayMs;
  }
  return nextDelayMs;
}

void runScheduledTasks() {
  connectionManager.startScheduledSyncIfDue(millis());
  alarmManager.check(rtcDriver.getTime());
}

bool canEnterIdleSleep() {
  if (connectionManager.isNetworkEnabled() || alarmManager.isRinging() ||
      audioDriver.isPlaying() || isButtonHeld() ||
      isWithinGracePeriod(g_lastButtonWakeMs, BUTTON_WAKE_GRACE_MS) ||
      isWithinGracePeriod(g_lastUserActivityMs, USER_ACTIVITY_GRACE_MS)) {
    return false;
  }

  ScreenState state = uiManager.getCurrentState();
  return state != SCREEN_RADIO && state != SCREEN_MUSIC;
}

void idleOrLightSleep() {
  if (!canEnterIdleSleep()) {
    delay(ACTIVE_LOOP_DELAY_MS);
    return;
  }

  uint32_t sleepDelayMs = getIdleSleepDelayMs();
  if (sleepDelayMs == 0) {
    delay(ACTIVE_LOOP_DELAY_MS);
    return;
  }
  uint64_t sleepDurationUs = static_cast<uint64_t>(sleepDelayMs) * 1000ULL;

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(sleepDurationUs);
  gpio_wakeup_enable((gpio_num_t)KEY_LEFT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)KEY_RIGHT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)KEY_ENTER, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  SleepLogger::logEnterLightSleep();
  esp_light_sleep_start();

  esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  SleepLogger::logWakeFromLightSleep(wakeupCause);

  if (wakeupCause == ESP_SLEEP_WAKEUP_GPIO) {
    // 关键逻辑：被按键从轻睡眠唤醒后，需要立即把“当前仍按下”的状态
    // 同步给输入状态机，否则短按会在下一轮再次入睡前被吞掉。
    g_lastButtonWakeMs = millis();
    markUserActivity();
    inputDriver.syncWakePressedButtons();
  }
}
} // namespace

#include <esp_task_wdt.h>

void networkTask(void *pvParameters) {
  static uint32_t wifiSessionStart = 0;

  while (true) {
    uint32_t now = millis();

    if (connectionManager.isNetworkEnabled()) {
      if (wifiSessionStart == 0) {
        wifiSessionStart = now;
      }
      connectionManager.loop();
      alarmManager.updateHolidayCache(rtcDriver.getSoftwareTime());

      // Only update weather on Home or Weather screens
      if (uiManager.getCurrentState() == SCREEN_HOME ||
          uiManager.getCurrentState() == SCREEN_WEATHER) {
        weatherManager.update();
      }

      // 同步完成后关闭 WiFi 以省电
      // 判断条件：NTP 已同步；在首页/天气页时还要求天气刚更新
      ScreenState state = uiManager.getCurrentState();
      bool weatherNeeded = state == SCREEN_HOME || state == SCREEN_WEATHER;
      bool weatherFresh = millis() - weatherManager.getLastUpdate() < 120000;
      if (connectionManager.isSyncComplete() &&
          (!weatherNeeded || weatherFresh)) {
        Serial.println("Sync Complete, powering off WiFi...");
        connectionManager.enableNetwork(false);
        wifiSessionStart = 0;
      } else if (state != SCREEN_NETWORK_CONFIG &&
                 now - wifiSessionStart > WIFI_SYNC_TIMEOUT_MS) {
        Serial.println("WiFi sync timeout, powering off WiFi...");
        connectionManager.enableNetwork(false);
        wifiSessionStart = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // 增加延迟，降低轮询开销
  }
}

void setup() {
  startSerialDebug();
  configurePowerSaving();
  configureBootPins();
  initBootDrivers();
  runStartupHardwareCheckIfNeeded();
  initRtcSensorAndStorage();
  initOptionalDrivers();
  initManagers();

  // Create background network task on Core 0 (shared with WiFi protocol stack)
  // This physically isolates network logic from the main UI/Hardware thread on
  // Core 1 (Arduino default).
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1,
                          &networkTaskHandle, 0); // Moved to Core 0
}

void loop() {
  uint32_t t_start = millis();

  runScheduledTasks();
  connectionManager.flushPendingRtcSync();

  t_start = millis();
  uiManager.update();
  // Serial.printf("UI update: %ums\n", millis() - t_start);

  t_start = millis();
  audioDriver.loop(); // For audio processing
  // Serial.printf("Audio loop: %ums\n", millis() - t_start);

  if (alarmManager.isRinging()) {
    if (!audioDriver.isPlaying()) {
      audioDriver.playFromFS(SPIFFS, "/alarm.mp3");
    }
  }

  // Handle Input
  t_start = millis();
  ButtonEvent btn = inputDriver.loop();
  // Serial.printf("Input loop: %ums\n", millis() - t_start);

  if (btn != BTN_NONE) {
    markUserActivity();
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

  // 外设电源动态管理
  if (!alarmManager.isRinging() && !audioDriver.isPlaying()) {
    if (uiManager.getCurrentState() != SCREEN_MUSIC)
      audioDriver.end();
  } else {
    digitalWrite(CODEC_EN, HIGH);
    // digitalWrite(AMP_EN, HIGH); // 由音频驱动自行控制更佳
  }

  // 仅在收音机界面且处于活动状态时开启收音机电源
  if (uiManager.getCurrentState() != SCREEN_RADIO) {
    digitalWrite(RADIO_EN, LOW);
  }

  idleOrLightSleep();
}
