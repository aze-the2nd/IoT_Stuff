#include "Display.h"

#include <TFT_eSPI.h>
#include <math.h>

#include "Config.h"

namespace {

TFT_eSPI tft;

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;

constexpr int STATUS_Y = 2;
constexpr int STATUS_H = 16;

constexpr int LIVE_Y = STATUS_Y + STATUS_H;
constexpr int LIVE_H = 64;

constexpr int CHART_X = 34;
constexpr int CHART_Y = LIVE_Y + LIVE_H + 6;
constexpr int CHART_W = SCREEN_W - CHART_X - 6;
constexpr int CHART_H = SCREEN_H - CHART_Y - 16;

float g_bucketSum[CHART_W];
uint16_t g_bucketCount[CHART_W];

}  // namespace

void Display::begin() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.drawRect(CHART_X - 1, CHART_Y - 1, CHART_W + 2, CHART_H + 2, TFT_DARKGREY);
}

void Display::showStatus(const char* msg) {
  tft.fillRect(0, STATUS_Y, SCREEN_W, STATUS_H, TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(4, STATUS_Y + 4);
  tft.print(msg);
}

void Display::showLiveTemperature(float tempC, bool sensorOk) {
  tft.fillRect(0, LIVE_Y, SCREEN_W, LIVE_H, TFT_BLACK);
  tft.setCursor(8, LIVE_Y + 10);

  if (!sensorOk || isnan(tempC)) {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(3);
    tft.print("SENSOR FAULT");
    return;
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(5);
  tft.printf("%.1f", tempC);
  tft.setTextSize(3);
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
