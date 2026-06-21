#include "LunarCalendar.h"

const uint32_t LunarCalendar::LUNAR_INFO[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554,
    0x056a0, 0x09ad0, 0x055d2, 0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250,
    0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977, 0x04970,
    0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570,
    0x052f2, 0x04970, 0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0,
    0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950, 0x0d4a0, 0x1d8a6,
    0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950,
    0x0b557, 0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5d0, 0x14573,
    0x052d0, 0x0a9a8, 0x0e950, 0x06aa0, 0x0aea6, 0x0ab50, 0x04b60,
    0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558,
    0x0b540, 0x0b6a0, 0x195a6, 0x095b0, 0x049b0, 0x0a974, 0x0a4b0,
    0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570, 0x04af5,
    0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60,
    0x096d5, 0x092e0, 0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552,
    0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5, 0x0a950, 0x0b4a0,
    0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0,
    0x0a930, 0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6,
    0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, 0x05aa0, 0x076a3, 0x096d0,
    0x04bd7, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50,
    0x1b255, 0x06d20, 0x0ada0, 0x14b63, 0x09370, 0x049f8, 0x04970,
    0x064b0, 0x168a6, 0x0ea50, 0x06aa0, 0x1a6c4, 0x0aae0, 0x0a2e0,
    0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0,
    0x0a6d0, 0x055d4, 0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6,
    0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0, 0x0b273, 0x06930,
    0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4,
    0x0d160, 0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0,
    0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252, 0x0d520};

const uint16_t LunarCalendar::LUNAR_YEAR_COUNT =
    sizeof(LunarCalendar::LUNAR_INFO) / sizeof(LunarCalendar::LUNAR_INFO[0]);

LunarDate LunarCalendar::fromSolar(uint16_t year, uint8_t month, uint8_t day) {
  LunarDate result;
  if (!isSupportedYear(year) || month < 1 || month > 12)
    return result;
  if (day < 1 || day > getGregorianMonthDays(year, month))
    return result;

  int32_t offset = getSolarOffset(year, month, day);
  if (offset < 0)
    return result;

  // 关键逻辑：农历年表以 1900-01-31（农历正月初一）为零点。
  // 先用公历日期换算出距零点的天数，再逐年、逐月扣减，得到农历月日。
  uint16_t lunarYear = BASE_YEAR;
  while (lunarYear < BASE_YEAR + LUNAR_YEAR_COUNT) {
    uint16_t yearDays = getLunarYearDays(lunarYear);
    if (offset < yearDays)
      break;
    offset -= yearDays;
    lunarYear++;
  }

  if (!isSupportedYear(lunarYear))
    return result;

  result.year = lunarYear;
  result.valid = true;
  uint8_t leapMonth = getLeapMonth(lunarYear);
  for (uint8_t lunarMonth = 1; lunarMonth <= 12; ++lunarMonth) {
    uint8_t monthDays = getLunarMonthDays(lunarYear, lunarMonth);
    if (offset < monthDays) {
      result.month = lunarMonth;
      result.day = offset + 1;
      return result;
    }
    offset -= monthDays;

    if (leapMonth != lunarMonth)
      continue;

    // 关键逻辑：闰月排在同名农历月之后，例如“闰二月”跟在二月后面。
    // 因此必须先扣普通月，再判断同月是否存在闰月。
    uint8_t leapDays = getLeapMonthDays(lunarYear);
    if (offset < leapDays) {
      result.month = lunarMonth;
      result.day = offset + 1;
      result.isLeapMonth = true;
      return result;
    }
    offset -= leapDays;
  }
  result.valid = false;
  return result;
}

String LunarCalendar::getDayLabel(uint16_t year, uint8_t month, uint8_t day) {
  LunarDate lunar = fromSolar(year, month, day);
  if (!lunar.valid)
    return "";

  const char *festival = getLunarFestival(lunar);
  if (festival[0] != '\0')
    return String(festival);
  String solarFestival = getSolarFestivalLabel(year, month, day);
  if (solarFestival.length() > 0)
    return solarFestival;
  if (lunar.day != 1)
    return String(getLunarDayName(lunar.day));

  String label = lunar.isLeapMonth ? "闰" : "";
  label += getLunarMonthName(lunar.month);
  label += "月";
  return label;
}

