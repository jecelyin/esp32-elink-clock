#include "CalendarScreen.h"
#include "../../utils/LunarCalendar.h"
#include "../../utils/qweather_fonts.h"
#include "../UIManager.h"

namespace {
LunarFestivalCountdown mergeCountdown(const HolidayCountdown &official,
                                      const LunarFestivalCountdown &local) {
  if (!official.valid)
    return local;
  if (local.valid && local.days < official.days)
    return local;

  // 关键逻辑：倒计时按“最近日期”选择；若 API 与本地规则同一天命中，
  // 使用 API 名称，避免本地简称覆盖官方节假日名称。
  LunarFestivalCountdown result;
  result.name = official.name;
  result.days = official.days;
  result.valid = true;
  return result;
}
} // namespace

CalendarScreen::CalendarScreen(RtcDriver *rtc, StatusBar *statusBar,
                               AlarmManager *alarmMgr, WeatherManager *weather)
    : rtc(rtc), statusBar(statusBar), alarmMgr(alarmMgr), weather(weather) {}

void CalendarScreen::enter() {
  syncViewedMonthToToday();
  lastRenderedDateKey = 0;
}

void CalendarScreen::update() {
  if (!uiManager || lastRenderedDateKey == 0)
    return;

  // 关键逻辑：跨日后公历、农历、倒计日和今日框都会变化。
  DateParts today = getToday();
  if (lastRenderedDateKey == getDateKey(today))
    return;

  syncViewedMonthToToday();
  draw(uiManager->getDisplayDriver());
}

void CalendarScreen::draw(DisplayDriver *displayDrv) {
  if (!displayDrv)
    return;

  DateParts today = getToday();
  primeHolidayCache();
  displayDrv->display.setFullWindow();
  displayDrv->display.firstPage();
  do {
    drawPage(displayDrv, today);
  } while (displayDrv->display.nextPage());
  displayDrv->powerOff();
  lastRenderedDateKey = getDateKey(today);
}

bool CalendarScreen::handleInput(UIKey key) {
  if (key == UI_KEY_LEFT) {
    shiftViewedMonth(-1);
    return true;
  }
  if (key == UI_KEY_RIGHT) {
    shiftViewedMonth(1);
    return true;
  }
  if (key == UI_KEY_ENTER) {
    syncViewedMonthToToday();
    return true;
  }
  return false;
}

void CalendarScreen::drawPage(DisplayDriver *displayDrv,
                              const DateParts &today) {
  displayDrv->display.fillScreen(GxEPD_WHITE);
  statusBar->draw(displayDrv, true);
  drawTopPanel(displayDrv, today);
  drawWeekHeader(displayDrv);
  drawCalendarGrid(displayDrv, today);
}

void CalendarScreen::drawTopPanel(DisplayDriver *displayDrv,
                                  const DateParts &today) {
  drawDateBlock(displayDrv, today);
  drawLunarBlock(displayDrv, today);
  drawCountdown(displayDrv, today);
  drawWeatherPanel(displayDrv);
  displayDrv->display.drawLine(0, HEADER_Y, SCREEN_W, HEADER_Y, GxEPD_BLACK);
}

void CalendarScreen::drawDateBlock(DisplayDriver *displayDrv,
                                   const DateParts &today) {
  auto &u8g2 = displayDrv->u8g2Fonts;
  u8g2.setForegroundColor(GxEPD_BLACK);
  u8g2.setBackgroundColor(GxEPD_WHITE);
  u8g2.setFont(u8g2_font_fub25_tn);
  u8g2.setCursor(10, TOP_Y + TOP_H - 28);
  u8g2.print(today.year);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.print("年");
  u8g2.setFont(u8g2_font_fub25_tn);
  u8g2.print(today.month);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.print("月");
  u8g2.setFont(u8g2_font_fub25_tn);
  u8g2.print(today.day);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.print("日");

  char weekLine[12];
  snprintf(weekLine, sizeof(weekLine), "第%02d周", getIsoWeek(today));
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  u8g2.setCursor(10, TOP_Y + TOP_H - 5);
  u8g2.print(weekLine);
}

