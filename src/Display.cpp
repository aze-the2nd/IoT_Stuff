#include "Display.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "Config.h"

namespace {

TFT_eSPI tft;

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

constexpr int STATUS_Y = 2;
// Tall enough to also hold the settings gear icon alongside the status text.
constexpr int STATUS_H = 22;

constexpr int LIVE_Y = STATUS_Y + STATUS_H;
constexpr int LIVE_H = 36;

// Just wide enough for the Y-axis min/max labels ("-12".."103" at text
// size 1, ~6px/char) plus a couple px of breathing room before the border.
constexpr int CHART_X = 24;
constexpr int CHART_Y = LIVE_Y + LIVE_H + 4;
constexpr int CHART_W = SCREEN_W - CHART_X - 6;
constexpr int CHART_H = SCREEN_H - CHART_Y - 16;

float g_bucketSum[CHART_W];
uint16_t g_bucketCount[CHART_W];

constexpr int GEAR_SIZE = 20;
constexpr int GEAR_X = SCREEN_W - GEAR_SIZE - 4;
constexpr int GEAR_Y = STATUS_Y;
// Hit zone is a bit larger than the drawn icon — easier to tap accurately.
constexpr int GEAR_HIT_MARGIN = 6;

void drawGear(int cx, int cy, int r, uint16_t color) {
  tft.drawCircle(cx, cy, r, color);
  tft.fillCircle(cx, cy, r / 2, color);
  for (int i = 0; i < 8; i++) {
    float angle = i * (PI / 4.0f);
    int x1 = cx + static_cast<int>(cosf(angle) * r);
    int y1 = cy + static_cast<int>(sinf(angle) * r);
    int x2 = cx + static_cast<int>(cosf(angle) * (r + 3));
    int y2 = cy + static_cast<int>(sinf(angle) * (r + 3));
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

}  // namespace

void Display::begin() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  drawChrome();
}

void Display::drawChrome() {
  tft.drawRect(CHART_X - 1, CHART_Y - 1, CHART_W + 2, CHART_H + 2, TFT_DARKGREY);
  drawGear(GEAR_X + GEAR_SIZE / 2, GEAR_Y + GEAR_SIZE / 2, GEAR_SIZE / 2 - 2, TFT_LIGHTGREY);
}

bool Display::readTouch(int16_t& x, int16_t& y) {
  uint16_t tx, ty;
  if (tft.getTouch(&tx, &ty)) {
    x = static_cast<int16_t>(tx);
    y = static_cast<int16_t>(ty);
    return true;
  }
  return false;
}

bool Display::isInGearZone(int16_t x, int16_t y) {
  return x >= GEAR_X - GEAR_HIT_MARGIN && x <= GEAR_X + GEAR_SIZE + GEAR_HIT_MARGIN &&
         y >= GEAR_Y - GEAR_HIT_MARGIN && y <= GEAR_Y + GEAR_SIZE + GEAR_HIT_MARGIN;
}

void Display::showStatus(const char* msg) {
  // Leave the gear icon's corner untouched.
  tft.fillRect(0, STATUS_Y, GEAR_X - 4, STATUS_H, TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, STATUS_Y + 4);
  tft.print(msg);
}

void Display::showLiveTemperature(float tempC, bool sensorOk) {
  tft.fillRect(0, LIVE_Y, SCREEN_W, LIVE_H, TFT_BLACK);
  tft.setCursor(8, LIVE_Y + 2);

  if (!sensorOk || isnan(tempC)) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(2);
    tft.print("SENSOR FAULT");
    return;
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(4);
  tft.printf("%.1f", tempC);
  tft.setTextSize(2);
  tft.print(" C");
}

namespace {

// Day gridlines + "-7" .. "0" labels — independent of whether there's any
// data yet, so the axis is always legible instead of only appearing once
// the chart has enough samples to plot.
void drawDayAxis() {
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  for (uint32_t d = 0; d <= HISTORY_DAYS; d++) {
    int x = CHART_X + static_cast<int>((float)d / HISTORY_DAYS * (CHART_W - 1));
    tft.drawFastVLine(x, CHART_Y, CHART_H, TFT_NAVY);
    tft.setCursor(x - 4, CHART_Y + CHART_H + 2);
    tft.print(static_cast<int>(d) - static_cast<int>(HISTORY_DAYS));
  }
}

}  // namespace

void Display::showChart(const HistorySample* samples, size_t count,
                         uint32_t nowEpoch, uint32_t windowSeconds) {
  tft.fillRect(CHART_X, CHART_Y, CHART_W, CHART_H, TFT_BLACK);
  drawDayAxis();

  if (count < 2) {
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(CHART_X + 8, CHART_Y + CHART_H / 2);
    tft.print("collecting data...");
    return;
  }

  for (int i = 0; i < CHART_W; i++) {
    g_bucketSum[i] = 0.0f;
    g_bucketCount[i] = 0;
  }

  uint32_t windowStart = (nowEpoch > windowSeconds) ? nowEpoch - windowSeconds : 0;
  float minT = samples[0].tempC;
  float maxT = samples[0].tempC;

  for (size_t i = 0; i < count; i++) {
    const HistorySample& s = samples[i];
    if (s.tempC < minT) minT = s.tempC;
    if (s.tempC > maxT) maxT = s.tempC;

    long bucket = static_cast<long>((s.epoch - windowStart) * (uint64_t)CHART_W /
                                     windowSeconds);
    if (bucket < 0) bucket = 0;
    if (bucket >= CHART_W) bucket = CHART_W - 1;
    g_bucketSum[bucket] += s.tempC;
    g_bucketCount[bucket]++;
  }

  if (maxT - minT < 1.0f) {
    // Flat/near-flat data — give the chart a little headroom so the line
    // isn't glued to one edge.
    float mid = (minT + maxT) / 2.0f;
    minT = mid - 0.5f;
    maxT = mid + 0.5f;
  }
  float padding = (maxT - minT) * 0.1f;
  minT -= padding;
  maxT += padding;

  auto yForTemp = [&](float t) -> int {
    float frac = (t - minT) / (maxT - minT);
    return CHART_Y + CHART_H - 1 - static_cast<int>(frac * (CHART_H - 1));
  };

  // Y-axis min/max labels.
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(0, CHART_Y);
  tft.printf("%.0f", maxT);
  tft.setCursor(0, CHART_Y + CHART_H - 8);
  tft.printf("%.0f", minT);

  // Trend line, skipping gaps where a bucket has no data.
  int prevX = -1, prevY = 0;
  for (int i = 0; i < CHART_W; i++) {
    if (g_bucketCount[i] == 0) {
      prevX = -1;
      continue;
    }
    float avg = g_bucketSum[i] / g_bucketCount[i];
    int x = CHART_X + i;
    int y = yForTemp(avg);
    if (prevX >= 0) {
      tft.drawLine(prevX, prevY, x, y, TFT_CYAN);
    } else {
      tft.drawPixel(x, y, TFT_CYAN);
    }
    prevX = x;
    prevY = y;
  }
}

namespace {
void infoLine(int& y, const char* fmt, ...) {
  char buf[48];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  tft.setCursor(8, y);
  tft.print(buf);
  y += 16;
}
}  // namespace

void Display::showInfoScreen(const DeviceInfo& info) {
  tft.fillScreen(TFT_BLACK);
  drawGear(GEAR_X + GEAR_SIZE / 2, GEAR_Y + GEAR_SIZE / 2, GEAR_SIZE / 2 - 2, TFT_LIGHTGREY);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  int y = STATUS_Y + STATUS_H + 6;

  infoLine(y, "Firmware: v%s", info.fwVersion);
  infoLine(y, "Uptime: %lus", static_cast<unsigned long>(info.uptimeSeconds));
  y += 6;

  if (info.wifiConnected) {
    infoLine(y, "Wi-Fi: %s", info.wifiSsid);
    infoLine(y, "IP: %s  RSSI: %ddBm", info.ipAddress, info.rssi);
  } else {
    infoLine(y, "Wi-Fi: not connected");
  }
  infoLine(y, "Clock: %s", !info.hasTime      ? "no time reference"
                            : info.timeSynced ? "NTP synced"
                                               : "estimated (no Wi-Fi)");
  y += 6;

  infoLine(y, "Heap: %u / %u KB free", info.freeHeapBytes / 1024, info.totalHeapBytes / 1024);
  infoLine(y, "Flash: %u / %u KB used", info.usedFsBytes / 1024, info.totalFsBytes / 1024);
  infoLine(y, "History: %u / %u samples", info.historySamples, info.historyCapacity);

  if (info.lastStoreEpoch > 0) {
    time_t t = static_cast<time_t>(info.lastStoreEpoch);
    struct tm* tmInfo = localtime(&t);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", tmInfo);
    infoLine(y, "Last saved: %s", timeBuf);
  } else {
    infoLine(y, "Last saved: never");
  }

  y += 10;
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  infoLine(y, "(tap anywhere to close)");
}
