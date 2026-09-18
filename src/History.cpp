#include "History.h"

#include <LittleFS.h>

#include <algorithm>

#include "Config.h"

namespace {

struct Meta {
  uint32_t nextIndex;
  uint32_t filled;
};

Meta g_meta{0, 0};

bool loadMeta() {
  File f = LittleFS.open(HISTORY_META_FILE, "r");
  if (!f || f.size() != sizeof(Meta)) {
    if (f) f.close();
    return false;
  }
  f.read(reinterpret_cast<uint8_t*>(&g_meta), sizeof(Meta));
  f.close();
  return true;
}

bool saveMeta() {
  File f = LittleFS.open(HISTORY_META_FILE, "w");
  if (!f) return false;
  f.write(reinterpret_cast<const uint8_t*>(&g_meta), sizeof(Meta));
  f.close();
  return true;
}

bool ensureHistoryFile() {
  const size_t expectedSize = sizeof(HistorySample) * HISTORY_CAPACITY;

  if (LittleFS.exists(HISTORY_FILE)) {
    File existing = LittleFS.open(HISTORY_FILE, "r");
    bool ok = existing && existing.size() == expectedSize;
    if (existing) existing.close();
    if (ok) return true;
  }

  // Missing or wrong size (e.g. HISTORY_CAPACITY changed) — (re)create,
  // zero-filled.
  File f = LittleFS.open(HISTORY_FILE, "w");
  if (!f) return false;
  HistorySample empty{0, 0.0f};
  for (uint32_t i = 0; i < HISTORY_CAPACITY; i++) {
    f.write(reinterpret_cast<const uint8_t*>(&empty), sizeof(empty));
  }
  f.close();
  return true;
}

}  // namespace

bool History::begin() {
  if (!LittleFS.begin(true)) {
    return false;
  }
  if (!ensureHistoryFile()) {
    return false;
  }
  if (!loadMeta()) {
    g_meta = {0, 0};
    saveMeta();
  }
  return true;
}

void History::append(uint32_t epoch, float tempC) {
  File f = LittleFS.open(HISTORY_FILE, "r+");
  if (!f) return;

  HistorySample sample{epoch, tempC};
  f.seek(static_cast<uint32_t>(g_meta.nextIndex * sizeof(HistorySample)));
  f.write(reinterpret_cast<const uint8_t*>(&sample), sizeof(sample));
  f.close();

  g_meta.nextIndex = (g_meta.nextIndex + 1) % HISTORY_CAPACITY;
  if (g_meta.filled < HISTORY_CAPACITY) g_meta.filled++;
  saveMeta();
}

size_t History::readWindow(uint32_t nowEpoch, uint32_t windowSeconds,
                            HistorySample* out, size_t maxOut) {
  File f = LittleFS.open(HISTORY_FILE, "r");
  if (!f) return 0;

  uint32_t cutoff = (nowEpoch > windowSeconds) ? nowEpoch - windowSeconds : 0;
  size_t count = 0;

  for (uint32_t i = 0; i < g_meta.filled && count < maxOut; i++) {
    f.seek(static_cast<uint32_t>(i * sizeof(HistorySample)));
    HistorySample sample;
    f.read(reinterpret_cast<uint8_t*>(&sample), sizeof(sample));
    if (sample.epoch != 0 && sample.epoch >= cutoff && sample.epoch <= nowEpoch) {
      out[count++] = sample;
    }
  }
  f.close();

  // Ring-buffer order isn't chronological once it wraps — sort so the
  // caller (chart renderer) can assume oldest-first.
  std::sort(out, out + count, [](const HistorySample& a, const HistorySample& b) {
    return a.epoch < b.epoch;
  });
  return count;
}

void History::shiftEpochsFrom(uint32_t floorEpoch, int32_t deltaSeconds) {
  if (deltaSeconds == 0) return;

  File f = LittleFS.open(HISTORY_FILE, "r+");
  if (!f) return;

  for (uint32_t i = 0; i < g_meta.filled; i++) {
    uint32_t offset = static_cast<uint32_t>(i * sizeof(HistorySample));
    f.seek(offset);
    HistorySample sample;
    f.read(reinterpret_cast<uint8_t*>(&sample), sizeof(sample));

    if (sample.epoch != 0 && sample.epoch >= floorEpoch) {
      sample.epoch = static_cast<uint32_t>(static_cast<int64_t>(sample.epoch) + deltaSeconds);
      f.seek(offset);
      f.write(reinterpret_cast<const uint8_t*>(&sample), sizeof(sample));
    }
  }
  f.close();
}

void History::reset() {
  LittleFS.remove(HISTORY_FILE);
  LittleFS.remove(HISTORY_META_FILE);
  g_meta = {0, 0};
  ensureHistoryFile();
  saveMeta();
}

uint32_t History::filledCount() { return g_meta.filled; }

void History::filesystemUsage(uint32_t& usedBytes, uint32_t& totalBytes) {
  usedBytes = LittleFS.usedBytes();
  totalBytes = LittleFS.totalBytes();
}