void CalendarScreen::drawLunarBlock(DisplayDriver *displayDrv,
                                    const DateParts &today) {
  auto &u8g2 = displayDrv->u8g2Fonts;
  String yearLabel = LunarCalendar::getYearLabel(today.year, today.month,
                                                 today.day);
  String dateLabel = LunarCalendar::getFullDateLabel(today.year, today.month,
                                                     today.day);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(196, TOP_Y + 16);
  u8g2.print(yearLabel);
  u8g2.setCursor(196, TOP_Y + 34);
  u8g2.print(dateLabel);
}

void CalendarScreen::drawCountdown(DisplayDriver *displayDrv,
                                   const DateParts &today) {
  LunarFestivalCountdown local = LunarCalendar::getNextFestivalCountdown(
      today.year, today.month, today.day);
  HolidayCountdown official;
  DateTime todayDate = {0, 0, 0, today.day, today.month,
                        static_cast<uint8_t>(today.year - 2000), today.week};
  if (alarmMgr)
    official = alarmMgr->getNextHolidayCountdown(todayDate, 380);
  LunarFestivalCountdown countdown = mergeCountdown(official, local);
  if (!countdown.valid)
    return;

  auto &u8g2 = displayDrv->u8g2Fonts;
  bool isTodayFestival = countdown.days == 0;
  String prefix = isTodayFestival ? "今天是" : "距 ";
  String middle = " 还有 ";
  String suffix = " 天";
  char days[6];
  snprintf(days, sizeof(days), "%u", countdown.days);

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  int width = u8g2.getUTF8Width(prefix.c_str()) +
              u8g2.getUTF8Width(countdown.name.c_str());
  if (!isTodayFestival) {
    width += u8g2.getUTF8Width(middle.c_str()) +
             u8g2.getUTF8Width(suffix.c_str());
    u8g2.setFont(u8g2_font_fub14_tn);
    width += u8g2.getUTF8Width(days);
  }

  int beginX = 76;
  int endX = WEATHER_X;
  u8g2.setCursor(beginX + max(0, (endX - beginX - width) / 2),
                 TOP_Y + TOP_H - 5);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.print(prefix);
  u8g2.print(countdown.name);
  if (isTodayFestival)
    return;
  u8g2.print(middle);
  u8g2.setFont(u8g2_font_fub14_tn);
  u8g2.print(days);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.print(suffix);
}

void CalendarScreen::drawWeatherPanel(DisplayDriver *displayDrv) {
  if (!weather)
    return;

  auto &u8g2 = displayDrv->u8g2Fonts;
  u8g2.setFont(u8g2_font_qweather_icon_16);
  u8g2.drawUTF8(WEATHER_X, TOP_Y + 44, weather->data.icon_str);

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  String lineOne = buildWeatherLineOne(weather->data);
  String lineTwo = buildWeatherLineTwo(weather->data);
  u8g2.setCursor(WEATHER_X + 28, TOP_Y + 22);
  u8g2.print(lineOne);
  u8g2.setCursor(WEATHER_X + 28, TOP_Y + 38);
  u8g2.print(lineTwo);
}

void CalendarScreen::drawWeekHeader(DisplayDriver *displayDrv) {
  for (uint8_t col = 0; col < 7; ++col) {
    drawWeekHeaderCell(displayDrv, col);
  }
  drawWeekHeaderBorder(displayDrv);
}

