#include "Clock.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

#include "Config.h"
#include "secrets.h"

namespace {

constexpr const char* ANCHOR_FILE = "/clock_anchor.bin";
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 60UL * 1000;
constexpr uint32_t BACKGROUND_RETRY_TIMEOUT_MS = 8000;

bool g_realSynced = false;
uint32_t g_sessionFloorEpoch = 0;  // anchor estimate this boot started from
uint32_t g_lastRetryMs = 0;

bool g_pendingCorrection = false;
uint32_t g_correctionFloor = 0;
int32_t g_correctionDelta = 0;

uint32_t loadAnchor() {
  File f = LittleFS.open(ANCHOR_FILE, "r");
  if (!f || f.size() != sizeof(uint32_t)) {
    if (f) f.close();
    return 0;
  }
  uint32_t epoch = 0;
  f.read(reinterpret_cast<uint8_t*>(&epoch), sizeof(epoch));
  f.close();
  return epoch;
}

void saveAnchor(uint32_t epoch) {
  File f = LittleFS.open(ANCHOR_FILE, "w");
  if (!f) return;
  f.write(reinterpret_cast<const uint8_t*>(&epoch), sizeof(epoch));
  f.close();
}

void seedClock(uint32_t epoch) {
  struct timeval tv;
  tv.tv_sec = static_cast<time_t>(epoch);
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

bool tryNtpSync(uint32_t timeoutMs) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      delay(250);
    }
  }
  if (WiFi.status() != WL_CONNECTED) return false;

  configTzTime(TZ_INFO, NTP_SERVER);
  struct tm timeinfo;
  return getLocalTime(&timeinfo, timeoutMs);
}

}  // namespace

void Clock::begin(uint32_t wifiTimeoutMs) {
  if (tryNtpSync(wifiTimeoutMs)) {
    g_realSynced = true;
    saveAnchor(static_cast<uint32_t>(time(nullptr)));
    return;
  }

  uint32_t anchor = loadAnchor();
  if (anchor > 0) {
    seedClock(anchor);
    g_sessionFloorEpoch = anchor;
  }
  g_realSynced = false;
  g_lastRetryMs = millis();
}

void Clock::poll() {
  if (g_realSynced) return;

  uint32_t now = millis();
  if (now - g_lastRetryMs < WIFI_RETRY_INTERVAL_MS) return;
  g_lastRetryMs = now;

  // What our free-running (anchor-seeded) clock currently believes, right
  // before we potentially overwrite it — this is the basis for the
  // retroactive correction below.
  uint32_t estimatedNow = static_cast<uint32_t>(time(nullptr));

  if (!tryNtpSync(BACKGROUND_RETRY_TIMEOUT_MS)) return;

  uint32_t real = static_cast<uint32_t>(time(nullptr));
  g_realSynced = true;
  saveAnchor(real);

  if (g_sessionFloorEpoch > 0) {
    g_pendingCorrection = true;
    g_correctionFloor = g_sessionFloorEpoch;
    g_correctionDelta = static_cast<int32_t>(real) - static_cast<int32_t>(estimatedNow);
  }
}

bool Clock::isRealTimeSynced() { return g_realSynced; }

bool Clock::hasTime() { return g_realSynced || g_sessionFloorEpoch > 0; }

bool Clock::consumeCorrection(uint32_t& floorEpoch, int32_t& deltaSeconds) {
  if (!g_pendingCorrection) return false;
  floorEpoch = g_correctionFloor;
  deltaSeconds = g_correctionDelta;
  g_pendingCorrection = false;
  return true;
}

void Clock::maybeRefreshAnchor() {
  if (g_realSynced) {
    saveAnchor(static_cast<uint32_t>(time(nullptr)));
  }
}
