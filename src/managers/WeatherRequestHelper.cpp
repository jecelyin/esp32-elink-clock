#include "WeatherRequestHelper.h"
#include <ArduinoUZlib.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>

namespace {
constexpr uint32_t WEATHER_HTTP_TIMEOUT_MS = 8000UL;
constexpr uint32_t WEATHER_POLL_DELAY_MS = 10UL;
constexpr char WEATHER_API_KEY[] = "ba7d575ae307406f9455efd0b11abe18";

bool deserializePayload(const String &encoding,
                        const std::vector<uint8_t> &payload,
                        JsonDocument &doc) {
  if (payload.empty()) {
    Serial.println("Weather payload is empty");
    return false;
  }

  if (encoding.indexOf("gzip") > -1) {
    uint8_t *buffer = nullptr;
    uint32_t size = 0;
    // ArduinoUZlib 旧接口错误地要求可写入输入缓冲区，这里传入 vector
    // 的底层地址，但逻辑上仍按只读输入使用。
    ArduinoUZlib::decompress(const_cast<uint8_t *>(payload.data()),
                             payload.size(), buffer, size);

    if (buffer == nullptr || size == 0) {
      Serial.println("Weather gzip decompress failed");
      return false;
    }

    DeserializationError error = deserializeJson(doc, buffer, size);
    free(buffer);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return false;
    }
    return true;
  }

  DeserializationError error =
      deserializeJson(doc, payload.data(), payload.size());
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }
  return true;
}

bool readResponsePayload(HTTPClient &http, std::vector<uint8_t> &payload) {
  NetworkClient *stream = http.getStreamPtr();
  if (stream == nullptr) {
    Serial.println("Weather stream is null");
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength > 0) {
    payload.reserve(contentLength);
  }

  uint32_t lastDataTime = millis();
  while (http.connected() || stream->available()) {
    int availableBytes = stream->available();
    if (availableBytes <= 0) {
      if (!stream->connected() && !http.connected()) {
        break;
      }
      if (millis() - lastDataTime >= WEATHER_HTTP_TIMEOUT_MS) {
        Serial.println("Weather payload read timeout");
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(WEATHER_POLL_DELAY_MS));
      continue;
    }

    uint8_t buffer[256];
    size_t chunkSize = static_cast<size_t>(availableBytes);
    if (chunkSize > sizeof(buffer)) {
      chunkSize = sizeof(buffer);
    }

    int readLen = stream->readBytes(reinterpret_cast<char *>(buffer), chunkSize);
    if (readLen <= 0) {
      Serial.println("Weather payload read failed");
      return false;
    }

    payload.insert(payload.end(), buffer, buffer + readLen);
    lastDataTime = millis();
  }

  return !payload.empty();
}

bool beginWeatherRequest(HTTPClient &http, WiFiClientSecure &client,
                         const char *url, const char *requestName) {
  if (!http.begin(client, url)) {
    Serial.printf("%s begin failed: %s\n", requestName, url);
    return false;
  }

  // 强制请求非复用连接，避免在 ESP32 的 TLS 连接回收边界上留下悬空 socket。
  http.useHTTP10(true);
  http.setTimeout(WEATHER_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.addHeader("X-QW-Api-Key", WEATHER_API_KEY);
  http.addHeader("Accept-Encoding", "identity");

  const char *headers[] = {"Content-Encoding"};
  http.collectHeaders(headers, 1);
  return true;
}

bool validateHttpResponse(HTTPClient &http, const char *requestName) {
  int httpCode = http.GET();
  if (httpCode <= 0) {
    Serial.printf("%s HTTP GET failed, error: %s\n", requestName,
                  http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("%s HTTP code: %d\n", requestName, httpCode);
    http.end();
    return false;
  }

  return true;
}
} // namespace

bool requestWeatherApi(const char *url, const char *requestName,
                       std::function<void(JsonDocument &)> callback) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("%s skipped: WiFi disconnected\n", requestName);
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!beginWeatherRequest(http, client, url, requestName)) {
    return false;
  }

  if (!validateHttpResponse(http, requestName)) {
    return false;
  }

  std::vector<uint8_t> payload;
  if (!readResponsePayload(http, payload)) {
    Serial.printf("%s payload read failed\n", requestName);
    http.end();
    return false;
  }

  JsonDocument doc;
  if (!deserializePayload(http.header("Content-Encoding"), payload, doc)) {
    Serial.printf("%s payload parse failed\n", requestName);
    http.end();
    return false;
  }

  if (callback) {
    callback(doc);
  }

  http.end();
  return true;
}
