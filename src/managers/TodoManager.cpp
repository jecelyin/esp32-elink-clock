#include "TodoManager.h"

TodoManager::TodoManager() {}

void TodoManager::begin() {
    prefs.begin("todos", false);
    load();
}

void TodoManager::load() {
    todos.clear();
    String json = prefs.getString("data", "[]");
    
    JsonDocument doc;
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

bool TodoManager::save() {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& item : todos) {
        JsonObject obj = array.add<JsonObject>();
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
    return prefs.putString("data", json) > 0;
}

String TodoManager::getTodosJSON() {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& item : todos) {
        JsonObject obj = array.add<JsonObject>();
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

bool TodoManager::saveTodosFromJSON(const String& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error || !doc.is<JsonArray>() || doc.size() > 20) {
        Serial.printf("Todo config parse failed: %s\n", error.c_str());
        return false;
    }

    std::vector<TodoConfig> updated;
    JsonArray array = doc.as<JsonArray>();
    int fallbackId = millis();
    for (JsonObject obj : array) {
        TodoConfig item;
        if (!parseTodo(obj, item, fallbackId++)) {
            return false;
        }
        updated.push_back(item);
    }

    std::sort(updated.begin(), updated.end(), [](const TodoConfig& a, const TodoConfig& b) {
        if (a.hour != b.hour) return a.hour < b.hour;
        return a.minute < b.minute;
    });

    std::vector<TodoConfig> previous = todos;
    todos = updated;
    if (save()) {
        return true;
    }
    todos = previous;
    return false;
}

bool TodoManager::parseTodo(JsonObject obj, TodoConfig &item, int fallbackId) {
    int hour = obj["h"] | -1;
    int minute = obj["m"] | -1;
    String content = obj["c"] | "";
    int weekDays = obj["w"] | 0;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        weekDays < 0 || weekDays > 0x7F || content.length() > 64) {
        return false;
    }

    item.id = obj["id"] | fallbackId;
    if (item.id == 0) {
        item.id = fallbackId;
    }
    item.hour = hour;
    item.minute = minute;
    item.content = content;
    item.highPriority = obj["p"] | false;
    item.weekDays = static_cast<uint8_t>(weekDays);
    item.enabled = obj["e"] | true;
    return true;
}

std::vector<TodoItem> TodoManager::getVisibleTodos(const DateTime& now) {
    std::vector<TodoItem> result;
    std::vector<TodoConfig> dayTodos = getDayTodos(now);
    if (dayTodos.empty()) {
        return result;
    }

    int startIndex = findVisibleStart(dayTodos, now);
    for (int i = startIndex;
         i < static_cast<int>(dayTodos.size()) && result.size() < 3; ++i) {
        result.push_back(buildVisibleItem(dayTodos[i], now));
    }
    return result;
}

std::vector<TodoConfig> TodoManager::getDayTodos(const DateTime& now) const {
    std::vector<TodoConfig> result;
    uint8_t currentDayBit = static_cast<uint8_t>(1 << now.week);
    for (const TodoConfig& item : todos) {
        if (item.enabled && (item.weekDays & currentDayBit)) {
            result.push_back(item);
        }
    }
    return result;
}

int TodoManager::findVisibleStart(const std::vector<TodoConfig>& dayTodos,
                                  const DateTime& now) const {
    int currentMinutes = now.hour * 60 + now.minute;
    int nextIndex = -1;
    for (int i = 0; i < static_cast<int>(dayTodos.size()); ++i) {
        int itemMinutes = dayTodos[i].hour * 60 + dayTodos[i].minute;
        if (itemMinutes >= currentMinutes) {
            nextIndex = i;
            break;
        }
    }

    // 关键逻辑：首页固定展示最多三项，并尽量保留一项已过事项作为上下文。
    if (nextIndex < 0) {
        return max(0, static_cast<int>(dayTodos.size()) - 3);
    }
    int startIndex = nextIndex > 0 ? nextIndex - 1 : 0;
    return min(startIndex, max(0, static_cast<int>(dayTodos.size()) - 3));
}

TodoItem TodoManager::buildVisibleItem(const TodoConfig& item,
                                       const DateTime& now) {
    TodoItem result;
    result.time = formatTime(item.hour, item.minute);
    result.content = item.content;
    result.highPriority = item.highPriority;
    result.countdown = formatCountdown(now, item.hour, item.minute);
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
