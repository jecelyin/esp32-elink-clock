#include "config.h"
#include "drivers/AlarmPcmPlayer.h"
#include "drivers/AudioDriver.h"
#include "drivers/BatteryDriver.h"
#include "drivers/DisplayDriver.h"
#include "drivers/ES8311AlarmCodec.h"
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
AlarmPcmPlayer alarmPcmPlayer;
InputDriver inputDriver;
SDCardDriver sdCardDriver;

ConfigManager configManager;
ConnectionManager connectionManager;
WeatherManager weatherManager;
AlarmManager alarmManager;
MusicManager musicManager(&audioDriver, &sdCardDriver, &configManager);

UIManager uiManager(&displayDriver, &rtcDriver, &weatherManager, &sensorDriver,
                    &batteryDriver, &connectionManager, &alarmManager,
                    &radioDriver, &audioDriver, &musicManager, &configManager,
                    &sdCardDriver);

// #include <nvs_flash.h>

TaskHandle_t networkTaskHandle = NULL;

namespace {
constexpr uint32_t WIFI_SYNC_TIMEOUT_MS = 120000UL;
constexpr uint32_t BUTTON_WAKE_GRACE_MS = 120UL;
constexpr uint32_t USER_ACTIVITY_GRACE_MS = 2000UL;
constexpr uint32_t ALARM_IDLE_CHECK_INTERVAL_MS = 3600000UL;
constexpr uint32_t ACTIVE_LOOP_DELAY_MS = 50UL;
constexpr uint32_t PCM_ACTIVE_LOOP_DELAY_MS = 1UL;
constexpr uint8_t ALARM_PLAYBACK_VOLUME = 21; // ESP32-audioI2S maximum.
constexpr uint8_t IDLE_CPU_FREQUENCY_MHZ = 80;
constexpr uint8_t ALARM_CPU_FREQUENCY_MHZ = 160;
uint32_t g_lastButtonWakeMs = 0;
uint32_t g_lastUserActivityMs = 0;
uint32_t g_lastAlarmPlaybackAttemptMs = 0;
uint32_t g_alarmPlaybackTriggerSequence = 0;
bool g_alarmOwnsAudio = false;
bool g_alarmCodecReady = false;

void startSerialDebug() {
#if ENABLE_SERIAL_DEBUG
  Serial.begin(115200);
  delay(1000);
  Serial.println("System Starting with Power Optimization...");
#endif
}

void configurePowerSaving() {
  setCpuFrequencyMhz(IDLE_CPU_FREQUENCY_MHZ);
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
  audioDriver.setVolume(configManager.config.volume);
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
    configManager.saveHardwareCheck();
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
  alarmManager.begin(&configManager);
  if (ALARM_STARTUP_TEST_ENABLED) {
    alarmManager.addStartupTestAlarm(rtcDriver.getTime(),
                                     ALARM_STARTUP_TEST_DELAY_MINUTES);
  }
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

void restoreRadioAfterAlarmIfNeeded() {
  if (uiManager.getCurrentState() != SCREEN_RADIO) {
    return;
  }

  // AudioDriver::stop() removes the complete 3.3V_codec rail, which also
  // power-cycles the RDA5807. Raising RADIO_EN alone cannot restore registers;
  // initialize and reapply the saved settings before enabling the amplifier.
  digitalWrite(AMP_EN, LOW);
  digitalWrite(RADIO_EN, HIGH);
  radioDriver.setup();
  radioDriver.setVolume(configManager.config.volume);
  radioDriver.setBassBoost(configManager.config.radio_bass_boost);
  radioDriver.setMono(configManager.config.radio_force_mono);
  radioDriver.setSoftMute(configManager.config.radio_soft_mute);
  radioDriver.setSeekThreshold(configManager.config.radio_seek_threshold);
  digitalWrite(AMP_EN, HIGH);
  Serial.println("[Alarm] radio restored");
}

void releaseAlarmAudio() {
  alarmPcmPlayer.stop();
  audioDriver.stop();
  audioDriver.setVolume(configManager.config.volume);
  setCpuFrequencyMhz(IDLE_CPU_FREQUENCY_MHZ);
  g_alarmOwnsAudio = false;
  g_alarmCodecReady = false;
  g_lastAlarmPlaybackAttemptMs = 0;
  restoreRadioAfterAlarmIfNeeded();
}

bool prepareAlarmCodecForPlayback() {
  if (g_alarmCodecReady) {
    digitalWrite(AMP_EN, HIGH);
    return true;
  }

  // AudioDriver stays at its original implementation. Once it has created the
  // I2S clock, configure only the digital alarm path and keep the PA muted
  // until the ES8311 reference/output has settled.
  digitalWrite(AMP_EN, LOW);
  if (!ES8311AlarmCodec::begin()) {
    Serial.println("[Alarm] codec initialization failed");
    audioDriver.stop();
    return false;
  }
  g_alarmCodecReady = true;
  digitalWrite(AMP_EN, HIGH);
  return true;
}

bool isDirectionInput(UIKey key) {
  return key == UI_KEY_LEFT || key == UI_KEY_RIGHT ||
         key == UI_KEY_LEFT_LONG || key == UI_KEY_RIGHT_LONG;
}

const char *buttonEventName(ButtonEvent btn) {
  switch (btn) {
  case BTN_ENTER_SHORT:
    return "ENTER_SHORT";
  case BTN_ENTER_LONG:
    return "ENTER_LONG";
  case BTN_LEFT_SHORT:
    return "LEFT_SHORT";
  case BTN_LEFT_LONG:
    return "LEFT_LONG";
  case BTN_RIGHT_SHORT:
    return "RIGHT_SHORT";
  case BTN_RIGHT_LONG:
    return "RIGHT_LONG";
  default:
    return "NONE";
  }
}

const char *uiKeyName(UIKey key) {
  switch (key) {
  case UI_KEY_ENTER:
    return "ENTER";
  case UI_KEY_LEFT:
    return "LEFT";
  case UI_KEY_RIGHT:
    return "RIGHT";
  case UI_KEY_ENTER_LONG:
    return "ENTER_LONG";
  case UI_KEY_LEFT_LONG:
    return "LEFT_LONG";
  case UI_KEY_RIGHT_LONG:
    return "RIGHT_LONG";
  default:
    return "NONE";
  }
}

const char *screenStateName(ScreenState state) {
  switch (state) {
  case SCREEN_HOME:
    return "HOME";
  case SCREEN_MENU:
    return "MENU";
  case SCREEN_CALENDAR:
    return "CALENDAR";
  case SCREEN_ALARM:
    return "ALARM";
  case SCREEN_RADIO:
    return "RADIO";
  case SCREEN_MUSIC:
    return "MUSIC";
  case SCREEN_WEATHER:
    return "WEATHER";
  case SCREEN_SETTINGS:
    return "SETTINGS";
  case SCREEN_TIMER:
    return "TIMER";
  default:
    return "UNKNOWN";
  }
}

UIKey mapButtonEventToUIKey(ButtonEvent btn) {
  switch (btn) {
  case BTN_ENTER_SHORT:
    return UI_KEY_ENTER;
  case BTN_ENTER_LONG:
    return UI_KEY_ENTER_LONG;
  case BTN_LEFT_SHORT:
    return UI_KEY_LEFT;
  case BTN_LEFT_LONG:
    return UI_KEY_LEFT_LONG;
  case BTN_RIGHT_SHORT:
    return UI_KEY_RIGHT;
  case BTN_RIGHT_LONG:
    return UI_KEY_RIGHT_LONG;
  default:
    return UI_KEY_NONE;
  }
}

void clearEnterNoiseAfterDirection(UIKey key) {
  if (!isDirectionInput(key)) {
    return;
  }

  // 关键逻辑：方向键在所有页面都只负责移动光标。电子墨水屏局刷阻塞期间
  // 若 ENTER 被硬件串扰锁存，下一轮可能触发当前焦点动作或长按返回菜单；
  // 因此方向键处理后要清掉 ENTER 的全部待处理事件。
  inputDriver.clearPendingEnterPresses();
}

void handleInputEvents() {
  if (!uiManager.canAcceptInput()) {
    return;
  }

  inputDriver.clearPendingEnterPressesIfDirectionActive();
  ButtonEvent btn = inputDriver.loop();
  if (btn == BTN_NONE) {
    return;
  }

  markUserActivity();
  if (alarmManager.isRinging()) {
    // 关键逻辑：响铃是全局模态状态。确认键停止闹钟，左右键只消费不分发，
    // 防止用户在关闹钟时误触发当前页面的调频、切歌或配置操作。
    if (btn == BTN_ENTER_SHORT || btn == BTN_ENTER_LONG) {
      alarmManager.stop();
      releaseAlarmAudio();
      Serial.println("[Alarm] stopped by ENTER");
    }
    return;
  }
  UIKey key = mapButtonEventToUIKey(btn);
  if (key == UI_KEY_NONE) {
    return;
  }
  ScreenState beforeState = uiManager.getCurrentState();
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Input][dispatch] btn=%s(%d) key=%s(%d) screen=%s\n",
                buttonEventName(btn), btn, uiKeyName(key), key,
                screenStateName(beforeState));
#endif
  if (key == UI_KEY_ENTER_LONG) {
    // 关键逻辑：ENTER 长按进入菜单后，物理释放仍属于同一次手势。
    // 先屏蔽到释放稳定，避免菜单全刷期间的释放抖动被当成短 ENTER。
    inputDriver.suppressEnterUntilReleased();
  }
  bool accepted = uiManager.onInput(key);
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Input][result] accepted=%d screen=%s->%s\n", accepted,
                screenStateName(beforeState),
                screenStateName(uiManager.getCurrentState()));
