#include "UIManager.h"
#include "screens/AlarmScreen.h"
#include "screens/HomeScreen.h"
#include "screens/MenuScreen.h"
#include "screens/MusicScreen.h"
#include "screens/RadioScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/WeatherScreen.h"


UIManager::UIManager(DisplayDriver *disp, RtcDriver *rtc,
                     WeatherManager *weather, ConnectionManager *conn,
                     AlarmManager *alarm, RadioDriver *radio,
                     AudioDriver *audio, ConfigManager *config)
    : display(disp), rtc(rtc), weather(weather), conn(conn), alarmMgr(alarm),
      radio(radio), audio(audio), config(config) {

  statusBar = new StatusBar(conn, rtc);
  todoMgr = new TodoManager();

  
  webMgr = new WebManager(todoMgr);

  // Create Screens
  homeScreen = new HomeScreen(rtc, weather, statusBar, todoMgr);
  menuScreen = new MenuScreen(statusBar);
  alarmScreen = new AlarmScreen(alarmMgr, statusBar);
  radioScreen = new RadioScreen(radio, statusBar);
  musicScreen = new MusicScreen(audio, statusBar);
  weatherScreen = new WeatherScreen(weather, statusBar);
  settingsScreen = new SettingsScreen(config, statusBar);

  // Set UIManager reference
  homeScreen->setUIManager(this);
  menuScreen->setUIManager(this);
  alarmScreen->setUIManager(this);
  radioScreen->setUIManager(this);
  musicScreen->setUIManager(this);
  weatherScreen->setUIManager(this);
  settingsScreen->setUIManager(this);

  currentScreenObj = homeScreen;
}

void UIManager::init() {
  todoMgr->begin(); // Initialize TodoManager
  // webMgr->begin(); // Start Web Server

  if (currentScreenObj)
    currentScreenObj->init();
}

void UIManager::update() {
  // Refresh logic (e.g. every minute for clock)
  static unsigned long lastRefresh = 0;
  // Force refresh every minute or if requested (not impl)
  // For now, let's just redraw if needed.
  // E-Ink shouldn't refresh too often.

  if (millis() - lastRefresh > 60000 || lastRefresh == 0) {
    if (currentScreenObj)
      currentScreenObj->draw(display);
    lastRefresh = millis();
  }

  if (currentScreenObj)
    currentScreenObj->update();
    
  if (webMgr) webMgr->loop();
}

void UIManager::handleInput(int key) {
  if (currentScreenObj) {
    currentScreenObj->handleInput(key);
    // Redraw after input?
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
  }

  if (currentScreenObj) {
    currentScreenObj->enter();
    currentScreenObj->draw(display);
  }
}
