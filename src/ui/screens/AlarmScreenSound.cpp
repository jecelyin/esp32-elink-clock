#include "AlarmScreen.h"

void AlarmScreen::beginSoundSettings() {
  // 关键逻辑：每次进入都读取最新配置，使网页保存后设备端不会沿用旧副本。
  // 调整只修改草稿，明确保存成功后才会影响所有闹钟。
  draftSound = configMgr->getAlarmSoundSettings();
  soundFocus = SOUND_VOLUME_DOWN;
  soundSaveError = false;
  helperText = "左右选择，回车调整或保存";
  mode = MODE_SOUND;
}

bool AlarmScreen::handleSoundInput(UIKey key) {
  if (soundSaveError) {
    if (key == UI_KEY_ENTER) soundSaveError = false;
    return true;
  }
  if (key == UI_KEY_LEFT || key == UI_KEY_RIGHT) {
    int delta = key == UI_KEY_LEFT ? -1 : 1;
    soundFocus = (soundFocus + delta + SOUND_FOCUS_COUNT) % SOUND_FOCUS_COUNT;
    return true;
  }
  if (key != UI_KEY_ENTER) return false;
  if (soundFocus == SOUND_SAVE) {
    saveSoundSettings();
  } else if (soundFocus == SOUND_CANCEL) {
    closeSoundSettings();
  } else {
    adjustSoundValue();
  }
  return true;
}

void AlarmScreen::adjustSoundValue() {
  bool isVolume = soundFocus <= SOUND_VOLUME_UP;
  bool increase = soundFocus == SOUND_VOLUME_UP || soundFocus == SOUND_FADE_UP;
  uint8_t &value = isVolume ? draftSound.volume : draftSound.fadeInSeconds;
  uint8_t maximum = isVolume ? AlarmSoundSettings::MAX_VOLUME
                             : AlarmSoundSettings::MAX_FADE_IN_SECONDS;
  if (increase && value < maximum) ++value;
  if (!increase && value > 0) --value;
}

void AlarmScreen::saveSoundSettings() {
  if (!configMgr->saveAlarmSoundSettings(draftSound)) {
    // 关键逻辑：保存失败保留草稿，通过模态错误提示阻止把未持久化状态当作成功。
    soundSaveError = true;
    Serial.println("[Alarm] sound settings could not be written to NVS");
    return;
  }
  closeSoundSettings();
  helperText = "闹钟声音设置已保存";
}

void AlarmScreen::closeSoundSettings() {
  mode = MODE_LIST;
  listFocus = getSoundIndex();
  helperText = "回车编辑当前项，长按退出";
  clampListFocus();
}

void AlarmScreen::drawSoundSettings(DisplayDriver *display) {
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(18, 50);
  display->u8g2Fonts.print("闹钟声音设置");
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 71);
  display->u8g2Fonts.print("所有闹钟共用，独立于音乐和收音机");
  drawSoundValue(display, 88, "闹钟音量", draftSound.volume,
                 AlarmSoundSettings::MAX_VOLUME, SOUND_VOLUME_DOWN);
  drawSoundValue(display, 158, "渐强时长（秒）", draftSound.fadeInSeconds,
                 AlarmSoundSettings::MAX_FADE_IN_SECONDS, SOUND_FADE_DOWN);
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(18, 220);
  display->u8g2Fonts.print("从静音逐渐升至设定音量");
  display->u8g2Fonts.setCursor(18, 239);
  display->u8g2Fonts.print("0 秒关闭渐强；音量 0 为静音");
  drawSoundButton(display, 76, 252, 112, "保存", SOUND_SAVE);
  drawSoundButton(display, 212, 252, 112, "取消", SOUND_CANCEL);
  drawEditorHints(display);
  if (soundSaveError) drawSoundSaveError(display);
}

void AlarmScreen::drawSoundValue(DisplayDriver *display, int y,
                                 const String &label, uint8_t value,
                                 uint8_t maximum, int downFocus) {
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(18, y + 23);
  display->u8g2Fonts.print(label + "  " + value + " / " + maximum);
  display->display.drawRoundRect(18, y + 40, 210, 10, 4, GxEPD_BLACK);
  int filled = static_cast<int>(value) * 206 / maximum;
  if (filled > 0) {
    display->display.fillRect(20, y + 42, filled, 6, GxEPD_BLACK);
  }
  drawSoundButton(display, 250, y, 58, "-1", downFocus);
  drawSoundButton(display, 322, y, 58, "+1", downFocus + 1);
}

void AlarmScreen::drawSoundButton(DisplayDriver *display, int x, int y,
                                  int width, const char *label, int focus) {
  bool focused = focus < 0 || soundFocus == focus;
  if (focused) {
    display->display.fillRoundRect(x, y, width, 30, 7, GxEPD_BLACK);
  } else {
    display->display.drawRoundRect(x, y, width, 30, 7, GxEPD_BLACK);
  }
  display->u8g2Fonts.setForegroundColor(focused ? GxEPD_WHITE : GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(focused ? GxEPD_BLACK : GxEPD_WHITE);
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  int labelWidth = display->u8g2Fonts.getUTF8Width(label);
  display->u8g2Fonts.setCursor(x + (width - labelWidth) / 2, y + 21);
  display->u8g2Fonts.print(label);
  display->u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  display->u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

void AlarmScreen::drawSoundSaveError(DisplayDriver *display) {
  display->display.fillRoundRect(20, 90, 360, 150, 10, GxEPD_WHITE);
  display->display.drawRoundRect(20, 90, 360, 150, 10, GxEPD_BLACK);
  display->u8g2Fonts.setFont(u8g2_font_wqy16_t_gb2312);
  display->u8g2Fonts.setCursor(40, 121);
  display->u8g2Fonts.print("闹钟声音设置未保存");
  display->u8g2Fonts.setFont(u8g2_font_wqy12_t_gb2312);
  display->u8g2Fonts.setCursor(40, 151);
  display->u8g2Fonts.print("无法写入设备存储，草稿已保留。");
  display->u8g2Fonts.setCursor(40, 175);
  display->u8g2Fonts.print("按回车关闭提示后可再次保存。");
  drawSoundButton(display, 144, 194, 112, "确定", -1);
}
