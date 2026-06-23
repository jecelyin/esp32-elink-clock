#include "RadioScreen.h"
#include "../../config.h"
#include <Arduino.h>

namespace {
const int CONTROL_ROW_COUNT = 5;
const int PRESET_ROW_COUNT = RADIO_PRESET_COUNT;
const int BUTTON_ROW_H = 50;
const uint16_t FREQUENCY_STEP = 10;
const uint32_t RADIO_STATUS_DURATION_MS = 5000UL;

void setRowButtonBounds(RadioScreen::UIButton &button, int index, int count,
                        int y) {
  // 关键逻辑：按钮宽度统一按数量计算，并保留余数分摊逻辑，
  // 避免后续调整按钮数量时出现整行宽度累计偏差。
  int baseW = Layout::SCREEN_W / count;
  int extraW = Layout::SCREEN_W % count;
  button.x = index * baseW + min(index, extraW);
  button.y = y;
  button.w = baseW + (index < extraW ? 1 : 0);
  button.h = BUTTON_ROW_H;
}

void drawButtonLabel(DisplayDriver *display, RadioScreen::UIButton &button) {
  display->u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  if (display->u8g2Fonts.getUTF8Width(button.label) > button.w - 6) {
    display->u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  }

  int textW = display->u8g2Fonts.getUTF8Width(button.label);
  int textX = button.x + (button.w - textW) / 2;
  int textY = button.y + button.h / 2 + 5;
  display->u8g2Fonts.setCursor(textX, textY);
  display->u8g2Fonts.print(button.label);
}
} // namespace

void RadioScreen::initButtons() {
  const char *controls[CONTROL_ROW_COUNT] = {"Scan", "<< Seek", "Seek >>",
                                             "Vol-", "Vol +"};
  int controlY = Layout::CONTROLS_Y;
  int presetY = Layout::CONTROLS_Y + BUTTON_ROW_H;

  for (int i = 0; i < CONTROL_ROW_COUNT; i++) {
    setRowButtonBounds(buttons[i], i, CONTROL_ROW_COUNT, controlY);
    snprintf(buttons[i].label, sizeof(buttons[i].label), "%s", controls[i]);
  }

  for (int i = 0; i < PRESET_ROW_COUNT; i++) {
    int buttonIndex = CONTROL_PRESET_START + i;
    setRowButtonBounds(buttons[buttonIndex], i, PRESET_ROW_COUNT, presetY);
    snprintf(buttons[buttonIndex].label, sizeof(buttons[buttonIndex].label),
             "%d", i + 1);
  }
}

void RadioScreen::drawSingleButton(DisplayDriver *display, int idx,
                                   bool isFocused, bool partial) {
  (void)partial;
  UIButton &button = buttons[idx];
  uint16_t bg = isFocused ? Layout::COLOR_FG : Layout::COLOR_BG;
  uint16_t fg = isFocused ? Layout::COLOR_BG : Layout::COLOR_FG;

  display->display.fillRect(button.x, button.y, button.w, button.h, bg);
  display->display.drawRect(button.x, button.y, button.w, button.h,
                            Layout::COLOR_FG);
  display->u8g2Fonts.setForegroundColor(fg);
  display->u8g2Fonts.setBackgroundColor(bg);
  drawButtonLabel(display, button);
  display->u8g2Fonts.setForegroundColor(Layout::COLOR_FG);
  display->u8g2Fonts.setBackgroundColor(Layout::COLOR_BG);
}

void RadioScreen::updateButtonFocus(DisplayDriver *display, int oldIdx,
                                    int newIdx) {
  int oldRowY = oldIdx == -1 ? -1 : buttons[oldIdx].y;
  int newRowY = buttons[newIdx].y;

  // 关键逻辑：按钮是紧贴排布的，单独局刷旧/新按钮会让相邻边框由不同
  // 分页窗口分别生成，电子墨水屏上容易出现一像素白边或残影。按整行
  // 重绘可以保证背景、焦点块和分隔线来自同一次局刷。
  if (oldRowY != -1 && oldRowY != newRowY) {
    updateButtonRow(display, oldRowY);
  }

  updateButtonRow(display, newRowY);
  display->powerOff();
}

void RadioScreen::updateButtonRow(DisplayDriver *display, int rowY) {
  setAlignedPartialWindow(display, 0, rowY, Layout::SCREEN_W, BUTTON_ROW_H);
  display->display.firstPage();
  do {
    display->display.fillRect(0, rowY, Layout::SCREEN_W, BUTTON_ROW_H,
                              Layout::COLOR_BG);
    for (int i = 0; i < BUTTON_COUNT; i++) {
      if (buttons[i].y == rowY) {
        drawSingleButton(display, i, focusedControl == i, true);
      }
    }
  } while (display->display.nextPage());
}

void RadioScreen::drawButtons(DisplayDriver *display, bool partial) {
  display->display.fillRect(0, Layout::CONTROLS_Y, Layout::SCREEN_W,
                            BUTTON_ROW_H * 2, Layout::COLOR_BG);
  for (int i = 0; i < BUTTON_COUNT; i++) {
    drawSingleButton(display, i, focusedControl == i, partial);
  }
}

void RadioScreen::setRadioStatus(const char *text) {
  radioStatus = text == nullptr ? "" : text;
  radioStatusExpiresAt =
      radioStatus.length() > 0 ? millis() + RADIO_STATUS_DURATION_MS : 0;
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][status] %s\n", radioStatus.c_str());
#endif
}

