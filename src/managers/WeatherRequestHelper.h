#pragma once

#include <ArduinoJson.h>
#include <functional>

bool requestWeatherApi(const char *url, const char *requestName,
                       std::function<void(JsonDocument &)> callback);