String LunarCalendar::getFullDateLabel(uint16_t year, uint8_t month,
                                       uint8_t day) {
  LunarDate lunar = fromSolar(year, month, day);
  if (!lunar.valid)
    return "农历--";

  String label = "农历";
  if (lunar.isLeapMonth)
    label += "闰";
  label += getLunarMonthName(lunar.month);
  label += "月";
  label += getLunarDayName(lunar.day);
  return label;
}

LunarFestivalCountdown
LunarCalendar::getNextFestivalCountdown(uint16_t year, uint8_t month,
                                        uint8_t day) {
  LunarFestivalCountdown countdown;
  if (!isSupportedYear(year))
    return countdown;

  uint16_t cursorYear = year;
  uint8_t cursorMonth = month;
  uint8_t cursorDay = day;
  for (uint16_t offset = 0; offset <= 380; ++offset) {
    LunarDate lunar = fromSolar(cursorYear, cursorMonth, cursorDay);
    String name =
        getCountdownFestival(lunar, cursorYear, cursorMonth, cursorDay);
    if (name.length() > 0) {
      countdown.name = name;
      countdown.days = offset;
      countdown.valid = true;
      return countdown;
    }
    advanceSolarDate(cursorYear, cursorMonth, cursorDay);
  }
  return countdown;
}

String LunarCalendar::getSolarFestivalLabel(uint16_t year, uint8_t month,
                                            uint8_t day) {
  struct SolarFestivalRule {
    uint16_t key;
    const char *name;
  };
  static const SolarFestivalRule rules[] = {
      {101, "元旦"},     {214, "情人节"}, {308, "妇女节"},
      {312, "植树节"},   {401, "愚人节"}, {504, "青年节"},
      {512, "护士节"},   {601, "儿童节"}, {701, "建党节"},
      {801, "建军节"},   {910, "教师节"}, {1001, "国庆节"},
      {1224, "平安夜"},  {1225, "圣诞节"}};

  if (month == 5 && isNthWeekdayOfMonth(year, month, day, 0, 2))
    return "母亲节";
  if (month == 6 && isNthWeekdayOfMonth(year, month, day, 0, 3))
    return "父亲节";

  uint16_t key = static_cast<uint16_t>(month) * 100 + day;
  for (const SolarFestivalRule &rule : rules) {
    if (rule.key == key)
      return rule.name;
  }
  return "";
}

String LunarCalendar::getYearLabel(uint16_t year, uint8_t month, uint8_t day) {
  LunarDate lunar = fromSolar(year, month, day);
  if (!lunar.valid)
    return "--年 --";

  static const char *stems[] = {"甲", "乙", "丙", "丁", "戊",
                                "己", "庚", "辛", "壬", "癸"};
  static const char *branches[] = {"子", "丑", "寅", "卯", "辰", "巳",
                                   "午", "未", "申", "酉", "戌", "亥"};
  static const char *zodiac[] = {"鼠", "牛", "虎", "兔", "龙", "蛇",
                                 "马", "羊", "猴", "鸡", "狗", "猪"};
  uint8_t stemIndex = (lunar.year - 4) % 10;
  uint8_t branchIndex = (lunar.year - 4) % 12;
  return String(stems[stemIndex]) + branches[branchIndex] + "年 " +
         zodiac[branchIndex];
}

bool LunarCalendar::isGregorianLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

bool LunarCalendar::isSupportedYear(uint16_t year) {
  return year >= BASE_YEAR && year < BASE_YEAR + LUNAR_YEAR_COUNT;
}

int32_t LunarCalendar::getSolarOffset(uint16_t year, uint8_t month,
                                      uint8_t day) {
  // 关键逻辑：这里返回的是目标公历日相对 1900-01-31 的零基偏移。
  // 例如 1900-01-31 返回 0，对应农历正月初一。
  int32_t offset = 0;
  for (uint16_t currentYear = BASE_YEAR; currentYear < year; ++currentYear) {
    offset += isGregorianLeapYear(currentYear) ? 366 : 365;
  }
  for (uint8_t currentMonth = 1; currentMonth < month; ++currentMonth) {
    offset += getGregorianMonthDays(year, currentMonth);
  }
  offset += day - 1;
  offset -= BASE_DAY - 1;
  return offset;
}

uint8_t LunarCalendar::getGregorianMonthDays(uint16_t year, uint8_t month) {
  static const uint8_t days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
  if (month == 2 && isGregorianLeapYear(year))
    return 29;
  return days[month - 1];
}

