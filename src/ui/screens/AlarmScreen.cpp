#include "AlarmScreen.h"
#include "../UIManager.h"

AlarmScreen::AlarmScreen(AlarmManager *alarmMgr, StatusBar *statusBar)
    : alarmMgr(alarmMgr), statusBar(statusBar), mode(MODE_LIST),
      helperText(""), editFocus(FOCUS_HOUR), listFocus(0), listScroll(0),
      partialRefreshPending(false), creatingAlarm(false), editingIndex(0) {
  resetListState();
  resetEditorState();
}

void AlarmScreen::init() {
  listFocus = 0;
  listScroll = 0;
  partialRefreshPending = false;
  resetListState();
}

void AlarmScreen::enter() {
  listFocus = 0;
  listScroll = 0;
  partialRefreshPending = false;
  resetListState();
}

void AlarmScreen::draw(DisplayDriver *display) {
  if (partialRefreshPending) {
    partialRefreshPending = false;
    drawContentPartial(display);
    return;
  }

  display->display.setFullWindow();
  display->display.firstPage();
  do {
    display->display.fillScreen(GxEPD_WHITE);
    statusBar->draw(display, true);
    renderContent(display);
  } while (display->display.nextPage());
  display->powerOff();
}

bool AlarmScreen::onInput(UIKey key) {
  bool handled = false;
  if (mode == MODE_EDITOR) {
    handled = handleEditorInput(key);
  } else {
    handled = handleListInput(key);
  }
  if (handled && uiManager->getCurrentState() == SCREEN_ALARM) {
    // UIManager will call draw() after input while holding its drawing guard.
    // Defer the partial refresh until then instead of driving the panel from
    // inside the input callback.
    partialRefreshPending = true;
  }
  return handled;
}

void AlarmScreen::adjustCurrentValue(int delta) {
  if (editFocus == FOCUS_HOUR) {
    draftAlarm.hour = (draftAlarm.hour + delta + 24) % 24;
    return;
  }
  if (editFocus == FOCUS_MINUTE) {
    draftAlarm.minute = (draftAlarm.minute + delta + 60) % 60;
  }
}

void AlarmScreen::applyRepeatSelection() {
  AlarmRepeatType previousRepeatType = draftAlarm.repeatType;
  if (editFocus == FOCUS_REPEAT_DAILY) {
    draftAlarm.repeatType = ALARM_REPEAT_DAILY;
  } else if (editFocus == FOCUS_REPEAT_WEEKLY) {
    draftAlarm.repeatType = ALARM_REPEAT_WEEKLY;
  } else {
    draftAlarm.repeatType = ALARM_REPEAT_WORKDAY;
  }
  // 关键逻辑：每天/工作日的 weekMask 表示整组规则，不能直接沿用为
  // “指定星期”的初始勾选，否则用户再按目标日期会把它们反向取消。
  if (draftAlarm.repeatType == ALARM_REPEAT_WEEKLY &&
      previousRepeatType != ALARM_REPEAT_WEEKLY) {
    draftAlarm.weekMask = 0;
    helperText = "请选择至少一个星期";
    return;
  }
  helperText = draftAlarm.repeatType == ALARM_REPEAT_WORKDAY
                   ? "法定休息日会跳过，补班日会响铃"
                   : "规则已切换";
}

void AlarmScreen::beginCreate() {
  resetEditorState();
  creatingAlarm = true;
  editingIndex = alarmMgr->getAlarmCount();
  draftAlarm = alarmMgr->buildDefaultAlarm();
  setRepeatFocus();
  helperText = "左右选择，回车修改，长按退出";
  mode = MODE_EDITOR;
}

void AlarmScreen::beginEdit(size_t index) {
  resetEditorState();
  creatingAlarm = false;
  editingIndex = index;
  draftAlarm = alarmMgr->getAlarm(index);
  setRepeatFocus();
  helperText = "左右选择，回车修改，长按退出";
  mode = MODE_EDITOR;
}

