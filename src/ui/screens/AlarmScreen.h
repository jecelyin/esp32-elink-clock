#pragma once

#include "../../managers/AlarmManager.h"
#include "../Screen.h"
#include "../components/StatusBar.h"

class AlarmScreen : public Screen {
public:
  AlarmScreen(AlarmManager *alarmMgr, StatusBar *statusBar);

  void init() override;
  void enter() override;
  void draw(DisplayDriver *display) override;
  bool onInput(UIKey key) override;

private:
  enum AlarmScreenMode { MODE_LIST = 0, MODE_EDITOR = 1 };

  static const int FOCUS_HOUR = 0;
  static const int FOCUS_MINUTE_FINE_DOWN = 1;
  static const int FOCUS_MINUTE = 2;
  static const int FOCUS_MINUTE_FINE_UP = 3;
  static const int FOCUS_REPEAT_DAILY = 4;
  static const int FOCUS_REPEAT_WEEKLY = 5;
  static const int FOCUS_REPEAT_WORKDAY = 6;
  static const int FOCUS_DAY_START = 7;
  static const int FOCUS_DAY_END = 13;
  static const int FOCUS_RINGTONE = 14;
  static const int ACTION_SAVE = 15;
  static const int ACTION_CANCEL = 16;
  static const int ACTION_DELETE = 17;
  static const int ACTION_TOGGLE_ENABLED = 18;
  static const int VISIBLE_ROWS = 4;

  AlarmManager *alarmMgr;
  StatusBar *statusBar;
  AlarmScreenMode mode;
  AlarmConfig draftAlarm;
  String helperText;
  int editFocus;
  int listFocus;
  int listScroll;
  bool partialRefreshPending;
  bool creatingAlarm;
  size_t editingIndex;

  void adjustCurrentValue(int delta);
  void applyRepeatSelection();
  void beginCreate();
  void beginEdit(size_t index);
  void clampListFocus();
  bool commitDraft();
  void drawEditor(DisplayDriver *display);
  void drawEditorActions(DisplayDriver *display);
  void drawEditorHints(DisplayDriver *display);
  void drawEditorTime(DisplayDriver *display);
  void drawEditorTitle(DisplayDriver *display);
  void drawRepeatSelector(DisplayDriver *display);
  void drawRingtoneSelector(DisplayDriver *display);
  void drawWeekdaySelector(DisplayDriver *display);
  void drawContentPartial(DisplayDriver *display);
  void drawList(DisplayDriver *display);
  void drawListHeader(DisplayDriver *display);
  void drawListRow(DisplayDriver *display, int itemIndex, int rowIndex);
  int getEditorActionCount() const;
  int getRepeatFocus(AlarmRepeatType repeatType) const;
  int getLastEditorFocus() const;
  int getActionRowCount() const;
  int getAddIndex() const;
  int getBackIndex() const;
  int getItemCount() const;
  String getListRowSubtitle(int itemIndex) const;
  String getListRowTitle(int itemIndex) const;
  String getRingtoneLabel() const;
  String getTimeText(const AlarmConfig &alarm) const;
  uint8_t getWeekdayBit(int dayFocus) const;
  void handleDeleteAction();
  bool handleEditorEnter();
  void drawCheckbox(DisplayDriver *display, int x, int y, bool checked,
                    bool focused);
  bool handleEditorInput(UIKey key);
  bool handleListInput(UIKey key);
  bool isAlarmIndex(int itemIndex) const;
  bool isEditorFocusAvailable(int focus) const;
  bool isEditorDayFocus() const;
  bool isRepeatFocus() const;
  bool isSaveFocus() const;
  void moveEditorFocus(int delta);
  void moveListFocus(int delta);
  void openEditorForFocus();
  void renderContent(DisplayDriver *display);
  void selectNextRingtone();
  void setRepeatFocus();
  void resetEditorState();
  void resetListState();
};