uint8_t LunarCalendar::getLeapMonth(uint16_t lunarYear) {
  return LUNAR_INFO[lunarYear - BASE_YEAR] & 0x0F;
}

uint8_t LunarCalendar::getLeapMonthDays(uint16_t lunarYear) {
  if (getLeapMonth(lunarYear) == 0)
    return 0;
  return (LUNAR_INFO[lunarYear - BASE_YEAR] & 0x10000) ? 30 : 29;
}

uint8_t LunarCalendar::getLunarMonthDays(uint16_t lunarYear,
                                         uint8_t lunarMonth) {
  uint32_t flag = 0x10000 >> lunarMonth;
  return (LUNAR_INFO[lunarYear - BASE_YEAR] & flag) ? 30 : 29;
}

uint16_t LunarCalendar::getLunarYearDays(uint16_t lunarYear) {
  uint16_t days = 348;
  uint32_t info = LUNAR_INFO[lunarYear - BASE_YEAR];
  for (uint32_t flag = 0x8000; flag > 0x8; flag >>= 1) {
    if (info & flag)
      days++;
  }
  return days + getLeapMonthDays(lunarYear);
}

void LunarCalendar::advanceSolarDate(uint16_t &year, uint8_t &month,
                                     uint8_t &day) {
  day++;
  if (day <= getGregorianMonthDays(year, month))
    return;

  day = 1;
  month++;
  if (month <= 12)
    return;

  month = 1;
  year++;
}

String LunarCalendar::getCountdownFestival(const LunarDate &date,
                                           uint16_t solarYear,
                                           uint8_t solarMonth,
                                           uint8_t solarDay) {
  // 关键逻辑：倒计日只选用户最可能关心的节日，避免清明、小满等节气抢占春节。
  const char *lunarFestival = getLunarFestival(date);
  if (lunarFestival[0] != '\0')
    return String(lunarFestival);
  return getSolarFestivalLabel(solarYear, solarMonth, solarDay);
}

uint8_t LunarCalendar::getGregorianWeekday(uint16_t year, uint8_t month,
                                           uint8_t day) {
  if (month < 3) {
    month += 12;
    year--;
  }

  // 关键逻辑：蔡勒公式返回 0=周六、1=周日，这里转换为项目统一的 0=周日。
  uint16_t century = year / 100;
  uint16_t yearOfCentury = year % 100;
  uint8_t zeller = (day + 13 * (month + 1) / 5 + yearOfCentury +
                    yearOfCentury / 4 + century / 4 + 5 * century) %
                   7;
  return (zeller + 6) % 7;
}

const char *LunarCalendar::getLunarDayName(uint8_t lunarDay) {
  static const char *names[] = {
      "",     "初一", "初二", "初三", "初四", "初五", "初六", "初七",
      "初八", "初九", "初十", "十一", "十二", "十三", "十四", "十五",
      "十六", "十七", "十八", "十九", "二十", "廿一", "廿二", "廿三",
      "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};
  if (lunarDay > 30)
    return "";
  return names[lunarDay];
}

const char *LunarCalendar::getLunarFestival(const LunarDate &date) {
  if (date.isLeapMonth)
    return "";
  if (date.month == 1 && date.day == 1)
    return "春节";
  if (date.month == 1 && date.day == 15)
    return "元宵节";
  if (date.month == 5 && date.day == 5)
    return "端午节";
  if (date.month == 7 && date.day == 7)
    return "七夕节";
  if (date.month == 8 && date.day == 15)
    return "中秋节";
  if (date.month == 9 && date.day == 9)
    return "重阳节";
  if (date.month == 12 && date.day == 8)
    return "腊八节";
  if (date.month == 12 && date.day == getLunarMonthDays(date.year, 12))
    return "除夕";
  return "";
}

const char *LunarCalendar::getLunarMonthName(uint8_t lunarMonth) {
  static const char *names[] = {"",   "正", "二", "三", "四", "五", "六",
                                "七", "八", "九", "十", "冬", "腊"};
  if (lunarMonth > 12)
    return "";
  return names[lunarMonth];
}

bool LunarCalendar::isNthWeekdayOfMonth(uint16_t year, uint8_t month,
                                        uint8_t day, uint8_t weekday,
                                        uint8_t nth) {
  if (getGregorianWeekday(year, month, day) != weekday)
    return false;
  return static_cast<uint8_t>((day - 1) / 7 + 1) == nth;
}
