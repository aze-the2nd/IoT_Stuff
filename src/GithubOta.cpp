#include "GithubOta.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "Config.h"
#include "Display.h"

namespace {

bool g_checkedOnce = false;
uint32_t g_lastCheckMs = 0;

int parseVersionInt(const char* s) {
  while (*s && !isdigit(static_cast<unsigned char>(*s))) s++;
  return atoi(s);
}

// Fetches the latest release's tag + firmware.bin asset URL. Returns true
// and fills the outputs on success.
bool fetchLatestRelease(String& outTag, String& outAssetUrl) {
  WiFiClientSecure client;
  // GitHub's API/CDN certs rotate over time and aren't worth pinning for a
  // hobby device on a trusted home network — traffic is still encrypted,
  // just not server-identity-verified. Revisit if this ever leaves the LAN.
  client.setInsecure();

  HTTPClient https;
  String url = String("https://api.github.com/repos/") + GITHUB_OWNER + "/" + GITHUB_REPO +
               "/releases/latest";
  if (!https.begin(client, url)) return false;
  https.addHeader("User-Agent", GITHUB_REPO);  // GitHub API rejects requests with no UA
  https.addHeader("Accept", "application/vnd.github+json");

  int code = https.GET();
  if (code != HTTP_CODE_OK) {
    https.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream());
  https.end();
  if (err) return false;

  outTag = doc["tag_name"] | "";
  for (JsonObject asset : doc["assets"].as<JsonArray>()) {
    const char* name = asset["name"] | "";
    if (String(name) == GITHUB_ASSET_NAME) {
      outAssetUrl = asset["browser_download_url"] | "";
      break;
    }
  }
  return outTag.length() > 0 && outAssetUrl.length() > 0;
}

void performUpdate(const String& assetUrl) {
  Display::showStatus("GitHub OTA: downloading update...");

  WiFiClientSecure client;
  client.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // release assets 302 to a CDN

  t_httpUpdate_return ret = httpUpdate.update(client, assetUrl);
  switch (ret) {
    case HTTP_UPDATE_FAILED: {
      char buf[64];
      snprintf(buf, sizeof(buf), "GitHub OTA failed (%d): %s", httpUpdate.getLastError(),
               httpUpdate.getLastErrorString().c_str());
      Display::showStatus(buf);
      break;
    }
    case HTTP_UPDATE_NO_UPDATES:
      Display::showStatus("GitHub OTA: no update delivered");
      break;
    case HTTP_UPDATE_OK:
      // Device reboots itself on success; this line normally never runs.
      Display::showStatus("GitHub OTA: rebooting...");
      break;
  }
}

}  // namespace

void GithubOta::poll() {
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (g_checkedOnce && now - g_lastCheckMs < GITHUB_OTA_CHECK_INTERVAL_MS) return;
  g_checkedOnce = true;
  g_lastCheckMs = now;

  Serial.println("[GithubOta] checking latest release...");
  String tag, assetUrl;
  if (!fetchLatestRelease(tag, assetUrl)) {
    Serial.println("[GithubOta] check failed (no release / network error)");
    return;
  }
  Serial.printf("[GithubOta] latest tag=%s current=%s\n", tag.c_str(), FW_VERSION);

  if (parseVersionInt(tag.c_str()) > parseVersionInt(FW_VERSION)) {
    performUpdate(assetUrl);
  }
}
