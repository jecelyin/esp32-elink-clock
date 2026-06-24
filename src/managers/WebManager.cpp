#include "WebManager.h"
#include "ConfigPortal.h"
#include "SystemWebPage.h"
#include <ArduinoJson.h>

WebManager::WebManager(TodoManager *todo, AlarmManager *alarm,
                       ConfigManager *config, SDCardDriver *sd,
                       ConnectionManager *conn, WeatherManager *weather)
    : server(ConfigPortal::SYSTEM_WEB_PORT), todoMgr(todo), alarmMgr(alarm),
      configMgr(config), sd(sd), conn(conn), weatherMgr(weather) {}

void WebManager::begin() {
  registerRoutes();
}

void WebManager::loop() {
  // 关键逻辑：业务 WebServer 只在用户主动开启“系统设置”热点时监听，
  // 避免设备连接家庭 WiFi 同步天气期间把配置接口暴露到局域网。
  if (conn != nullptr && conn->isSystemPortalActive()) {
    if (!serverStarted) {
      server.begin();
      serverStarted = true;
      Serial.printf("System web server started on port %u\n",
                    ConfigPortal::SYSTEM_WEB_PORT);
    }
    server.handleClient();
    return;
  }
  if (serverStarted) {
    abortUpload();
    server.stop();
    serverStarted = false;
    Serial.println("System web server stopped");
  }
}

void WebManager::registerRoutes() {
  server.on("/", HTTP_GET,
            [this]() { if (authorizeRequest()) handleRoot(); });
  server.on("/api/todos", HTTP_GET,
            [this]() { if (authorizeRequest()) handleGetTodos(); });
  server.on("/api/todos", HTTP_POST,
            [this]() { if (authorizeRequest()) handleSaveTodos(); });
  server.on("/api/alarms", HTTP_GET,
            [this]() { if (authorizeRequest()) handleGetAlarms(); });
  server.on("/api/alarms", HTTP_POST,
            [this]() { if (authorizeRequest()) handleSaveAlarms(); });
  server.on("/api/radio", HTTP_GET,
            [this]() { if (authorizeRequest()) handleGetRadio(); });
  server.on("/api/radio", HTTP_POST,
            [this]() { if (authorizeRequest()) handleSaveRadio(); });
  server.on("/api/api-settings", HTTP_GET,
            [this]() { if (authorizeRequest()) handleGetApiSettings(); });
  server.on("/api/api-settings", HTTP_POST,
            [this]() { if (authorizeRequest()) handleSaveApiSettings(); });
  server.on("/api/files", HTTP_GET,
            [this]() { if (authorizeRequest()) handleListFiles(); });
  server.on("/api/files/download", HTTP_GET,
            [this]() { if (authorizeRequest()) handleDownloadFile(); });
  server.on("/api/files/rename", HTTP_POST,
            [this]() { if (authorizeRequest()) handleRenameFile(); });
  server.on("/api/files/trash", HTTP_POST,
            [this]() { if (authorizeRequest()) handleTrashFile(); });
  server.on("/api/ringtones", HTTP_GET,
            [this]() { if (authorizeRequest()) handleGetRingtones(); });
  server.on("/api/files/upload", HTTP_POST,
            [this]() { if (authorizeRequest()) handleUploadDone(); },
            [this]() { if (isSystemClient()) handleFileUpload(); });
  server.onNotFound(
      [this]() {
        if (authorizeRequest())
          sendResult(404, false, "Resource not found");
      });
}

void WebManager::handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", SYSTEM_WEB_PAGE);
}

void WebManager::handleGetTodos() {
  sendJson(200, todoMgr->getTodosJSON());
}

void WebManager::handleSaveTodos() {
  if (!server.hasArg("plain") ||
      !todoMgr->saveTodosFromJSON(server.arg("plain"))) {
    sendResult(400, false, "Todo data is invalid");
    return;
  }
  sendResult(200, true, "Todo saved");
}

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

