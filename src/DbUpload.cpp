#include "DbUpload.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "Config.h"

bool DbUpload::upload(uint32_t epoch, float tempC) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  if (!http.begin(DB_BRIDGE_URL)) return false;
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(2000);

  char body[64];
  snprintf(body, sizeof(body), "{\"epoch\":%lu,\"temp_c\":%.2f}",
           static_cast<unsigned long>(epoch), tempC);

  int code = http.POST(reinterpret_cast<uint8_t*>(body), strlen(body));
  http.end();

  if (code < 200 || code >= 300) {
    Serial.printf("[DbUpload] failed, HTTP %d\n", code);
    return false;
  }
  return true;
}