void AlarmScreen::clampListFocus() {
  int itemCount = getItemCount();
  if (itemCount <= 0) {
    listFocus = 0;
    listScroll = 0;
    return;
  }
  if (listFocus >= itemCount) {
    listFocus = itemCount - 1;
  }
  if (listFocus < 0) {
    listFocus = 0;
  }
  if (listFocus < listScroll) {
    listScroll = listFocus;
  }
  if (listFocus >= listScroll + VISIBLE_ROWS) {
    listScroll = listFocus - VISIBLE_ROWS + 1;
  }
}

bool AlarmScreen::commitDraft() {
  if (draftAlarm.repeatType == ALARM_REPEAT_WEEKLY && draftAlarm.weekMask == 0) {
    helperText = "指定星期至少选择一天";
    return true;
  }

  bool saved = creatingAlarm ? alarmMgr->addAlarm(draftAlarm)
                             : alarmMgr->updateAlarm(editingIndex, draftAlarm);
  if (!saved) {
    helperText = "保存失败，请重试";
    Serial.println("[Alarm] editor save failed");
    return true;
  }

  listFocus = creatingAlarm
                  ? static_cast<int>(alarmMgr->getAlarmCount()) - 1
                  : static_cast<int>(editingIndex);

  helperText = "回车编辑当前项，长按退出";
  mode = MODE_LIST;
  clampListFocus();
  return true;
}

bool AlarmScreen::handleEditorInput(UIKey key) {
  if (key == UI_KEY_LEFT)
    moveEditorFocus(-1);
  if (key == UI_KEY_RIGHT)
    moveEditorFocus(1);
  if (key == UI_KEY_ENTER)
    return handleEditorEnter();
  return true;
}

void AlarmScreen::handleDeleteAction() {
  if (creatingAlarm) {
    resetListState();
    return;
  }
  alarmMgr->removeAlarm(editingIndex);
  resetListState();
  clampListFocus();
}

bool AlarmScreen::handleEditorEnter() {
  if (editFocus == FOCUS_HOUR) {
    adjustCurrentValue(1);
    helperText = "小时已加 1";
  } else if (editFocus == FOCUS_MINUTE) {
    adjustCurrentValue(10);
    helperText = "分钟已加 10";
  } else if (editFocus == FOCUS_MINUTE_FINE_DOWN) {
    draftAlarm.minute = (draftAlarm.minute + 59) % 60;
    helperText = "分钟已减 1";
  } else if (editFocus == FOCUS_MINUTE_FINE_UP) {
    draftAlarm.minute = (draftAlarm.minute + 1) % 60;
    helperText = "分钟已加 1";
  } else if (editFocus == FOCUS_RINGTONE) {
    selectNextRingtone();
  } else if (isRepeatFocus()) {
    applyRepeatSelection();
  } else if (isEditorDayFocus()) {
    if (draftAlarm.repeatType == ALARM_REPEAT_WEEKLY) {
      draftAlarm.weekMask ^= getWeekdayBit(editFocus);
      helperText = "可多选星期，保存前至少保留一天";
    }
  } else if (isSaveFocus()) {
    return commitDraft();
  } else if (editFocus == ACTION_CANCEL) {
    resetListState();
  } else if (editFocus == ACTION_DELETE) {
    handleDeleteAction();
  } else if (editFocus == ACTION_TOGGLE_ENABLED) {
    draftAlarm.enabled = !draftAlarm.enabled;
    helperText = draftAlarm.enabled ? "已启用，回车保存生效"
                                    : "已停用，回车保存生效";
  }
  return true;
}

bool AlarmScreen::handleListInput(UIKey key) {
  if (key == UI_KEY_LEFT) {
    moveListFocus(-1);
    return true;
  }
  if (key == UI_KEY_RIGHT) {
    moveListFocus(1);
    return true;
  }
  if (key != UI_KEY_ENTER) {
    return false;
  }
  if (listFocus == getAddIndex()) {
    beginCreate();
    return true;
  }
  if (listFocus == getBackIndex()) {
    uiManager->switchScreen(SCREEN_MENU);
    return true;
  }
  openEditorForFocus();
  return true;
}