void WebManager::handleGetRadio() {
  JsonDocument doc;
  doc["step"] = configMgr->config.radio_seek_step;
  doc["threshold"] = configMgr->config.radio_seek_threshold;
  doc["bass"] = configMgr->config.radio_bass_boost;
  doc["mono"] = configMgr->config.radio_force_mono;
  doc["softMute"] = configMgr->config.radio_soft_mute;
  JsonArray stations = doc["stations"].to<JsonArray>();
  for (int i = 0; i < RADIO_PRESET_COUNT; ++i) {
    if (configMgr->config.radio_presets[i] == 0)
      continue;
    JsonObject item = stations.add<JsonObject>();
    item["frequency"] = configMgr->config.radio_presets[i];
    item["name"] =
        configMgr->getRadioName(configMgr->config.radio_presets[i]);
  }
  String json;
  serializeJson(doc, json);
  sendJson(200, json);
}

void WebManager::handleSaveRadio() {
  JsonDocument doc;
  if (!parseBody(doc)) {
    sendResult(400, false, "Radio data is invalid");
    return;
  }
  int step = doc["step"] | 10;
  int threshold = doc["threshold"] | 8;
  if ((step != 10 && step != 20 && step != 50 && step != 100) ||
      threshold < 0 || threshold > 15) {
    sendResult(400, false, "Radio range is invalid");
    return;
  }
  if (!doc["stations"].is<JsonArray>() || doc["stations"].size() > 50) {
    sendResult(400, false, "Station names are invalid");
    return;
  }
  JsonArray stations = doc["stations"].as<JsonArray>();
  for (JsonObject item : stations) {
    uint16_t frequency = item["frequency"] | 0;
    String name = item["name"] | "";
    if (frequency < 8750 || frequency > 10800 || frequency % 10 != 0 ||
        !isValidStationName(name)) {
      sendResult(400, false, "Station name is invalid");
      return;
    }
  }
  configMgr->config.radio_seek_step = step;
  configMgr->config.radio_seek_threshold = threshold;
  configMgr->config.radio_bass_boost = doc["bass"] | false;
  configMgr->config.radio_force_mono = doc["mono"] | false;
  configMgr->config.radio_soft_mute = doc["softMute"] | true;
  for (JsonObject item : stations) {
    uint16_t frequency = item["frequency"] | 0;
    String name = item["name"] | "";
    if (!configMgr->setRadioName(frequency, name)) {
      sendResult(400, false, "Station name storage is full");
      return;
    }
  }
  if (!configMgr->saveRadioConfiguration()) {
    sendResult(500, false, "Radio settings could not be saved");
    return;
  }
  sendResult(200, true, "Radio settings saved");
}

void WebManager::handleGetApiSettings() {
  JsonDocument doc;
  // Token 属于敏感配置，读取接口只返回是否已配置，绝不回传原文。
  doc["weatherConfigured"] = configMgr->getWeatherApiToken().length() > 0;
  doc["holidayConfigured"] = configMgr->getHolidayApiToken().length() > 0;
  String json;
  serializeJson(doc, json);
  sendJson(200, json);
}

void WebManager::handleSaveApiSettings() {
  JsonDocument doc;
  if (!parseBody(doc)) {
    sendResult(400, false, "API settings are invalid");
    return;
  }
  String weather = doc["weatherToken"] | "";
  String previousWeather = configMgr->getWeatherApiToken();
  String holiday = doc["holidayToken"] | "";
  String previousHoliday = configMgr->getHolidayApiToken();
  if (weather.length() > 160 || holiday.length() > 160) {
    sendResult(400, false, "API token is too long");
    return;
  }
  bool clearWeather = doc["clearWeather"] | false;
  bool clearHoliday = doc["clearHoliday"] | false;
  bool updateWeather = clearWeather || weather.length() > 0;
  bool updateHoliday = clearHoliday || holiday.length() > 0;
  String nextWeather = clearWeather ? "" : weather;
  String nextHoliday = clearHoliday ? "" : holiday;
  if (!configMgr->updateApiTokens(nextWeather, updateWeather, nextHoliday,
                                  updateHoliday)) {
    sendResult(500, false, "API settings could not be saved");
    return;
  }
  if (previousWeather != configMgr->getWeatherApiToken() &&
      weatherMgr != nullptr) {
    // 关键逻辑：天气 Token 更新后立刻解除重试节流，
    // 避免刚修正的新 Key 被旧失败窗口继续压住。
    weatherMgr->resetUpdateSchedule();
  }
  if (previousHoliday != configMgr->getHolidayApiToken())
    alarmMgr->resetHolidayFetchState();
  sendResult(200, true, "API settings saved");
}

