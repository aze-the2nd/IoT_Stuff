#pragma once

#include <Arduino.h>

// --- MAX31865 (RTD-to-digital) ---------------------------------------------
// Wired on the CYD's exposed VSPI header pins (SCK 18 / MISO 19 / MOSI 23 —
// free because the display runs on HSPI; see platformio.ini's
// USE_HSPI_PORT flag). Only the chip-select pin is free to choose — verify
// GPIO22 is actually broken out on your board's header (CN1/P3) and change
// here if not.
constexpr uint8_t PIN_RTD_CS = 22;

// RTD element nominal resistance at 0°C.
constexpr float RTD_NOMINAL_OHMS = 1000.0f;  // Pt-1000

// MAX31865 reference resistor on the breakout. Adafruit's PT1000 breakout
// uses 4300R — verify against your specific board's silkscreen/datasheet;
// a wrong value here shifts every reading.
constexpr float RTD_REF_OHMS = 4300.0f;

// --- Wi-Fi / time ------------------------------------------------------------
constexpr const char* NTP_SERVER = "pool.ntp.org";
// POSIX TZ string: Central European Time with automatic DST.
constexpr const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

// --- History / storage -------------------------------------------------------
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;         // live reading cadence
constexpr uint32_t STORE_INTERVAL_MS = 60UL * 1000;   // persisted-sample cadence
constexpr uint32_t HISTORY_DAYS = 7;
constexpr uint32_t HISTORY_CAPACITY = HISTORY_DAYS * 24 * 60;  // 1 sample/min
constexpr uint32_t HISTORY_WINDOW_SECONDS = HISTORY_DAYS * 24UL * 60 * 60;

constexpr const char* HISTORY_FILE = "/history.bin";
constexpr const char* HISTORY_META_FILE = "/history_meta.bin";

// If true, a live internal-die-temp fallback reading (see InternalTemp.h)
// also gets written to history whenever the RTD is unavailable — handy to
// see what the chart looks like before the RTD is wired up. Flip to false
// once real data matters: die temp is dominated by chip self-heating, not
// the room, and mixing it into the 7-day trend would be misleading.
constexpr bool STORE_INTERNAL_FALLBACK_IN_HISTORY = true;

// --- OTA ----------------------------------------------------------------------
// Bump this before tagging a GitHub release (see scripts/release.sh) — the
// device compares this against the latest release's tag to decide whether to
// self-update.
constexpr const char* FW_VERSION = "1";

// LAN OTA (ArduinoOTA, pushed from PlatformIO during development).
constexpr const char* OTA_HOSTNAME = "iot-rtd-sensor";
// OTA_PASSWORD lives in secrets.h, not here.

// GitHub-hosted pull update (checked periodically; see GithubOta).
constexpr const char* GITHUB_OWNER = "aze-the2nd";
constexpr const char* GITHUB_REPO = "IoT_Stuff";
constexpr const char* GITHUB_ASSET_NAME = "firmware.bin";
constexpr uint32_t GITHUB_OTA_CHECK_INTERVAL_MS = 24UL * 60 * 60 * 1000;  // once/day
