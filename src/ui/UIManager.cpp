#include "UIManager.h"
#include "screens/AlarmScreen.h"
#include "screens/HomeScreen.h"
#include "screens/MenuScreen.h"
#include "screens/MusicScreen.h"
#include "screens/RadioScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/TimerScreen.h"
#include "screens/WeatherScreen.h"
#include "../utils/WakeTiming.h"

UIManager::UIManager(DisplayDriver *disp, RtcDriver *rtc,
                     WeatherManager *weather, SensorDriver *sensor,
                     BatteryDriver *battery, ConnectionManager *conn,
                     AlarmManager *alarm, RadioDriver *radio,
                     AudioDriver *audio, MusicManager *music,
                     ConfigManager *config)
    : display(disp), rtc(rtc), weather(weather), sensor(sensor),
      battery(battery), conn(conn), alarmMgr(alarm), radio(radio), audio(audio),
      music(music), config(config) {

  statusBar = new StatusBar(conn, rtc, battery);
  todoMgr = new TodoManager();

  webMgr = new WebManager(todoMgr);

  // Create Screens
  homeScreen = new HomeScreen(rtc, weather, sensor, statusBar, todoMgr, conn);
  menuScreen = new MenuScreen(statusBar);
  alarmScreen = new AlarmScreen(alarmMgr, statusBar);
  radioScreen = new RadioScreen(radio, statusBar, config);
  musicScreen = new MusicScreen(music, statusBar, config);
  weatherScreen = new WeatherScreen(weather, statusBar);
  settingsScreen = new SettingsScreen(config, statusBar, battery, conn);
  timerScreen = new TimerScreen(statusBar);

  // Set UIManager reference
  homeScreen->setUIManager(this);
  menuScreen->setUIManager(this);
  alarmScreen->setUIManager(this);
  radioScreen->setUIManager(this);
  musicScreen->setUIManager(this);
  weatherScreen->setUIManager(this);
  settingsScreen->setUIManager(this);
  timerScreen->setUIManager(this);

  currentScreenObj = homeScreen;
  currentScreenState = SCREEN_HOME;
}

void UIManager::init() {
  todoMgr->begin(); // Initialize TodoManager
  // webMgr->begin(); // Start Web Server

  if (currentScreenObj)
    currentScreenObj->init();
}

void UIManager::update() {
  if (currentScreenObj)
    currentScreenObj->update();

  if (statusBar && display) {
    // 关键逻辑：首页顶部状态栏不显示时间，其余页面显示时间。
    // 这里统一在 UIManager 层补齐状态栏局刷，避免每个页面各自维护分钟刷新，
    // 从而修复“离开首页后顶部时间/联网状态不更新”的问题。
    bool showTimeInStatusBar = currentScreenState != SCREEN_HOME;
    if (statusBar->needsRefresh(showTimeInStatusBar)) {
      statusBar->refreshPartial(display, showTimeInStatusBar);
    }
  }

  if (webMgr)
    webMgr->loop();
}

void UIManager::handleInput(UIKey key) {
  if (key == UI_KEY_ENTER_LONG) {
    onLongPressEnter();
    return;
  }

  if (currentScreenObj) {
    if (currentScreenObj->handleInput(key)) {
      currentScreenObj->draw(display);
    }
  }
}

void UIManager::onLongPressEnter() {
  if (!currentScreenObj) {
    return;
  }

  if (currentScreenObj->onLongPress()) {
    currentScreenObj->draw(display);
  }
}

void UIManager::switchScreen(ScreenState state) {
  if (currentScreenObj)
    currentScreenObj->exit();

  switch (state) {
  case SCREEN_HOME:
    currentScreenObj = homeScreen;
    break;
  case SCREEN_MENU:
    currentScreenObj = menuScreen;
    break;
  case SCREEN_ALARM:
    currentScreenObj = alarmScreen;
    break;
  case SCREEN_RADIO:
    currentScreenObj = radioScreen;
    break;
  case SCREEN_MUSIC:
    currentScreenObj = musicScreen;
    break;
  case SCREEN_WEATHER:
    currentScreenObj = weatherScreen;
    break;
  case SCREEN_SETTINGS:
    currentScreenObj = settingsScreen;
    break;
  case SCREEN_TIMER:
    currentScreenObj = timerScreen;
    break;
  }

  currentScreenState = state;

  if (currentScreenObj) {
    currentScreenObj->enter();
    currentScreenObj->draw(display);
  }
}

uint32_t UIManager::getIdleSleepIntervalMs() const {
  if (currentScreenObj == nullptr) {
    return 30000UL;
  }

  uint32_t screenDelayMs = currentScreenObj->getIdleSleepIntervalMs();
  bool statusBarShowsTime = currentScreenState != SCREEN_HOME;
  if (!statusBarShowsTime || rtc == nullptr) {
    return screenDelayMs;
  }

  // 关键逻辑：非首页页面虽然大多没有自己的 update() 定时器，
  // 但顶部状态栏仍然要按分钟刷新时间。
  // 如果这里继续沿用 Screen 默认的 1 小时休眠周期，主循环就只会在按键
  // GPIO 唤醒时恢复执行，表现出来就是“只有按一下键，时间才跳一下”。
  uint32_t minuteDelayMs = WakeTiming::getMsUntilNextMinuteBoundary(
      rtc->getSoftwareTime(), millis());
  return minuteDelayMs < screenDelayMs ? minuteDelayMs : screenDelayMs;
}