#endif
  if (!accepted) {
    return;
  }
  clearEnterNoiseAfterDirection(key);
}

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
    // The direct PCM player feeds I2S in small chunks. A normal 50 ms UI idle
    // delay would starve DMA and create periodic gaps, so keep servicing it
    // until i2s_write itself provides the required pacing.
    delay(alarmPcmPlayer.isPlaying() ? PCM_ACTIVE_LOOP_DELAY_MS
                                     : ACTIVE_LOOP_DELAY_MS);
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

bool shouldUpdateWeatherWhileOnline(bool systemPortalActive);

void networkTask(void *pvParameters) {
  static uint32_t wifiSessionStart = 0;

  while (true) {
    uint32_t now = millis();
    // 所有 WiFi/WiFiManager 状态变更统一在 Core 0 执行，避免配置门户与
    // 后台连接流程跨核并发破坏 lwIP 内部状态。
    connectionManager.processPendingAction();

    if (connectionManager.isNetworkEnabled()) {
      if (wifiSessionStart == 0) {
        wifiSessionStart = now;
      }
      connectionManager.loop();
      bool systemPortalActive = connectionManager.isSystemPortalActive();

      // Keep the local configuration server as the only lwIP/HTTP workload
      // while the phone is connected to the SoftAP.
      if (!systemPortalActive) {
        alarmManager.updateHolidayCache(rtcDriver.getSoftwareTime());
        if (shouldUpdateWeatherWhileOnline(systemPortalActive)) {
          weatherManager.update();
        }
      }

      // 同步完成后关闭 WiFi 以省电
      // 判断条件：本轮 NTP 已同步；配置了天气 Token 时还要求天气刚更新。
      // 天气属于后台数据，不应由当前页面决定是否拉取。
      ScreenState state = uiManager.getCurrentState();
      bool weatherNeeded = configManager.getWeatherApiToken().length() > 0;
      bool weatherFresh = millis() - weatherManager.getLastUpdate() < 120000;
      bool settingsVisible = state == SCREEN_SETTINGS;
      if (!settingsVisible && connectionManager.isSyncComplete() &&
          (!weatherNeeded || weatherFresh)) {
        Serial.println("Sync Complete, powering off WiFi...");
        connectionManager.enableNetwork(false);
        wifiSessionStart = 0;
      } else if (!settingsVisible &&
                 now - wifiSessionStart > WIFI_SYNC_TIMEOUT_MS) {
        Serial.println("WiFi sync timeout, powering off WiFi...");
        connectionManager.enableNetwork(false);
        wifiSessionStart = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500)); // 增加延迟，降低轮询开销
  }
}

