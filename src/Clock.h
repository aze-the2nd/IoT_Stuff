#pragma once

#include <Arduino.h>

// Wraps time(nullptr) so the rest of the app can always assume a plausible
// epoch is available, even across a reboot with no Wi-Fi at boot:
//
//   1. begin() tries NTP. On success, time is authoritative immediately.
//   2. On failure, it seeds the system clock from the last known-good
//      epoch (persisted to flash) and lets it free-run from there, so
//      history keeps recording with a "probably close" timestamp instead
//      of stopping.
//   3. poll() retries NTP in the background. Once it succeeds, it re-seeds
//      the system clock and reports the correction (see consumeCorrection)
//      so already-stored samples from the estimated period can be shifted
//      to match.
namespace Clock {

void begin(uint32_t wifiTimeoutMs = 15000);

// Call periodically from loop(); rate-limits itself internally.
void poll();

// True once this boot has an NTP-confirmed epoch (as opposed to running on
// the anchor estimate).
bool isRealTimeSynced();

// True if time(nullptr) is usable at all — either NTP-confirmed, or seeded
// from a persisted anchor. False only on a first-ever boot with no Wi-Fi
// and no prior anchor, where there's genuinely no time reference yet.
bool hasTime();

// If a background resync just corrected the clock, returns true once and
// fills in [floorEpoch, +inf) and the delta (seconds) that should be added
// to any already-stored sample with epoch >= floorEpoch. Returns false
// (and leaves the outputs untouched) otherwise.
bool consumeCorrection(uint32_t& floorEpoch, int32_t& deltaSeconds);

// Persists the current epoch as the new anchor, but only while
// isRealTimeSynced() — call this periodically (e.g. alongside each history
// write) so a future reboot-without-Wi-Fi starts from a fresh estimate.
void maybeRefreshAnchor();

}  // namespace Clock
