#include "AlarmScreen.h"

namespace {
const char *kRuleLabels[] = {"每天", "指定星期", "工作日"};
const char *kWeekLabels[] = {"一", "二", "三", "四", "五", "六", "日"};
const uint8_t kWeekBits[] = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x01};
const int kContentTop = 30;
const int kRowHeight = 56;
}

void AlarmScreen::drawEditor(DisplayDriver *display) {
  drawEditorTitle(display);
  drawEditorTime(display);
  drawRepeatSelector(display);
  drawWeekdaySelector(display);
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
      display->display.fillRoundRect(x, 254, buttonW, 30, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 254, buttonW, 30, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
    int width = display->u8g2Fonts.getUTF8Width(labels[i]);
    display->u8g2Fonts.setCursor(x + (buttonW - width) / 2, 275);
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
  display->u8g2Fonts.setFont(u8g2_font_logisoso62_tn);
  int width = display->u8g2Fonts.getUTF8Width(timeText);
  display->u8g2Fonts.setCursor((400 - width) / 2, 102);
  display->u8g2Fonts.print(timeText);

  for (int i = 0; i < 2; ++i) {
    int x = 70 + i * 155;
    bool focused = editFocus == i;
    display->display.drawRoundRect(x, 110, 105, 38, 8, GxEPD_BLACK);
    if (focused) {
      display->display.drawRoundRect(x - 3, 107, 111, 44, 10, GxEPD_BLACK);
    }
    display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
    display->u8g2Fonts.setCursor(x + 18, 134);
    display->u8g2Fonts.print(i == 0 ? "小时 回车+1" : "分钟 回车+1");
  }
}

void AlarmScreen::drawEditorTitle(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(18, 52);
  display->u8g2Fonts.print(creatingAlarm ? "新建闹钟" : "编辑闹钟");
  display->u8g2Fonts.setFont(u8g2_font_helvB08_tr);
  display->u8g2Fonts.setCursor(18, 68);
  display->u8g2Fonts.print("LONG ENTER TO EXIT");
}

void AlarmScreen::drawRepeatSelector(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 168);
  display->u8g2Fonts.print("重复规则");

  for (int i = 0; i < 3; ++i) {
    int x = 18 + i * 126;
    bool selected = draftAlarm.repeatType == i;
    bool focused = editFocus == FOCUS_REPEAT_DAILY + i;
    if (selected) {
      display->display.fillRoundRect(x, 176, 112, 34, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 176, 112, 34, 8, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    if (focused) {
      display->display.drawRoundRect(x - 2, 174, 116, 38, 10, GxEPD_BLACK);
    }
    int width = display->u8g2Fonts.getUTF8Width(kRuleLabels[i]);
    display->u8g2Fonts.setCursor(x + (112 - width) / 2, 198);
    display->u8g2Fonts.print(kRuleLabels[i]);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }
}

void AlarmScreen::drawWeekdaySelector(DisplayDriver *display) {
  bool weekly = draftAlarm.repeatType == ALARM_REPEAT_WEEKLY;
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 220);
  display->u8g2Fonts.print(weekly ? "星期选择" : "仅“指定星期”模式可编辑");

  for (int i = 0; i < 7; ++i) {
    int x = 18 + i * 54;
    bool selected = (draftAlarm.weekMask & kWeekBits[i]) != 0;
    bool focused = editFocus == FOCUS_DAY_START + i;
    if (selected && weekly) {
      display->display.fillRoundRect(x, 224, 40, 26, 6, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_WHITE);
      display->u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    } else {
      display->display.drawRoundRect(x, 224, 40, 26, 6, GxEPD_BLACK);
      display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
      display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    }
    if (focused) {
      display->display.drawRoundRect(x - 2, 222, 44, 30, 8, GxEPD_BLACK);
    }
    int width = display->u8g2Fonts.getUTF8Width(kWeekLabels[i]);
    display->u8g2Fonts.setCursor(x + (40 - width) / 2, 243);
    display->u8g2Fonts.print(kWeekLabels[i]);
    display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  }
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