bool isScreenUsingSharedAudioPower(ScreenState state) {
  return state == SCREEN_MUSIC || state == SCREEN_RADIO;
}

bool shouldUpdateWeatherWhileOnline(bool systemPortalActive) {
  // The system portal owns the local AP socket set; postpone outbound HTTP
  // until the portal closes and the next normal STA synchronization begins.
  return !systemPortalActive;
}

void manageAudioPower(ScreenState state) {
  if (alarmManager.isRinging() || audioDriver.isPlaying()) {
    digitalWrite(CODEC_EN, HIGH);
    return;
  }

  if (isScreenUsingSharedAudioPower(state)) {
    return;
  }

  // 关键逻辑：audioDriver.end() 会关闭 AMP_EN 和共享 I2C 电源域。
  // 收音机使用同一个功放和 I2C 总线，不能在收音机页把它当作闲置音频关闭。
  audioDriver.end();
  // 系统配置页可能正在分块上传文件，不能在每次主循环末尾关闭 SD。
  if (!connectionManager.isSystemPortalActive()) {
    sdCardDriver.end();
  }
}

void manageRadioPower(ScreenState state) {
  if (state == SCREEN_RADIO && !alarmManager.isRinging()) {
    // 响铃抢占期间 RADIO_EN 会被拉低；确认停止后仍停留在收音机页时，
    // 这里恢复模拟音频通道，不要求用户退出页面再重新进入。
    digitalWrite(AMP_EN, HIGH);
    digitalWrite(RADIO_EN, HIGH);
    return;
  }

  digitalWrite(RADIO_EN, LOW);
}

