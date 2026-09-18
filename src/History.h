#pragma once

#include <Arduino.h>

struct HistorySample {
  uint32_t epoch;  // unix seconds, 0 = unused slot
  float tempC;
};

namespace History {

// Mounts LittleFS and opens (or creates) the fixed-size ring-buffer file.
bool begin();

// Persists one sample, overwriting the oldest slot once the ring is full.
void append(uint32_t epoch, float tempC);

// Fills out[] with samples whose epoch falls within
// [nowEpoch - windowSeconds, nowEpoch], ordered oldest-first. Returns the
// number of samples written (<= maxOut).
size_t readWindow(uint32_t nowEpoch, uint32_t windowSeconds,
                   HistorySample* out, size_t maxOut);

// Adds deltaSeconds to the epoch of every stored sample with
// epoch >= floorEpoch. Used to retroactively fix up samples that were
// timestamped from Clock's anchor estimate before a real NTP sync landed.
void shiftEpochsFrom(uint32_t floorEpoch, int32_t deltaSeconds);

// Wipes all stored samples (e.g. to discard test data). Call after begin().
void reset();

// Number of samples currently stored (<= HISTORY_CAPACITY).
uint32_t filledCount();

// LittleFS space usage in bytes, for the whole filesystem (not just the
// history file).
void filesystemUsage(uint32_t& usedBytes, uint32_t& totalBytes);

}  // namespace History
