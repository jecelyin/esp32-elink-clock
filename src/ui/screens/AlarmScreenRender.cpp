#include "AlarmScreen.h"

namespace {
const char *kRuleLabels[] = {"每天", "指定星期", "工作日"};
const char *kWeekLabels[] = {"一", "二", "三", "四", "五", "六", "日"};
const uint8_t kWeekBits[] = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x01};
const int kContentTop = 30;
const int kRowHeight = 56;
const int kContentPartialTop = 24;
const int kScreenWidth = 400;
const int kScreenHeight = 300;
}

void AlarmScreen::drawEditor(DisplayDriver *display) {
  drawEditorTitle(display);
  drawEditorTime(display);
  drawRepeatSelector(display);
  drawWeekdaySelector(display);
  drawRingtoneSelector(display);
  drawEditorActions(display);
  drawEditorHints(display);
}

void AlarmScreen::drawEditorActions(DisplayDriver *display) {
  String deleteLabel = "删除";
  const char *toggleLabel = draftAlarm.enabled ? "停用" : "启用";
  const char *labels[] = {"保存", "取消", deleteLabel.c_str(), toggleLabel};
  int focusMap[] = {ACTION_SAVE, ACTION_CANCEL, ACTION_DELETE,
                    ACTION_TOGGLE_ENABLED};
  int actionCount = getEditorActionCount();
  int startX = creatingAlarm ? 76 : 18;
  int buttonW = creatingAlarm ? 112 : 82;
  int gap = creatingAlarm ? 136 : 94;

  for (int i = 0; i < actionCount; ++i) {
    int x = startX + i * gap;
    bool focused = editFocus == focusMap[i];
    if (focused) {
      display->display.fillRoundRect(x, 252, buttonW, 30, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 252, buttonW, 30, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    int width = display->u8g2Fonts.getUTF8Width(labels[i]);
    display->u8g2Fonts.setCursor(x + (buttonW - width) / 2, 273);
    display->u8g2Fonts.print(labels[i]);
  }
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void AlarmScreen::drawEditorHints(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 292);
  display->u8g2Fonts.print(helperText);
}

void AlarmScreen::drawEditorTime(DisplayDriver *display) {
  char timeText[6];
  sprintf(timeText, "%02d:%02d", draftAlarm.hour, draftAlarm.minute);
  // Keep the time visually dominant without colliding with the title row.
  display->u8g2Fonts.setFont(u8g2_font_logisoso42_tn);
  int width = display->u8g2Fonts.getUTF8Width(timeText);
  display->u8g2Fonts.setCursor((400 - width) / 2, 102);
  display->u8g2Fonts.print(timeText);

  struct TimeControl {
    int focus;
    int x;
    int width;
    const char *label;
  };
  const TimeControl controls[] = {
      {FOCUS_HOUR, 12, 88, "小时 +1"},
      {FOCUS_MINUTE_FINE_DOWN, 108, 78, "分钟 -1"},
      {FOCUS_MINUTE, 194, 98, "分钟 +10"},
      {FOCUS_MINUTE_FINE_UP, 300, 88, "分钟 +1"},
  };

  for (const TimeControl &control : controls) {
    bool focused = editFocus == control.focus;
    if (focused) {
      display->display.fillRoundRect(control.x, 108, control.width, 32, 7,
                                     GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(control.x, 108, control.width, 32, 7,
                                     GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    int labelWidth = display->u8g2Fonts.getUTF8Width(control.label);
    display->u8g2Fonts.setCursor(
        control.x + (control.width - labelWidth) / 2, 129);
    display->u8g2Fonts.print(control.label);
  }
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void AlarmScreen::drawContentPartial(DisplayDriver *display) {
  display->display.setPartialWindow(0, kContentPartialTop, kScreenWidth,
                                    kScreenHeight - kContentPartialTop);
  display->display.firstPage();
  do {
    display->display.fillScreen(GxEPD_WHITE);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    renderContent(display);
  } while (display->display.nextPage());
  display->powerOff();
}

void AlarmScreen::drawEditorTitle(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(16, 45);
  display->u8g2Fonts.print(creatingAlarm ? "新建闹钟" : "编辑闹钟");
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  const char *state = draftAlarm.enabled ? "已启用" : "已停用";
  int width = display->u8g2Fonts.getUTF8Width(state);
  display->u8g2Fonts.setCursor(384 - width, 45);
  display->u8g2Fonts.print(state);
}

void AlarmScreen::drawRepeatSelector(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  for (int i = 0; i < 3; ++i) {
    int x = 18 + i * 126;
    bool selected = draftAlarm.repeatType == i;
    bool focused = editFocus == FOCUS_REPEAT_DAILY + i;
    if (selected) {
      display->display.fillRoundRect(x, 146, 112, 30, 7, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 146, 112, 30, 7, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    if (focused) {
      display->display.drawRoundRect(x - 2, 144, 116, 34, 9, GxEPD_BLACK);
    }
    int width = display->u8g2Fonts.getUTF8Width(kRuleLabels[i]);
    display->u8g2Fonts.setCursor(x + (112 - width) / 2, 166);
    display->u8g2Fonts.print(kRuleLabels[i]);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }
}

void AlarmScreen::drawWeekdaySelector(DisplayDriver *display) {
  bool weekly = draftAlarm.repeatType == ALARM_REPEAT_WEEKLY;
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(14, 199);
  display->u8g2Fonts.print("周");

  for (int i = 0; i < 7; ++i) {
    int x = 44 + i * 49;
    bool selected = (draftAlarm.weekMask & kWeekBits[i]) != 0;
    bool focused = editFocus == FOCUS_DAY_START + i;
    if (selected && weekly) {
      display->display.fillRoundRect(x, 181, 38, 25, 6, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 181, 38, 25, 6, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    if (focused) {
      display->display.drawRoundRect(x - 2, 179, 42, 29, 8, GxEPD_BLACK);
    }
    int width = display->u8g2Fonts.getUTF8Width(kWeekLabels[i]);
    display->u8g2Fonts.setCursor(x + (38 - width) / 2, 199);
    display->u8g2Fonts.print(kWeekLabels[i]);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }
}

void AlarmScreen::drawRingtoneSelector(DisplayDriver *display) {
  const int x = 18;
  const int y = 213;
  const int width = 364;
  const int height = 30;
  bool focused = editFocus == FOCUS_RINGTONE;

  if (focused) {
    display->display.fillRoundRect(x, y, width, height, 7, GxEPD_BLACK);
    display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
  } else {
    display->display.drawRoundRect(x, y, width, height, 7, GxEPD_BLACK);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }

  String label = String("铃声  ") + getRingtoneLabel();
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  int labelWidth = display->u8g2Fonts.getUTF8Width(label.c_str());
  display->u8g2Fonts.setCursor(x + (width - labelWidth) / 2, y + 20);
  display->u8g2Fonts.print(label);
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void AlarmScreen::drawList(DisplayDriver *display) {
  drawListHeader(display);
  for (int row = 0; row < VISIBLE_ROWS; ++row) {
    int itemIndex = listScroll + row;
    if (itemIndex >= getItemCount()) {
      break;
    }
    drawListRow(display, itemIndex, row);
  }
}

void AlarmScreen::drawListHeader(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(18, 52);
  display->u8g2Fonts.print("闹钟");
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 72);
  display->u8g2Fonts.print(helperText);
}

void AlarmScreen::drawListRow(DisplayDriver *display, int itemIndex, int rowIndex) {
  int y = kContentTop + 48 + rowIndex * kRowHeight;
  bool focused = itemIndex == listFocus;
  if (focused) {
    display->display.fillRoundRect(12, y, 376, 48, 10, GxEPD_BLACK);
    display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
  } else {
    display->display.drawRoundRect(12, y, 376, 48, 10, GxEPD_BLACK);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }

  display->u8g2Fonts.setFont(isAlarmIndex(itemIndex) ? u8g2_font_helvB18_tf
                                                      : u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(28, y + 31);
  display->u8g2Fonts.print(getListRowTitle(itemIndex));
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(150, y + 21);
  display->u8g2Fonts.print(getListRowSubtitle(itemIndex));
  if (isAlarmIndex(itemIndex)) {
    drawCheckbox(display, 344, y + 12, alarmMgr->getAlarm(itemIndex).enabled,
                 focused);
    display->u8g2Fonts.setCursor(150, y + 38);
    display->u8g2Fonts.print("回车编辑");
  }
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

int AlarmScreen::getEditorActionCount() const { return creatingAlarm ? 2 : 4; }

int AlarmScreen::getLastEditorFocus() const {
  return creatingAlarm ? ACTION_CANCEL : ACTION_TOGGLE_ENABLED;
}

int AlarmScreen::getActionRowCount() const { return 2; }

int AlarmScreen::getAddIndex() const {
  return static_cast<int>(alarmMgr->getAlarmCount());
}

int AlarmScreen::getBackIndex() const { return getAddIndex() + 1; }

int AlarmScreen::getItemCount() const {
  return static_cast<int>(alarmMgr->getAlarmCount()) + getActionRowCount();
}

String AlarmScreen::getListRowSubtitle(int itemIndex) const {
  if (itemIndex == getAddIndex()) {
    return "工作日 / 每天 / 多选星期";
  }
  if (itemIndex == getBackIndex()) {
    return "返回菜单";
  }
  return alarmMgr->getRepeatText(alarmMgr->getAlarm(itemIndex));
}

String AlarmScreen::getListRowTitle(int itemIndex) const {
  if (itemIndex == getAddIndex()) {
    return "新增闹钟";
  }
  if (itemIndex == getBackIndex()) {
    return "返回";
  }
  return getTimeText(alarmMgr->getAlarm(itemIndex));
}

String AlarmScreen::getRingtoneLabel() const {
  String ringtone = AlarmRingtones::normalize(draftAlarm.ringtone);
  for (size_t i = 0; i < AlarmRingtones::BUILTIN_COUNT; ++i) {
    if (ringtone == AlarmRingtones::BUILTIN[i].value) {
      return AlarmRingtones::BUILTIN[i].label;
    }
  }
  return ringtone.startsWith("sd:/") ? "SD 自定义铃声" : "默认闹铃";
}

String AlarmScreen::getTimeText(const AlarmConfig &alarm) const {
  char buffer[6];
  sprintf(buffer, "%02d:%02d", alarm.hour, alarm.minute);
  return String(buffer);
}

uint8_t AlarmScreen::getWeekdayBit(int dayFocus) const {
  return kWeekBits[dayFocus - FOCUS_DAY_START];
}

void AlarmScreen::drawCheckbox(DisplayDriver *display, int x, int y, bool checked,
                               bool focused) {
  int boxSize = 18;
  display->display.drawRect(x, y, boxSize, boxSize, focused ? GxEPD_WHITE
                                                             : GxEPD_BLACK);
  if (!checked) {
    return;
  }

  uint16_t color = focused ? GxEPD_WHITE : GxEPD_BLACK;
  display->display.drawLine(x + 3, y + 10, x + 7, y + 14, color);
  display->display.drawLine(x + 7, y + 14, x + 15, y + 4, color);
  display->display.drawLine(x + 3, y + 11, x + 7, y + 15, color);
  display->display.drawLine(x + 7, y + 15, x + 15, y + 5, color);
}