bool playConfiguredAlarm() {
  uint32_t now = millis();
  if (g_lastAlarmPlaybackAttemptMs != 0 &&
      now - g_lastAlarmPlaybackAttemptMs < 5000UL) {
    return false;
  }
  g_lastAlarmPlaybackAttemptMs = now;

  String ringtone =
      AlarmRingtones::normalize(alarmManager.getActiveRingtone());

  if (ringtone.startsWith("sd:")) {
    String path = ringtone.substring(3);
    if (sdCardDriver.begin() && sdCardDriver.exists(path.c_str())) {
      audioDriver.playFromSD(path.c_str());
      return prepareAlarmCodecForPlayback();
    }
  }

  if (ringtone.startsWith("spiffs:")) {
    String path = ringtone.substring(7);
    if (SPIFFS.exists(path)) {
      if (path.endsWith(".wav")) {
        return alarmPcmPlayer.begin(SPIFFS, path.c_str());
      }
      audioDriver.playFromFS(SPIFFS, path.c_str());
      return prepareAlarmCodecForPlayback();
    }
  }

  const String fallbackPath =
      String(AlarmRingtones::DEFAULT_VALUE).substring(7);
  if (SPIFFS.exists(fallbackPath)) {
    Serial.printf("Alarm ringtone not found: %s, using default\n",
                  ringtone.c_str());
    return alarmPcmPlayer.begin(SPIFFS, fallbackPath.c_str());
  }
  Serial.printf("Alarm ringtone not found: %s\n", ringtone.c_str());
  return false;
}

void serviceAlarmPlayback() {
  if (!alarmManager.isRinging()) {
    if (g_alarmOwnsAudio) {
      releaseAlarmAudio();
    }
    return;
  }

  uint32_t triggerSequence = alarmManager.getTriggerSequence();
  if (!g_alarmOwnsAudio) {
    // 关键逻辑：闹钟必须抢占所有页面共享的音频通道。否则音乐页正在播放时，
    // isPlaying() 会一直为 true，旧逻辑便永远不会启动闹铃。
    alarmPcmPlayer.stop();
    audioDriver.stop();
    // 闹钟音量不受音乐/收音机用户音量限制，固定使用解码器
    // 最大档；停止响铃后再恢复 config 中的音量。
    setCpuFrequencyMhz(ALARM_CPU_FREQUENCY_MHZ);
    audioDriver.setVolume(ALARM_PLAYBACK_VOLUME);
    digitalWrite(RADIO_EN, LOW);
    g_alarmOwnsAudio = true;
    g_alarmCodecReady = false;
    g_lastAlarmPlaybackAttemptMs = 0;
    g_alarmPlaybackTriggerSequence = triggerSequence;
    Serial.println("[Alarm] audio channel acquired");
  } else if (triggerSequence != g_alarmPlaybackTriggerSequence) {
    // A later alarm must replace the one that is still ringing. A monotonic
    // trigger sequence is used instead of comparing ringtone paths because
    // consecutive alarms are allowed to select the same sound.
    alarmPcmPlayer.stop();
    audioDriver.stop();
    g_alarmCodecReady = false;
    g_lastAlarmPlaybackAttemptMs = 0;
    g_alarmPlaybackTriggerSequence = triggerSequence;
    Serial.println("[Alarm] active ringtone stopped for newer alarm");
  }

  if (!alarmPcmPlayer.isPlaying() && !audioDriver.isPlaying() &&
      playConfiguredAlarm()) {
    Serial.println("[Alarm] ringtone playback started");
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
  alarmPcmPlayer.loop();
  // Serial.printf("Audio loop: %ums\n", millis() - t_start);

  serviceAlarmPlayback();

  handleInputEvents();
  // Serial.printf("Input loop: %ums\n", millis() - t_start);

  ScreenState currentState = uiManager.getCurrentState();
  manageAudioPower(currentState);
  manageRadioPower(currentState);

  idleOrLightSleep();
}
