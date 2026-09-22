#include "Display.h"

#include <TFT_eSPI.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "Config.h"

namespace {

TFT_eSPI tft;

// --- Touch (XPT2046, via TFT_eSPI's raw functions) --------------------------
// TFT_eSPI's own low-level getTouchRawZ()/getTouchRaw() communicate over
// the SAME HSPI peripheral instance the display already uses (touch and
// display share physical bus wires on this board, only CS differs) — no
// competing hardware peripheral, so no bus-ownership conflict. Confirmed
// working: these raw reads reliably returned real, varying pressure/
// position data in testing. The bug was entirely in TFT_eSPI's higher-level
// getTouch()/validTouch(), which rejects a reading unless two samples taken
// ~3ms apart agree within 20 raw units — too strict for this panel's noise,
// so it returned false almost always even when raw data was clearly good.
// Fix: read the raw functions directly and do our own, more tolerant
// outlier rejection (best-two-of-three samples, same idea as
// PaulStoffregen's XPT2046_Touchscreen library) instead of TFT_eSPI's
// consecutive-pair check. Two hardware-peripheral-based approaches
// (TFT_eSPI's own touch, and a separate XPT2046_Touchscreen instance on
// VSPI) plus one bit-banged rewrite were tried and ruled out first — see
// git history if revisiting this.
//
// That got real communication working, but the Z-threshold check alone
// turned out to false-trigger from just picking the board up — handling
// the panel couples enough noise into the analog X/Y/Z lines to look like
// a light press. The XPT2046's PENIRQ pin (wired here as PIN_TOUCH_IRQ) is
// a hardware comparator that only pulls low on genuine resistive contact
// between the panel's layers, so gate every read behind it first — noise
// picked up without actual contact won't assert this line.
constexpr int TOUCH_Z_THRESHOLD = 500;

// Raw range from TFT_eSPI's getTouchRaw() is ~13-bit (0..8191). Mapped
// empirically against the physical board: X follows screen X directly
// (min ~20 at the left edge, ~8191 at the right); Y is inverted (~8191 at
// the top, ~0 at the bottom).
constexpr int TOUCH_RAW_X_MIN = 20;
constexpr int TOUCH_RAW_X_MAX = 8191;
constexpr int TOUCH_RAW_Y_MIN = 0;
constexpr int TOUCH_RAW_Y_MAX = 8191;

int16_t bestTwoAvg(int16_t a, int16_t b, int16_t c) {
  int16_t dab = abs(a - b), dac = abs(a - c), dcb = abs(c - b);
  if (dab <= dac && dab <= dcb) return (a + b) / 2;
  if (dac <= dab && dac <= dcb) return (a + c) / 2;
  return (c + b) / 2;
}

// Returns true and fills outX/outY (raw ADC units) if currently pressed
// above the noise floor.
bool touchReadRaw(int16_t& outX, int16_t& outY) {
  // Hardware gate first: PENIRQ (active low) only asserts on real contact.
  // Skip the SPI transaction entirely otherwise — cheap and avoids feeding
  // noise-only Z spikes into the rest of the pipeline.
  if (digitalRead(PIN_TOUCH_IRQ) != LOW) return false;

  if (tft.getTouchRawZ() < TOUCH_Z_THRESHOLD) return false;

  uint16_t xs[3], ys[3];
  for (int i = 0; i < 3; i++) {
    tft.getTouchRaw(&xs[i], &ys[i]);
  }

  outX = bestTwoAvg(xs[0], xs[1], xs[2]);
  outY = bestTwoAvg(ys[0], ys[1], ys[2]);
  return true;
}

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

  pinMode(PIN_TOUCH_IRQ, INPUT);

  drawChrome();
}

void Display::drawChrome() {
  // Full clear first: this is also what wipes the info screen's leftovers
  // when returning from it. showStatus()/showLiveTemperature()/showChart()
  // each only clear their own region, not the y-axis label margin to the
  // chart's left, so without this a closed info screen left stray text
  // behind there.
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(CHART_X - 1, CHART_Y - 1, CHART_W + 2, CHART_H + 2, TFT_DARKGREY);
  drawGear(GEAR_X + GEAR_SIZE / 2, GEAR_Y + GEAR_SIZE / 2, GEAR_SIZE / 2 - 2, TFT_LIGHTGREY);
}

bool Display::readTouch(int16_t& x, int16_t& y) {
  int16_t rx, ry;
  if (!touchReadRaw(rx, ry)) return false;

  long xx = map(rx, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, 0, SCREEN_W - 1);
  long yy = map(ry, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, 0, SCREEN_H - 1);
  yy = SCREEN_H - 1 - yy;  // raw Y is inverted on this panel

  x = static_cast<int16_t>(constrain(xx, 0, SCREEN_W - 1));
  y = static_cast<int16_t>(constrain(yy, 0, SCREEN_H - 1));
  return true;
}

bool Display::isInGearZone(int16_t x, int16_t y) {
  return x >= GEAR_X - GEAR_HIT_MARGIN && x <= GEAR_X + GEAR_SIZE + GEAR_HIT_MARGIN &&
         y >= GEAR_Y - GEAR_HIT_MARGIN && y <= GEAR_Y + GEAR_SIZE + GEAR_HIT_MARGIN;
}

void Display::debugTouchOverlay() {
  int16_t rx, ry;
  if (!touchReadRaw(rx, ry)) return;
  Serial.printf("[touch] raw x=%d y=%d\n", rx, ry);
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

// Rolling 24h chart window — see CHART_WINDOW_SECONDS in Config.h.
constexpr int CHART_WINDOW_HOURS = CHART_WINDOW_SECONDS / 3600;
constexpr int CHART_TICK_HOURS = 4;

namespace {

// Hour gridlines + "-24" .. "0" labels — independent of whether there's
// any data yet, so the axis is always legible instead of only appearing
// once the chart has enough samples to plot.
void drawTimeAxis() {
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  for (int h = 0; h <= CHART_WINDOW_HOURS; h += CHART_TICK_HOURS) {
    int x = CHART_X + static_cast<int>((float)h / CHART_WINDOW_HOURS * (CHART_W - 1));
    tft.drawFastVLine(x, CHART_Y, CHART_H, TFT_NAVY);
    tft.setCursor(x - 6, CHART_Y + CHART_H + 2);
    tft.print(h - CHART_WINDOW_HOURS);
  }
}

}  // namespace

void Display::showChart(const HistorySample* samples, size_t count,
                         uint32_t nowEpoch, uint32_t windowSeconds) {
  tft.fillRect(CHART_X, CHART_Y, CHART_W, CHART_H, TFT_BLACK);
  drawTimeAxis();

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