void WebManager::handleListFiles() {
  String path = getRequestPath("path", "/");
  if (!isSafePath(path) || !mountSD()) {
    sendResult(400, false, "SD card or path is unavailable");
    return;
  }
  std::vector<FileInfo> files = sd->listDir(path.c_str());
  JsonDocument doc;
  doc["path"] = path;
  JsonArray items = doc["items"].to<JsonArray>();
  for (const FileInfo &file : files) {
    JsonObject item = items.add<JsonObject>();
    item["name"] = file.name;
    item["directory"] = file.isDirectory;
    item["size"] = file.size;
  }
  String json;
  serializeJson(doc, json);
  sd->end();
  sendJson(200, json);
}

void WebManager::handleDownloadFile() {
  String path = getRequestPath("path", "");
  if (!isSafePath(path) || !mountSD()) {
    sendResult(400, false, "File path is invalid");
    return;
  }
  File file = sd->open(path.c_str(), FILE_READ);
  if (!file || file.isDirectory()) {
    if (file)
      file.close();
    sd->end();
    sendResult(404, false, "File not found");
    return;
  }
  server.streamFile(file, getContentType(path));
  file.close();
  sd->end();
}

void WebManager::handleRenameFile() {
  JsonDocument doc;
  if (!parseBody(doc) || !mountSD()) {
    sendResult(400, false, "Rename request is invalid");
    return;
  }
  String from = doc["from"] | "";
  String to = doc["to"] | "";
  bool ok = isSafePath(from) && isSafePath(to) && sd->exists(from.c_str());
  if (ok && from == to) {
    sd->end();
    sendResult(200, true, "File name unchanged");
    return;
  }
  if (ok && sd->exists(to.c_str())) {
    sd->end();
    sendResult(409, false, "Target file already exists");
    return;
  }
  if (ok) {
    ok = sd->rename(from.c_str(), to.c_str());
  }
  sd->end();
  sendResult(ok ? 200 : 400, ok, ok ? "File renamed" : "Rename failed");
}

void WebManager::handleTrashFile() {
  JsonDocument doc;
  if (!parseBody(doc) || !mountSD()) {
    sendResult(400, false, "Trash request is invalid");
    return;
  }
  String path = doc["path"] | "";
  bool ok = isSafePath(path) && path != "/" && path != "/.trash" &&
            sd->softDelete(path.c_str());
  sd->end();
  sendResult(ok ? 200 : 400, ok,
             ok ? "Moved to .trash" : "Move to trash failed");
}

void WebManager::handleGetRingtones() {
  JsonDocument doc;
  JsonArray items = doc.to<JsonArray>();
  items.add("spiffs:/alarm.mp3");
  if (mountSD()) {
    for (const FileInfo &file : sd->listDir("/")) {
      if (!file.isDirectory && file.name.endsWith(".mp3")) {
        String path = file.name.startsWith("/") ? file.name
                                                : "/" + file.name;
        items.add("sd:" + path);
      }
    }
    sd->end();
  }
  String json;
  serializeJson(doc, json);
  sendJson(200, json);
}

void WebManager::handleFileUpload() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadStarted = true;
    uploadFailed = false;
    uploadTargetPath = "";
    uploadFailed = !mountSD();
    String directory = getRequestPath("path", "/");
    String filename = upload.filename;
    filename.replace("\\", "");
    filename.replace("/", "");
    String target = directory == "/" ? "/" + filename
                                     : directory + "/" + filename;
    uploadTargetPath = target;
    uploadFailed = uploadFailed || !isSafePath(target);
    // 覆盖文件前先移动到 .trash，上传失败时仍能恢复旧版本。
    if (!uploadFailed && sd->exists(target.c_str()))
      uploadFailed = !sd->softDelete(target.c_str());
    if (!uploadFailed)
      uploadFile = sd->open(target.c_str(), FILE_WRITE);
    uploadFailed = uploadFailed || !uploadFile;
  } else if (upload.status == UPLOAD_FILE_WRITE && !uploadFailed) {
    if (upload.totalSize > 32UL * 1024UL * 1024UL) {
      uploadFailed = true;
      return;
    }
    uploadFailed =
        uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END ||
             upload.status == UPLOAD_FILE_ABORTED) {
    if (upload.status == UPLOAD_FILE_ABORTED)
      uploadFailed = true;
    if (uploadFile)
      uploadFile.close();
    if (uploadFailed && sd->isMounted() && uploadTargetPath.length() > 0 &&
        sd->exists(uploadTargetPath.c_str())) {
      sd->softDelete(uploadTargetPath.c_str());
    }
    if (sd->isMounted())
      sd->end();
  }
}

