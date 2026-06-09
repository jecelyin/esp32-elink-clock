#pragma once

#include <WebServer.h>
#include "TodoManager.h"

class WebManager {
public:
    WebManager(TodoManager* todoMgr);
    void begin();
    void loop();

private:
    WebServer server;
    TodoManager* todoMgr;

    void handleRoot();
    void handleGetTodos();
    void handleSaveTodos();
};