bool AlarmScreen::isAlarmIndex(int itemIndex) const {
  return itemIndex >= 0 && itemIndex < static_cast<int>(alarmMgr->getAlarmCount());
}

bool AlarmScreen::isEditorFocusAvailable(int focus) const {
  return draftAlarm.repeatType == ALARM_REPEAT_WEEKLY ||
         focus < FOCUS_DAY_START || focus > FOCUS_DAY_END;
}

bool AlarmScreen::isEditorDayFocus() const {
  return editFocus >= FOCUS_DAY_START && editFocus <= FOCUS_DAY_END;
}

bool AlarmScreen::isRepeatFocus() const {
  return editFocus >= FOCUS_REPEAT_DAILY && editFocus <= FOCUS_REPEAT_WORKDAY;
}

bool AlarmScreen::isSaveFocus() const { return editFocus == ACTION_SAVE; }

int AlarmScreen::getRepeatFocus(AlarmRepeatType repeatType) const {
  if (repeatType == ALARM_REPEAT_DAILY) {
    return FOCUS_REPEAT_DAILY;
  }
  if (repeatType == ALARM_REPEAT_WEEKLY) {
    return FOCUS_REPEAT_WEEKLY;
  }
  return FOCUS_REPEAT_WORKDAY;
}

void AlarmScreen::moveEditorFocus(int delta) {
  do {
    editFocus += delta;
    if (editFocus > getLastEditorFocus()) {
      editFocus = FOCUS_HOUR;
    }
    if (editFocus < FOCUS_HOUR) {
      editFocus = getLastEditorFocus();
    }
  } while (!isEditorFocusAvailable(editFocus));
}

void AlarmScreen::moveListFocus(int delta) {
  int itemCount = getItemCount();
  if (itemCount <= 0) {
    return;
  }
  listFocus = (listFocus + delta + itemCount) % itemCount;
  clampListFocus();
}

void AlarmScreen::openEditorForFocus() {
  if (isAlarmIndex(listFocus)) {
    beginEdit(static_cast<size_t>(listFocus));
  }
}

void AlarmScreen::renderContent(DisplayDriver *display) {
  if (mode == MODE_LIST) {
    drawList(display);
    return;
  }
  drawEditor(display);
}

void AlarmScreen::selectNextRingtone() {
  String current = AlarmRingtones::normalize(draftAlarm.ringtone);
  for (size_t i = 0; i < AlarmRingtones::BUILTIN_COUNT; ++i) {
    if (current == AlarmRingtones::BUILTIN[i].value) {
      size_t next = (i + 1) % AlarmRingtones::BUILTIN_COUNT;
      draftAlarm.ringtone = AlarmRingtones::BUILTIN[next].value;
      helperText = String("铃声：") + AlarmRingtones::BUILTIN[next].label;
      return;
    }
  }

  // A custom SD ringtone remains untouched unless the user explicitly cycles
  // this field. The first press switches it to the first built-in sound.
  draftAlarm.ringtone = AlarmRingtones::DEFAULT_VALUE;
  helperText = String("铃声：") + AlarmRingtones::BUILTIN[0].label;
}

void AlarmScreen::setRepeatFocus() { editFocus = getRepeatFocus(draftAlarm.repeatType); }

void AlarmScreen::resetEditorState() {
  draftAlarm = alarmMgr->buildDefaultAlarm();
  helperText = "回车编辑，长按退出";
  editFocus = FOCUS_REPEAT_WORKDAY;
  creatingAlarm = false;
  editingIndex = 0;
}

void AlarmScreen::resetListState() {
  mode = MODE_LIST;
  editFocus = FOCUS_REPEAT_WORKDAY;
  helperText = "回车编辑当前项，长按退出";
  if (!isAlarmIndex(listFocus)) {
    listFocus = 0;
  }
  clampListFocus();
}
