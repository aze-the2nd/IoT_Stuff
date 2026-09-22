#pragma once

#include <Arduino.h>

// Pushes stored samples to the keller_temp table via the small HTTP-to-MySQL
// bridge running on the Pi (see docker/). Best-effort: a failed upload is
// logged and dropped, it never blocks local history/display — the flash
// ring buffer (History) remains the source of truth on the device itself.
namespace DbUpload {

// Blocking HTTP POST, expected to complete in well under a second on the
// LAN. Returns true on a 2xx response. No-ops (returns false) if Wi-Fi
// isn't connected.
bool upload(uint32_t epoch, float tempC);

}  // namespace DbUpload
