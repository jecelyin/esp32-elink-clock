#pragma once

#include "../drivers/SDCardDriver.h"
#include "AlarmManager.h"
#include "ConfigManager.h"
#include "ConnectionManager.h"
#include "TodoManager.h"
#include "WeatherManager.h"
#include <WebServer.h>

class WebManager {
public:
  WebManager(TodoManager *todo, AlarmManager *alarm, ConfigManager *config,
             SDCardDriver *sd, ConnectionManager *conn,
             WeatherManager *weather);
  void begin();
  void loop();

private:
  WebServer server;
  TodoManager *todoMgr;
  AlarmManager *alarmMgr;
  ConfigManager *configMgr;
  SDCardDriver *sd;
  ConnectionManager *conn;
  WeatherManager *weatherMgr;
  File uploadFile;
  bool uploadFailed = false;
  bool uploadStarted = false;
  bool serverStarted = false;
  String uploadTargetPath;

  void registerRoutes();
  void handleRoot();
  void handleGetTodos();
  void handleSaveTodos();
  void handleGetAlarms();
  void handleSaveAlarms();
  void handleGetRadio();
  void handleSaveRadio();
  void handleGetApiSettings();
  void handleSaveApiSettings();
  void handleListFiles();
  void handleDownloadFile();
  void handleRenameFile();
  void handleTrashFile();
  void handleGetRingtones();
  void handleFileUpload();
  void handleUploadDone();
  void abortUpload();
  bool parseBody(JsonDocument &doc);
  bool authorizeRequest();
  bool isSystemClient();
  bool mountSD();
  bool isSafePath(const String &path) const;
  bool isValidStationName(const String &name) const;
  String getRequestPath(const char *name, const char *fallback);
  String getContentType(const String &path) const;
  void sendJson(int status, const String &json);
  void sendResult(int status, bool ok, const char *message);
};