void CalendarScreen::drawWeekHeaderCell(DisplayDriver *displayDrv,
                                        uint8_t col) {
  auto &display = displayDrv->display;
  auto &u8g2 = displayDrv->u8g2Fonts;
  bool weekend = col == 0 || col == 6;
  int x = GRID_X + col * CELL_W;
  uint16_t bg = weekend ? GxEPD_BLACK : GxEPD_WHITE;
  uint16_t fg = weekend ? GxEPD_WHITE : GxEPD_BLACK;
  display.fillRect(x, HEADER_Y, CELL_W, HEADER_H, bg);
  u8g2.setForegroundColor(fg);
  u8g2.setBackgroundColor(bg);
  u8g2.setFont(u8g2_font_wqy16_t_gb2312);
  centerText(displayDrv, getWeekdayText(col), x, HEADER_Y + 15, CELL_W);
  restoreTextColors(displayDrv);
}

void CalendarScreen::drawWeekHeaderBorder(DisplayDriver *displayDrv) {
  auto &display = displayDrv->display;
  int beginX = GRID_X;
  int endX = GRID_X + CELL_W * 7;
  // 星期栏只保留整行上下边框，避免恢复日期格子的竖向分隔线。
  display.drawLine(beginX, HEADER_Y, endX, HEADER_Y, GxEPD_BLACK);
  display.drawLine(beginX, GRID_Y, endX, GRID_Y, GxEPD_BLACK);
}

void CalendarScreen::drawCalendarGrid(DisplayDriver *displayDrv,
                                      const DateParts &today) {
  uint8_t totalDays = getDaysInMonth(viewedYear, viewedMonth);
  uint8_t firstWeekday = getWeekday(viewedYear, viewedMonth, 1);
  uint8_t rowCount = getCalendarRowCount(totalDays, firstWeekday);
  uint8_t cellHeight = getCalendarCellHeight(rowCount);

  for (uint8_t day = 1; day <= totalDays; ++day) {
    drawDayCell(displayDrv, today, day, firstWeekday, cellHeight);
  }
}

void CalendarScreen::drawDayCell(DisplayDriver *displayDrv,
                                 const DateParts &today, uint8_t day,
                                 uint8_t firstWeekday, uint8_t cellHeight) {
  int x, y;
  getDayCellOrigin(day, firstWeekday, cellHeight, x, y);
  HolidayDayType dayType = getHolidayDayType(day, firstWeekday);
  drawDayNumber(displayDrv, day, x, y, cellHeight);
  drawDayLabel(displayDrv, day, x, y, cellHeight);
  drawHolidayMarker(displayDrv, dayType, x, y);
  if (isViewedDay(today, viewedYear, viewedMonth, day))
    drawTodayFrame(displayDrv, x, y, cellHeight);
}

void CalendarScreen::drawDayNumber(DisplayDriver *displayDrv, uint8_t day,
                                   int x, int y, uint8_t cellHeight) {
  auto &u8g2 = displayDrv->u8g2Fonts;
  char number[3];
  snprintf(number, sizeof(number), "%u", day);
  u8g2.setFont(u8g2_font_fub17_tn);
  int baselineY = y + cellHeight / 2 + 3;
  u8g2.setCursor(x + (CELL_W - u8g2.getUTF8Width(number)) / 2, baselineY);
  u8g2.print(number);
}

void CalendarScreen::drawDayLabel(DisplayDriver *displayDrv, uint8_t day,
                                  int x, int y, uint8_t cellHeight) {
  String label = getDayLabel(day);
  if (label.length() == 0)
    return;

  displayDrv->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  centerText(displayDrv, label.c_str(), x, y + cellHeight - 4, CELL_W);
}

void CalendarScreen::drawTodayFrame(DisplayDriver *displayDrv, int x, int y,
                                    uint8_t cellHeight) {
  auto &display = displayDrv->display;
  // 关键逻辑：用双线框替代参考三色屏红框，保留“今天”的视觉锚点。
  display.drawRoundRect(x + 1, y + 1, CELL_W - 2, cellHeight - 2, 4,
                        GxEPD_BLACK);
  display.drawRoundRect(x + 2, y + 2, CELL_W - 4, cellHeight - 4, 3,
                        GxEPD_BLACK);
}

