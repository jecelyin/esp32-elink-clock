#pragma once

#include "../../drivers/DisplayDriver.h"
#include "../../drivers/RtcDriver.h"
#include "../../managers/TodoManager.h"
#include "../../managers/WeatherManager.h"
#include "../../utils/LunarCalendar.h"
#include "../../utils/qweather_fonts.h"

// 首页底部共用全刷和局刷入口，确保任务与预报切换时使用相同布局。
class HomeBottomSection {
public:
  static void draw(DisplayDriver *drv, const DateTime &now,
                   const std::vector<TodoItem> &tasks, const WeatherData &data) {
    if (!tasks.empty()) {
      drawTasks(drv, tasks);
      return;
    }
    auto &font = drv->u8g2Fonts;
    font.setFont(u8g2_font_wqy12_t_gb2312);
    font.setCursor(10, 215);
    font.print("天气预报");
    String lunar = LunarCalendar::getFullDateLabel(2000 + now.year, now.month, now.day);
    font.setCursor(390 - font.getUTF8Width(lunar.c_str()), 215);
    font.print(lunar);
    drawDaily(drv, now, data);
    drv->display.drawLine(0, 260, 400, 260, GxEPD_BLACK);
    drawHourly(drv, now, data);
  }

  // 独立比较预报内容：即使整批请求部分失败，成功返回的预报也必须刷新。
  static String snapshot(const WeatherData &data) {
    String value;
    for (const auto &day : data.daily) {
      value += day.date + ":" + String(day.temp_min) + ":" +
               String(day.temp_max) + ":" + String(day.icon_code) + ";";
    }
    for (const auto &hour : data.hourly) {
      value += hour.dateTime + ":" + String(hour.temp) + ":" +
               String(hour.icon_code) + ";";
    }
    return value;
  }

private:
  static String dateKey(const DateTime &now) {
    char value[11];
    snprintf(value, sizeof(value), "%04u-%02u-%02u", 2000 + now.year,
             now.month, now.day);
    return String(value);
  }

  static void drawIcon(DisplayDriver *drv, int x, int y, const char *icon) {
    auto &font = drv->u8g2Fonts;
    font.setFont(u8g2_font_qweather_icon_16);
    if (icon != nullptr)
      font.drawUTF8(x, y, icon);
    font.setFont(u8g2_font_wqy12_t_gb2312);
  }

  static void drawDaily(DisplayDriver *drv, const DateTime &now,
                        const WeatherData &data) {
    String today = dateKey(now);
    int count = 0;
    // 按完整日期排除今天和旧缓存，跨月、跨年仍保持正确顺序。
    for (const auto &day : data.daily) {
      if (day.date <= today)
        continue;
      drawDay(drv, day, count++);
      if (count == 3)
        break;
    }
    if (count == 0) {
      drv->u8g2Fonts.setCursor(10, 245);
      drv->u8g2Fonts.print("未来三天预报暂无数据");
    }
  }

  static void drawDay(DisplayDriver *drv, const DailyData &day, int index) {
    auto &font = drv->u8g2Fonts;
    int x = 8 + index * 132;
    font.setCursor(x, 235);
    font.print(day.day);
    // 图标左移以增加与温度的留白，温度位置固定，避免挤入相邻日期列。
    drawIcon(drv, x + 37, 251, day.icon_str);
    font.setCursor(x + 65, 250);
    font.print(String(day.temp_min) + "~" + String(day.temp_max) + "°");
  }

  static void drawHourly(DisplayDriver *drv, const DateTime &now,
                         const WeatherData &data) {
    char time[7];
    snprintf(time, sizeof(time), "T%02u:%02u", now.hour, now.minute);
    String current = dateKey(now) + time;
    int count = 0;
    // 完整时间用于过滤旧预报，不能只比较 HH:mm，否则午夜后会选错数据。
    for (const auto &hour : data.hourly) {
      if (hour.dateTime.substring(0, 16) <= current)
        continue;
      drawHour(drv, hour, count++);
      if (count == 6)
        break;
    }
    if (count == 0) {
      drv->u8g2Fonts.setCursor(10, 284);
      drv->u8g2Fonts.print("未来小时预报暂无数据");
    }
  }

  static void drawHour(DisplayDriver *drv, const HourlyData &hour, int index) {
    auto &font = drv->u8g2Fonts;
    int x = 8 + index * 66;
    font.setCursor(x, 274);
    font.print(hour.time);
    drawIcon(drv, x, 297, hour.icon_str);
    font.setCursor(x + 22, 295);
    font.print(String(hour.temp) + "°");
  }

  static void drawTasks(DisplayDriver *drv, const std::vector<TodoItem> &tasks) {
    auto &font = drv->u8g2Fonts;
    font.setFont(u8g2_font_helvB08_tr);
    font.setCursor(10, 215);
    font.print("UPCOMING TASKS");
    drv->display.fillRect(370, 205, 20, 14, GxEPD_BLACK);
    font.setForegroundColor(GxEPD_WHITE);
    font.setBackgroundColor(GxEPD_BLACK);
    font.setCursor(377, 216);
    font.print(tasks.size());
    font.setForegroundColor(GxEPD_BLACK);
    font.setBackgroundColor(GxEPD_WHITE);
    font.setFont(u8g2_font_wqy12_t_gb2312);
    for (size_t i = 0; i < 3 && i < tasks.size(); ++i)
      drawTask(drv, tasks[i], i);
  }

  static void drawTask(DisplayDriver *drv, const TodoItem &task, int index) {
    auto &font = drv->u8g2Fonts;
    auto &display = drv->display;
    int y = 221 + index * 26;
    if (task.highPriority)
      display.fillRect(5, y + 4, 4, 18, GxEPD_BLACK);
    else
      display.drawRect(5, y + 4, 4, 18, GxEPD_BLACK);
    font.setCursor(18, y + 18);
    font.print(task.time);
    display.drawLine(55, y + 4, 55, y + 22, GxEPD_BLACK);
    font.setCursor(65, y + 18);
    font.print(task.content);
    font.setCursor(390 - font.getUTF8Width(task.countdown.c_str()), y + 18);
    font.print(task.countdown);
    if (index < 2)
      display.drawLine(0, y + 26, 400, y + 26, GxEPD_BLACK);
  }
};
