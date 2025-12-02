#pragma once

#include "../../drivers/AudioDriver.h"
#include "../Screen.h"
#include "../UIManager.h"

class MusicScreen : public Screen {
public:
  MusicScreen(AudioDriver *audio) : audio(audio) {}

  void draw(DisplayDriver *display) override {
    display->display.setFullWindow();
    display->display.firstPage();
    do {
      display->display.fillScreen(GxEPD_WHITE);

      // Header
      display->u8g2Fonts.setFont(u8g2_font_helvB18_tf);
      display->u8g2Fonts.setCursor(150, 40);
      display->u8g2Fonts.print("MUSIC");

      // Icon
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_embedded_4x_t);
      display->u8g2Fonts.setCursor(20, 60);
      display->u8g2Fonts.print("M");

      // Song Info
      display->u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      display->u8g2Fonts.setCursor(50, 120);
      if (audio->isPlaying()) {
        display->u8g2Fonts.print("Playing: Song.mp3");
      } else {
        display->u8g2Fonts.print("Stopped");
      }

      // Controls
      display->u8g2Fonts.setFont(u8g2_font_open_iconic_play_2x_t);
      display->u8g2Fonts.setCursor(100, 200);
      display->u8g2Fonts.print("D"); // Pause
      display->u8g2Fonts.setCursor(150, 200);
      display->u8g2Fonts.print("E"); // Play
      display->u8g2Fonts.setCursor(200, 200);
      display->u8g2Fonts.print("F"); // Stop

    } while (display->display.nextPage());
  }

  void handleInput(int key) override {
    if (key == 1) { // Menu/Back
      uiManager->switchScreen(SCREEN_MENU);
    }
    // Add playback controls
  }

private:
  AudioDriver *audio;
};