void CalendarScreen::drawHolidayMarker(DisplayDriver *displayDrv,
                                       HolidayDayType type, int x, int y) {
  if (type == HOLIDAY_DAY_OFFDAY) {
    drawFilledMarker(displayDrv, "休", x + 41, y + 2);
    return;
  }
  if (type == HOLIDAY_DAY_MAKEUP_WORKDAY)
    drawOutlineMarker(displayDrv, "班", x + 41, y + 2);
}

void CalendarScreen::drawFilledMarker(DisplayDriver *displayDrv,
                                      const char *text, int x, int y) {
  auto &u8g2 = displayDrv->u8g2Fonts;
  displayDrv->display.fillRoundRect(x, y, 14, 14, 2, GxEPD_BLACK);
  u8g2.setForegroundColor(GxEPD_WHITE);
  u8g2.setBackgroundColor(GxEPD_BLACK);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(x + 1, y + 12, text);
  restoreTextColors(displayDrv);
}

void CalendarScreen::drawOutlineMarker(DisplayDriver *displayDrv,
                                       const char *text, int x, int y) {
  displayDrv->display.drawRoundRect(x, y, 14, 14, 2, GxEPD_BLACK);
  displayDrv->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  displayDrv->u8g2Fonts.drawUTF8(x + 1, y + 12, text);
}

void CalendarScreen::centerText(DisplayDriver *displayDrv, const char *text,
                                int x, int baselineY, int width) {
  auto &u8g2 = displayDrv->u8g2Fonts;
  int textWidth = u8g2.getUTF8Width(text);
  u8g2.drawUTF8(x + (width - textWidth) / 2, baselineY, text);
}

