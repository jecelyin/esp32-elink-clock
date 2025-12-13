#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>
#include "../drivers/RtcDriver.h"

// For UI display
struct TodoItem {
    String time;        // HH:MM
    String content;
    String countdown;   // "-XXm" or "--"
    bool highPriority;
};

// For storage and logic
struct TodoConfig {
    int id;
    int hour;
    int minute;
    String content;
    bool highPriority;
    uint8_t weekDays; // Bitmask: 0=Sun, 1=Mon, ... 6=Sat. 0x7F=Daily
    bool enabled;
};

class TodoManager {
public:
    TodoManager();
    void begin();
    
    // For UI
    std::vector<TodoItem> getVisibleTodos(const DateTime& now);
    
    // For Web/Config
    String getTodosJSON();
    void saveTodosFromJSON(const String& json);
    
    // Helper
    void addSampleData();

private:
    Preferences prefs;
    std::vector<TodoConfig> todos;
    
    void load();
    void save();
    String formatTime(int h, int m);
    String formatCountdown(const DateTime& now, int targetH, int targetM);
};
