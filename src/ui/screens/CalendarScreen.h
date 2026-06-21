#pragma once

#include "../../drivers/RtcDriver.h"
#include "../../managers/AlarmManager.h"
#include "../../managers/WeatherManager.h"
#include "../Screen.h"
#include "../components/StatusBar.h"
#include <Arduino.h>

class CalendarScreen : public Screen {
public:
  CalendarScreen(RtcDriver *rtc, StatusBar *statusBar, AlarmManager *alarmMgr,
                 WeatherManager *weather);

  void enter() override;
  void update() override;
  void draw(DisplayDriver *displayDrv) override;
  bool onInput(UIKey key) override;

private:
  struct DateParts {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t week;
  };

  static const int SCREEN_W = 400;
  static const int SCREEN_H = 300;
  static const int STATUS_H = 24;
  static const int TOP_Y = STATUS_H;
  static const int TOP_H = 60;
  static const int HEADER_H = 20;
  static const int CELL_W = 56;
  static const int WEATHER_X = 300;
  static const int GRID_X = 4;
  static const int HEADER_Y = TOP_Y + TOP_H;
  static const int GRID_Y = HEADER_Y + HEADER_H;

  RtcDriver *rtc;
  StatusBar *statusBar;
  AlarmManager *alarmMgr;
  WeatherManager *weather;
  uint16_t viewedYear = 2000;
  uint8_t viewedMonth = 1;
  uint32_t lastRenderedDateKey = 0;

  void centerText(DisplayDriver *displayDrv, const char *text, int x,
                  int baselineY, int width);
  DateTime buildViewedDate(uint8_t day, uint8_t firstWeekday) const;
  void drawCalendarGrid(DisplayDriver *displayDrv, const DateParts &today);
  void drawCountdown(DisplayDriver *displayDrv, const DateParts &today);
  void drawDateBlock(DisplayDriver *displayDrv, const DateParts &today);
  void drawDayCell(DisplayDriver *displayDrv, const DateParts &today,
                   uint8_t day, uint8_t firstWeekday, uint8_t cellHeight);
  void drawDayLabel(DisplayDriver *displayDrv, uint8_t day, int x, int y,
                    uint8_t cellHeight);
  void drawDayNumber(DisplayDriver *displayDrv, uint8_t day, int x, int y,
                     uint8_t cellHeight);
  void drawFilledMarker(DisplayDriver *displayDrv, const char *text, int x,
                        int y);
  void drawHolidayMarker(DisplayDriver *displayDrv, HolidayDayType type, int x,
                         int y);
  void drawLunarBlock(DisplayDriver *displayDrv, const DateParts &today);
  void drawOutlineMarker(DisplayDriver *displayDrv, const char *text, int x,
                         int y);
  void drawPage(DisplayDriver *displayDrv, const DateParts &today);
  void drawTodayFrame(DisplayDriver *displayDrv, int x, int y,
                      uint8_t cellHeight);
  void drawTopPanel(DisplayDriver *displayDrv, const DateParts &today);
  void drawWeatherPanel(DisplayDriver *displayDrv);
  void drawWeekHeader(DisplayDriver *displayDrv);
  void drawWeekHeaderBorder(DisplayDriver *displayDrv);
  void drawWeekHeaderCell(DisplayDriver *displayDrv, uint8_t col);
  void getDayCellOrigin(uint8_t day, uint8_t firstWeekday, uint8_t cellHeight,
                        int &x, int &y);
  DateParts getToday() const;
  HolidayDayType getHolidayDayType(uint8_t day, uint8_t firstWeekday);
  String getDayLabel(uint8_t day);
  void primeHolidayCache();
  void restoreTextColors(DisplayDriver *displayDrv);
  void shiftViewedMonth(int delta);
  void syncViewedMonthToToday();

  static String buildWeatherLineOne(const WeatherData &data);
  static String buildWeatherLineTwo(const WeatherData &data);
  static bool isLeapYear(uint16_t year);
  static bool isViewedDay(const DateParts &today, uint16_t viewedYear,
                          uint8_t viewedMonth, uint8_t day);
  static bool isWeekendDate(const DateTime &date);
  static const char *getWeekdayText(uint8_t week);
  static uint32_t getDateKey(const DateParts &date);
  static uint8_t getCalendarCellHeight(uint8_t rowCount);
  static uint8_t getCalendarRowCount(uint8_t totalDays, uint8_t firstWeekday);
  static uint8_t getDaysInMonth(uint16_t year, uint8_t month);
  static uint8_t getIsoWeek(const DateParts &date);
  static uint8_t getIsoWeeksInYear(uint16_t year);
  static uint8_t getWeekday(uint16_t year, uint8_t month, uint8_t day);
  static uint16_t getDayOfYear(const DateParts &date);
};