void CalendarScreen::restoreTextColors(DisplayDriver *displayDrv) {
  displayDrv->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  displayDrv->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void CalendarScreen::getDayCellOrigin(uint8_t day, uint8_t firstWeekday,
                                      uint8_t cellHeight, int &x, int &y) {
  uint8_t slot = firstWeekday + day - 1;
  x = GRID_X + (slot % 7) * CELL_W;
  y = GRID_Y + (slot / 7) * cellHeight;
}

void CalendarScreen::syncViewedMonthToToday() {
  DateParts today = getToday();
  viewedYear = today.year;
  viewedMonth = today.month;
}

void CalendarScreen::primeHolidayCache() {
  if (!alarmMgr)
    return;

  uint8_t firstWeekday = getWeekday(viewedYear, viewedMonth, 1);
  alarmMgr->getHolidayDayType(buildViewedDate(1, firstWeekday));
}

HolidayDayType CalendarScreen::getHolidayDayType(uint8_t day,
                                                 uint8_t firstWeekday) {
  DateTime date = buildViewedDate(day, firstWeekday);
  if (!alarmMgr)
    return isWeekendDate(date) ? HOLIDAY_DAY_WEEKEND : HOLIDAY_DAY_WORKDAY;
  return alarmMgr->getHolidayDayType(date);
}

String CalendarScreen::getDayLabel(uint8_t day) {
  uint8_t firstWeekday = getWeekday(viewedYear, viewedMonth, 1);
  DateTime date = buildViewedDate(day, firstWeekday);
  if (alarmMgr) {
    String officialLabel = alarmMgr->getHolidayName(date);
    if (officialLabel.length() > 0)
      return officialLabel;
  }
  return LunarCalendar::getDayLabel(viewedYear, viewedMonth, day);
}

DateTime CalendarScreen::buildViewedDate(uint8_t day,
                                         uint8_t firstWeekday) const {
  DateTime date = {0, 0, 0, day, viewedMonth,
                   static_cast<uint8_t>(viewedYear - 2000),
                   static_cast<uint8_t>((firstWeekday + day - 1) % 7)};
  return date;
}

void CalendarScreen::shiftViewedMonth(int delta) {
  int nextMonth = static_cast<int>(viewedMonth) + delta;
  if (nextMonth < 1) {
    viewedYear--;
    nextMonth = 12;
  }
  if (nextMonth > 12) {
    viewedYear++;
    nextMonth = 1;
  }
  viewedMonth = static_cast<uint8_t>(nextMonth);
}

CalendarScreen::DateParts CalendarScreen::getToday() const {
  DateTime now = rtc->getTime();
  return DateParts{static_cast<uint16_t>(now.year + 2000), now.month, now.day,
                   now.week};
}

String CalendarScreen::buildWeatherLineOne(const WeatherData &data) {
  String line = data.weather.length() == 0 ? "--" : data.weather;
  line += " ";
  line += data.humidity;
  line += "%";
  return line;
}

String CalendarScreen::buildWeatherLineTwo(const WeatherData &data) {
  String line = String(data.temp);
  line += "°C";
  if (!data.daily.empty()) {
    line += " ";
    line += data.daily[0].temp_min;
    line += "-";
    line += data.daily[0].temp_max;
    line += "°";
  }
  return line;
}

bool CalendarScreen::isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool CalendarScreen::isViewedDay(const DateParts &today, uint16_t viewedYear,
                                 uint8_t viewedMonth, uint8_t day) {
  return today.year == viewedYear && today.month == viewedMonth &&
         today.day == day;
}

bool CalendarScreen::isWeekendDate(const DateTime &date) {
  return date.week == 0 || date.week == 6;
}

const char *CalendarScreen::getWeekdayText(uint8_t week) {
  static const char *texts[] = {"日", "一", "二", "三", "四", "五", "六"};
  return texts[week % 7];
}

uint32_t CalendarScreen::getDateKey(const DateParts &date) {
  return static_cast<uint32_t>(date.year) * 10000UL +
         static_cast<uint32_t>(date.month) * 100UL + date.day;
}

uint8_t CalendarScreen::getCalendarCellHeight(uint8_t rowCount) {
  uint8_t safeRows = rowCount == 0 ? 1 : rowCount;
  return (SCREEN_H - GRID_Y) / safeRows;
}

uint8_t CalendarScreen::getCalendarRowCount(uint8_t totalDays,
                                            uint8_t firstWeekday) {
  // 关键逻辑：只画当前月份真正需要的周数，避免 5 周月份底部多出空白。
  return (firstWeekday + totalDays + 6) / 7;
}

uint8_t CalendarScreen::getDaysInMonth(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year))
    return 29;
  return days[month - 1];
}

uint8_t CalendarScreen::getIsoWeek(const DateParts &date) {
  uint8_t weekdayMonFirst = date.week == 0 ? 7 : date.week;
  int week = (getDayOfYear(date) - weekdayMonFirst + 10) / 7;
  if (week < 1)
    return getIsoWeeksInYear(date.year - 1);
  if (week > getIsoWeeksInYear(date.year))
    return 1;
  return static_cast<uint8_t>(week);
}

uint8_t CalendarScreen::getIsoWeeksInYear(uint16_t year) {
  uint8_t janFirst = getWeekday(year, 1, 1);
  bool startsThursday = janFirst == 4;
  bool leapStartsWednesday = janFirst == 3 && isLeapYear(year);
  return startsThursday || leapStartsWednesday ? 53 : 52;
}

uint8_t CalendarScreen::getWeekday(uint16_t year, uint8_t month, uint8_t day) {
  static const uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3)
    year--;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] +
          day) %
         7;
}

uint16_t CalendarScreen::getDayOfYear(const DateParts &date) {
  uint16_t dayOfYear = date.day;
  for (uint8_t month = 1; month < date.month; ++month) {
    dayOfYear += getDaysInMonth(date.year, month);
  }
  return dayOfYear;
}
