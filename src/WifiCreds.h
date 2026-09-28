#pragma once

#include <Arduino.h>

// Runtime-editable Wi-Fi credentials, persisted in NVS (via Preferences) so
// they survive reboots and OTA updates. On first-ever boot (empty NVS
// namespace) falls back to the compiled-in secrets.h values and seeds NVS
// with them, so the device keeps working exactly as before until someone
// actually reconfigures it over Bluetooth (see BleProvisioning).
namespace WifiCreds {

void begin();

const char* ssid();
const char* password();

// Persists new credentials to NVS and updates the in-memory values returned
// by ssid()/password() immediately. Does not itself touch the Wi-Fi
// connection — callers (BleProvisioning) trigger the reconnect separately.
void save(const char* newSsid, const char* newPassword);

}  // namespace WifiCreds