void WebManager::handleUploadDone() {
  bool success = uploadStarted && !uploadFailed;
  sendResult(success ? 200 : 400, success,
             success ? "File uploaded" : "Upload failed");
  uploadStarted = false;
  uploadFailed = false;
  uploadTargetPath = "";
}

void WebManager::abortUpload() {
  if (!uploadStarted) {
    return;
  }
  if (uploadFile) {
    uploadFile.close();
  }
  if (sd->isMounted() && uploadTargetPath.length() > 0 &&
      sd->exists(uploadTargetPath.c_str())) {
    sd->softDelete(uploadTargetPath.c_str());
  }
  if (sd->isMounted()) {
    sd->end();
  }
  uploadStarted = false;
  uploadFailed = false;
  uploadTargetPath = "";
}

bool WebManager::parseBody(JsonDocument &doc) {
  if (!server.hasArg("plain") || server.arg("plain").length() > 8192)
    return false;
  return deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok;
}

bool WebManager::authorizeRequest() {
  if (isSystemClient()) {
    return true;
  }
  sendResult(403, false, "System settings portal is not active");
  return false;
}

bool WebManager::isSystemClient() {
  if (conn == nullptr || !conn->isSystemPortalActive()) {
    return false;
  }
  IPAddress gateway = WiFi.softAPIP();
  WiFiClient client = server.client();
  IPAddress local = client.localIP();
  IPAddress remote = client.remoteIP();
  // 关键逻辑：localIP 必须等于 SoftAP 网关，才能排除 AP+STA 模式下
  // 从家庭局域网接口进入的连接；远端同时还要属于热点子网。
  return local == gateway && gateway[0] == remote[0] &&
         gateway[1] == remote[1] && gateway[2] == remote[2];
}

bool WebManager::mountSD() {
  return sd != nullptr && !alarmMgr->isRinging() && sd->begin();
}

bool WebManager::isSafePath(const String &path) const {
  return path.length() > 0 && path[0] == '/' && path.indexOf("..") < 0 &&
         path.indexOf('\\') < 0 && path.indexOf('\r') < 0 &&
         path.indexOf('\n') < 0;
}

bool WebManager::isValidStationName(const String &name) const {
  const uint8_t *bytes =
      reinterpret_cast<const uint8_t *>(name.c_str());
  size_t length = name.length();
  uint8_t characters = 0;
  for (size_t i = 0; i < length; ++characters) {
    uint8_t lead = bytes[i];
    size_t width = 0;
    if (lead < 0x80) {
      width = 1;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
      width = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      width = 3;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      width = 4;
    } else {
      return false;
    }
    if (i + width > length) {
      return false;
    }
    for (size_t offset = 1; offset < width; ++offset) {
      if ((bytes[i + offset] & 0xC0) != 0x80) {
        return false;
      }
    }
    if (width == 3 &&
        ((lead == 0xE0 && bytes[i + 1] < 0xA0) ||
         (lead == 0xED && bytes[i + 1] > 0x9F))) {
      return false;
    }
    if (width == 4 &&
        ((lead == 0xF0 && bytes[i + 1] < 0x90) ||
         (lead == 0xF4 && bytes[i + 1] > 0x8F))) {
      return false;
    }
    i += width;
  }
  return characters <= 24;
}

String WebManager::getRequestPath(const char *name,
                                  const char *fallback) {
  return server.hasArg(name) ? server.arg(name) : String(fallback);
}

String WebManager::getContentType(const String &path) const {
  if (path.endsWith(".mp3"))
    return "audio/mpeg";
  if (path.endsWith(".json"))
    return "application/json";
  if (path.endsWith(".txt"))
    return "text/plain";
  return "application/octet-stream";
}

void WebManager::sendJson(int status, const String &json) {
  server.send(status, "application/json; charset=utf-8", json);
}

void WebManager::sendResult(int status, bool ok, const char *message) {
  JsonDocument doc;
  doc["ok"] = ok;
  doc["message"] = message;
  String json;
  serializeJson(doc, json);
  sendJson(status, json);
}
