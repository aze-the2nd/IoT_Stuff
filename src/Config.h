#pragma once

#include <Arduino.h>

// --- MAX31865 (RTD-to-digital) ---------------------------------------------
// This board revision has no general GPIO header — only two small JST
// expansion connectors ("P3": GND/IO35/IO22/IO21, "CN1": GND/IO22/IO27/
// 3.3V), giving only IO35, IO22, IO27, IO21 as candidate GPIOs.
//
// CS genuinely needs to be a real, actively-toggled GPIO here: the
// Adafruit library's readRTD() chains ~9 separate register read/write
// transactions per call (clear fault, enable bias, read/write config,
// read RTD registers, disable bias), each expecting its own CS low/high
// framing so the chip's internal shift register stays in sync. Tying CS
// permanently to GND (tried first, to avoid touching IO21) broke that
// framing and produced garbage 0x0000/0x7FFF-ish raw reads — decoded by
// the CVD polynomial as exactly -242.02°C / ~988°C, which is what gave
// this away.
//
// So IO21 — otherwise the display backlight (TFT_BL) — does double duty
// as RTD chip-select too. Idle-HIGH is the correct resting state for
// both roles (backlight on, CS deselected), so the only side effect is a
// brief (sub-millisecond-per-transaction) backlight blip during each 1
// Hz RTD read, when CS pulses low.
constexpr uint8_t PIN_RTD_CS = 21;
constexpr uint8_t PIN_RTD_MOSI = 27;
constexpr uint8_t PIN_RTD_MISO = 35;
constexpr uint8_t PIN_RTD_SCLK = 22;

// --- Touch (XPT2046) -----------------------------------------------------
// Shares the display's physical bus wires (SCLK 14 / MOSI 13 / MISO 12,
// same as TFT_MISO/MOSI/SCLK below) through TFT_eSPI's own HSPI
// peripheral — see Display.cpp, which reads it via TFT_eSPI's
// getTouchRawZ()/getTouchRaw(). Only CS/IRQ are separate pins; IRQ isn't
// currently used (polling is enough) but is wired on this board.
constexpr uint8_t PIN_TOUCH_CS = 33;
constexpr uint8_t PIN_TOUCH_IRQ = 36;

// RTD element nominal resistance at 0°C.
constexpr float RTD_NOMINAL_OHMS = 100.0f;  // Pt-100

// MAX31865 reference resistor on the breakout — directly multimeter-
// measured at 425.8R (confirms it's a Pt-100-spec board; a Pt-1000-spec
// board would use ~4300R). The silkscreen print ("431" or "437", last
// digit ambiguous) was only ever an approximation — this measured value
// is the reliable one. A wrong value here shifts every reading.
constexpr float RTD_REF_OHMS = 425.8f;

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

// How much of that stored history the chart actually displays at once —
// independent of HISTORY_DAYS/HISTORY_CAPACITY above, which is how much
// stays on flash. A rolling 24h window: as time passes the display keeps
// showing "now - 24h" to "now", scrolling forward continuously, while up
// to 7 days still accumulate in storage for later.
constexpr uint32_t CHART_WINDOW_SECONDS = 24UL * 60 * 60;

constexpr const char* HISTORY_FILE = "/history.bin";
constexpr const char* HISTORY_META_FILE = "/history_meta.bin";

// --- Database upload -----------------------------------------------------
// Small HTTP-to-MySQL bridge on the Pi (see docker/) that inserts into the
// keller_temp table. Plain HTTP, no auth — it only listens on the LAN.
constexpr const char* DB_BRIDGE_URL = "http://192.168.178.23:5005/keller_temp";

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