void RadioScreen::setFrequencyStatus(const char *prefix) {
  char freqStr[12];
  char status[32];
  radio->getFormattedFrequency(freqStr, sizeof(freqStr));
  snprintf(status, sizeof(status), "%s %s MHz", prefix, freqStr);
  setRadioStatus(status);
}

void RadioScreen::setScanResultStatus(uint8_t count) {
  char status[32];
  snprintf(status, sizeof(status), "Scan: %u stations", count);
  setRadioStatus(status);
}

void RadioScreen::setVolumeStatus() {
  char status[20];
  snprintf(status, sizeof(status), "Vol: %u", config->config.volume);
  setRadioStatus(status);
}

bool RadioScreen::onInput(UIKey key) {
  int oldFocusedControl = focusedControl;
  bool handled = true;
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][input] key=%d focus=%d freq=%u vol=%d\n", key,
                focusedControl, radio ? radio->getFrequency() : 0,
                config ? config->config.volume : -1);
#endif

  if (key == UI_KEY_LEFT) {
    moveFocus(-1);
  } else if (key == UI_KEY_RIGHT) {
    moveFocus(1);
  } else if (key == UI_KEY_ENTER) {
    handleEnterAction();
  } else {
    handled = false;
  }

#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][result] key=%d focus=%d->%d handled=%d\n", key,
                oldFocusedControl, focusedControl, handled);
#endif
  return handled;
}

void RadioScreen::handleEnterAction() {
  saveFocusIfChanged();
  if (focusedControl == CONTROL_SCAN) {
    scanAndSavePresets();
  } else if (focusedControl == CONTROL_SEEK_DOWN) {
    tuneByStep(-1, "Seek");
  } else if (focusedControl == CONTROL_SEEK_UP) {
    tuneByStep(1, "Seek");
  } else if (focusedControl == CONTROL_VOL_DOWN) {
    changeVolume(-1);
  } else if (focusedControl == CONTROL_VOL_UP) {
    changeVolume(1);
  } else if (focusedControl >= CONTROL_PRESET_START) {
    loadPreset(focusedControl - CONTROL_PRESET_START);
  }
}

void RadioScreen::moveFocus(int offset) {
  focusedControl = (focusedControl + offset + BUTTON_COUNT) % BUTTON_COUNT;
}

void RadioScreen::tuneByStep(int offset, const char *statusPrefix) {
  uint16_t currentFreq = radio->getFrequency();
  int nextFreq = currentFreq + offset * FREQUENCY_STEP;
  nextFreq = constrain(nextFreq, radio->getMinFrequency(),
                       radio->getMaxFrequency());
  if (nextFreq == currentFreq) {
    setFrequencyStatus(statusPrefix);
    return;
  }
  radio->setFrequency((uint16_t)nextFreq);
  setFrequencyStatus(statusPrefix);
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][step] action=%s offset=%d %u->%u\n", statusPrefix,
                offset, currentFreq, (uint16_t)nextFreq);
#endif
}

void RadioScreen::changeVolume(int offset) {
  int nextVolume = config->config.volume + offset;
  nextVolume = constrain(nextVolume, 0, 15);
  if (nextVolume == config->config.volume) {
    setVolumeStatus();
    return;
  }
  config->config.volume = nextVolume;
  radio->setVolume(config->config.volume);
  config->save();
  setVolumeStatus();
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][volume] value=%u\n", config->config.volume);
#endif
}

void RadioScreen::scanAndSavePresets() {
  uint16_t stations[RADIO_PRESET_COUNT] = {0};
#if ENABLE_SERIAL_DEBUG
  Serial.println("[Radio][scan] ui start");
#endif
  uint8_t count = radio->scanStations(stations, RADIO_PRESET_COUNT);
  saveScannedPresets(stations, count);
  setScanResultStatus(count);
#if ENABLE_SERIAL_DEBUG
  Serial.printf("[Radio][scan] ui done count=%u\n", count);
#endif
}

void RadioScreen::saveScannedPresets(uint16_t *stations, uint8_t count) {
  // 关键逻辑：Scan 的结果代表当前电磁环境下的新预设列表；
  // 未扫到的位置必须清零，否则旧电台会混在新结果后面造成误判。
  for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
    config->config.radio_presets[i] = i < count ? stations[i] : 0;
  }
  config->save();
#if ENABLE_SERIAL_DEBUG
  Serial.print("[Radio][scan] saved presets:");
  for (int i = 0; i < RADIO_PRESET_COUNT; i++) {
    Serial.printf(" %u", config->config.radio_presets[i]);
  }
  Serial.println();
#endif
}

void RadioScreen::loadPreset(int index) {
  if (index < 0 || index >= RADIO_PRESET_COUNT) {
    return;
  }

  uint16_t freq = config->config.radio_presets[index];
  if (freq > 0) {
    radio->setFrequency(freq);
    setFrequencyStatus("Preset");
#if ENABLE_SERIAL_DEBUG
    Serial.printf("[Radio][preset] index=%d freq=%u\n", index, freq);
#endif
  } else {
    setRadioStatus("Preset empty");
#if ENABLE_SERIAL_DEBUG
    Serial.printf("[Radio][preset] index=%d empty\n", index);
#endif
  }
}
