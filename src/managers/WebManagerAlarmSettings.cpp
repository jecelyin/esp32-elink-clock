#include "WebManager.h"
#include <ArduinoJson.h>

void WebManager::handleGetAlarms() {
  sendJson(200, alarmMgr->getAlarmsJSON());
}

void WebManager::handleSaveAlarms() {
  if (!server.hasArg("plain") ||
      !alarmMgr->saveAlarmsFromJSON(server.arg("plain"))) {
    sendResult(400, false, "Alarm data is invalid");
    return;
  }
  sendResult(200, true, "Alarms saved");
}

void WebManager::handleGetAlarmSettings() {
  AlarmSoundSettings settings = configMgr->getAlarmSoundSettings();
  JsonDocument doc;
  doc["volume"] = settings.volume;
  doc["fadeInSeconds"] = settings.fadeInSeconds;
  String json;
  serializeJson(doc, json);
  sendJson(200, json);
}

void WebManager::handleSaveAlarmSettings() {
  JsonDocument doc;
  if (!parseBody(doc) || !doc.is<JsonObject>()) {
    sendResult(400, false, "闹钟声音设置必须是有效的 JSON 对象");
    return;
  }
  // 关键逻辑：先校验整数类型再取值，拒绝布尔值、字符串、小数和缺失字段，
  // 避免 JSON 隐式转换将错误输入保存为静音或意外关闭渐强。
  if (!doc["volume"].is<int>() || doc["volume"].as<int>() < 0 ||
      doc["volume"].as<int>() > AlarmSoundSettings::MAX_VOLUME) {
    sendResult(400, false, "闹钟音量必须是 0 到 21 之间的整数");
    return;
  }
  if (!doc["fadeInSeconds"].is<int>() ||
      doc["fadeInSeconds"].as<int>() < 0 ||
      doc["fadeInSeconds"].as<int>() >
          AlarmSoundSettings::MAX_FADE_IN_SECONDS) {
    sendResult(400, false, "闹钟渐强时长必须是 0 到 60 秒之间的整数");
    return;
  }
  AlarmSoundSettings settings;
  settings.volume = doc["volume"].as<uint8_t>();
  settings.fadeInSeconds = doc["fadeInSeconds"].as<uint8_t>();
  if (!configMgr->saveAlarmSoundSettings(settings)) {
    sendResult(500, false, "闹钟声音设置未能写入设备存储，请重试");
    return;
  }
  handleGetAlarmSettings();
}

void WebManager::handleGetRingtones() {
  JsonDocument doc;
  JsonArray items = doc.to<JsonArray>();
  for (size_t i = 0; i < AlarmRingtones::BUILTIN_COUNT; ++i) {
    JsonObject item = items.add<JsonObject>();
    item["value"] = AlarmRingtones::BUILTIN[i].value;
    item["label"] = AlarmRingtones::BUILTIN[i].label;
    item["builtin"] = true;
  }
  if (mountSD()) {
    for (const FileInfo &file : sd->listDir("/")) {
      if (!file.isDirectory && file.name.endsWith(".mp3")) {
        String path = file.name.startsWith("/") ? file.name
                                                : "/" + file.name;
        JsonObject item = items.add<JsonObject>();
        item["value"] = "sd:" + path;
        item["label"] = "SD 卡 · " + file.name;
        item["builtin"] = false;
      }
    }
    sd->end();
  }
  String json;
  serializeJson(doc, json);
  sendJson(200, json);
}
