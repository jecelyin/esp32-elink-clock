#include "TodoManager.h"

TodoManager::TodoManager() {}

void TodoManager::begin() {
    prefs.begin("todos", false);
    load();
    if (todos.empty()) {
        addSampleData();
    }
}

void TodoManager::load() {
    todos.clear();
    String json = prefs.getString("data", "[]");
    
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.println("Failed to parse todos");
        return;
    }
    
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        TodoConfig item;
        item.id = obj["id"] | 0;
        item.hour = obj["h"];
        item.minute = obj["m"];
        item.content = obj["c"].as<String>();
        item.highPriority = obj["p"];
        item.weekDays = obj["w"];
        item.enabled = obj["e"];
        todos.push_back(item);
    }
}

void TodoManager::save() {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& item : todos) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = item.id;
        obj["h"] = item.hour;
        obj["m"] = item.minute;
        obj["c"] = item.content;
        obj["p"] = item.highPriority;
        obj["w"] = item.weekDays;
        obj["e"] = item.enabled;
    }
    
    String json;
    serializeJson(doc, json);
    prefs.putString("data", json);
}

void TodoManager::addSampleData() {
    todos.push_back({1, 13, 30, "产品设计评审会议", true, 0x7F, true});
    todos.push_back({2, 15, 15, "发送周报邮件", false, 0x7F, true});
    todos.push_back({3, 18, 05, "取快递 (顺丰)", false, 0x7F, true});
    save();
}

String TodoManager::getTodosJSON() {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& item : todos) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = item.id;
        obj["h"] = item.hour;
        obj["m"] = item.minute;
        obj["c"] = item.content;
        obj["p"] = item.highPriority;
        obj["w"] = item.weekDays;
        obj["e"] = item.enabled;
    }
    
    String json;
    serializeJson(doc, json);
    return json;
}

void TodoManager::saveTodosFromJSON(const String& json) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.println("Invalid JSON");
        return;
    }
    
    todos.clear();
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        TodoConfig item;
        item.id = obj["id"]; // Generate ID if 0?
        if (item.id == 0) item.id = millis(); // Simple ID generation
        item.hour = obj["h"];
        item.minute = obj["m"];
        item.content = obj["c"].as<String>();
        item.highPriority = obj["p"];
        item.weekDays = obj["w"];
        item.enabled = obj["e"];
        todos.push_back(item);
    }
    
    // Sort by time?
    std::sort(todos.begin(), todos.end(), [](const TodoConfig& a, const TodoConfig& b) {
        if (a.hour != b.hour) return a.hour < b.hour;
        return a.minute < b.minute;
    });
    
    save();
}

std::vector<TodoItem> TodoManager::getVisibleTodos(const DateTime& now) {
    std::vector<TodoItem> result;
    if (todos.empty()) return result;

    // Filter by day of week
    // now.week is 0-6? RtcDriver uses 0=Sunday usually? 
    // Let's check RtcDriver or assume 0-6.
    // In connectionManager: dt.week = timeinfo.tm_wday (0-6, Sunday=0)
    int currentDayBit = 1 << now.week; 
    
    std::vector<TodoConfig> dayTodos;
    for (const auto& item : todos) {
        if (item.enabled && (item.weekDays & currentDayBit)) {
            dayTodos.push_back(item);
        }
    }
    
    // Implement logic: 1 past, 1 current, 1-2 future.
    // Or simplified: show 3 items around current time.
    // Let's find index of "next" item (first item > now)
    
    int currentMinutes = now.hour * 60 + now.minute;
    
    int nextIdx = -1;
    for (int i = 0; i < dayTodos.size(); i++) {
        int itemMinutes = dayTodos[i].hour * 60 + dayTodos[i].minute;
        if (itemMinutes >= currentMinutes) {
            nextIdx = i;
            break;
        }
    }
    
    // Strategy:
    // If we have items today:
    // Try to pick [nextIdx - 1, nextIdx, nextIdx + 1]
    // If nextIdx == -1 (all past), show last 3?
    // If nextIdx == 0 (all future), show first 3.
    
    int startIdx = 0;
    if (nextIdx == -1) {
        // All past, show last 3
        startIdx = max(0, (int)dayTodos.size() - 3);
    } else {
        // We have a future item at nextIdx.
        // We want at least 1 past if possible.
        if (nextIdx > 0) {
            startIdx = nextIdx - 1; 
        } else {
            startIdx = 0;
        }
        
        // Adjust if we are at the end
        if (startIdx + 3 > dayTodos.size()) {
            startIdx = max(0, (int)dayTodos.size() - 3);
        }
    }
    
    int count = 0;
    for (int i = startIdx; i < dayTodos.size() && count < 3; i++) {
        const auto& item = dayTodos[i];
        TodoItem uiItem;
        uiItem.time = formatTime(item.hour, item.minute);
        uiItem.content = item.content;
        uiItem.highPriority = item.highPriority;
        uiItem.countdown = formatCountdown(now, item.hour, item.minute);
        result.push_back(uiItem);
        count++;
    }
    
    return result;
}

String TodoManager::formatTime(int h, int m) {
    char buf[6];
    sprintf(buf, "%02d:%02d", h, m);
    return String(buf);
}

String TodoManager::formatCountdown(const DateTime& now, int targetH, int targetM) {
    int nowMins = now.hour * 60 + now.minute;
    int targetMins = targetH * 60 + targetM;
    int diff = targetMins - nowMins;
    
    if (diff < 0) return "--"; // Passed
    if (diff > 999) return "999+";
    
    // If diff is huge, maybe show hours? 
    // Req says "-45m".
    // Let's simplified to "XXm" or "XhYY"
    
    if (diff < 60) {
        return "-" + String(diff) + "m";
    } else {
        int h = diff / 60;
        int m = diff % 60;
        char buf[8];
        sprintf(buf, "-%dh%02d", h, m);
        return String(buf);
    }
}
