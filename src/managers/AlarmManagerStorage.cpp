#include "AlarmManager.h"
#include <ArduinoJson.h>

void AlarmManager::load() {
  alarms.clear();
  if (!prefsReady) {
    return;
  }

  String json = prefs.getString("data", "[]");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.printf("Alarm load parse failed: %s\n", error.c_str());
    return;
  }

  JsonArray items = doc.as<JsonArray>();
  for (JsonObject item : items) {
    AlarmConfig alarm;
    alarm.hour = item["h"] | 7;
    alarm.minute = item["m"] | 0;
    alarm.enabled = item["e"] | true;
    alarm.weekMask = item["w"] | 0x7F;
    int repeatValue = item["r"] | static_cast<int>(inferRepeatType(alarm.weekMask));
    alarm.repeatType = static_cast<AlarmRepeatType>(repeatValue);
    alarm.ringtone = item["s"] | "spiffs:/alarm.mp3";
    alarms.push_back(sanitizeAlarm(alarm));
  }
}

bool AlarmManager::save() {
  if (!prefsReady) {
    return false;
  }

  JsonDocument doc;
  JsonArray items = doc.to<JsonArray>();
  for (size_t i = 0; i < alarms.size(); ++i) {
    JsonObject item = items.add<JsonObject>();
    item["h"] = alarms[i].hour;
    item["m"] = alarms[i].minute;
    item["e"] = alarms[i].enabled;
    item["r"] = alarms[i].repeatType;
    item["w"] = alarms[i].weekMask;
    item["s"] = alarms[i].ringtone;
  }

  String json;
  serializeJson(doc, json);
  return prefs.putString("data", json) > 0;
}
