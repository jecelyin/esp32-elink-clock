#pragma once

#include "../drivers/DisplayDriver.h"
#include "../drivers/RtcDriver.h"
#include "../managers/ConnectionManager.h"
#include "../managers/WeatherManager.h"

#include "Screen.h"
class AlarmScreen;
class HomeScreen;
class MenuScreen;
class MusicScreen;
class RadioScreen;
class SettingsScreen;
class WeatherScreen;

#include "../drivers/AudioDriver.h"
#include "../drivers/RadioDriver.h"
#include "../managers/AlarmManager.h"
#include "../drivers/AudioDriver.h"
#include "../drivers/RadioDriver.h"
#include "../managers/AlarmManager.h"
#include "../managers/ConfigManager.h"
#include "../managers/TodoManager.h"
#include "../managers/WebManager.h"
#include "components/StatusBar.h"

enum ScreenState {
  SCREEN_HOME,
  SCREEN_MENU,
  SCREEN_ALARM,
  SCREEN_RADIO,
  SCREEN_MUSIC,
  SCREEN_WEATHER,
  SCREEN_SETTINGS
};

class UIManager {
public:
  UIManager(DisplayDriver *disp, RtcDriver *rtc, WeatherManager *weather,
            ConnectionManager *conn, AlarmManager *alarm, RadioDriver *radio,
            AudioDriver *audio, ConfigManager *config);
  void init();
  void update();
  void handleInput(int key); // 0: None, 1: Menu/Select, 2: Up, 3: Down
  void switchScreen(ScreenState state);

private:
  DisplayDriver *display;

  // Screens
  HomeScreen *homeScreen;
  MenuScreen *menuScreen;
  AlarmScreen *alarmScreen;
  RadioScreen *radioScreen;
  MusicScreen *musicScreen;
  WeatherScreen *weatherScreen;
  SettingsScreen *settingsScreen;

  Screen *currentScreenObj;

  // Dependencies for screens
  RtcDriver *rtc;
  WeatherManager *weather;
  ConnectionManager *conn;
  AlarmManager *alarmMgr;
  RadioDriver *radio;
  AudioDriver *audio;
  ConfigManager *config;
  StatusBar *statusBar;
  TodoManager *todoMgr;
  WebManager *webMgr;
};
