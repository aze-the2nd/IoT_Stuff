#pragma once

// BLE GATT peripheral for setting Wi-Fi credentials over Bluetooth (see the
// contract posted to the team Talk room: service UUID
// bbf1ab36-e36e-4b35-a238-e33e32684d8f with SSID/Password/Apply/Status
// characteristics). No pairing/auth in v1 — matches this project's existing
// "reachability, not auth" threat model (the DB bridge is likewise open);
// BLE's physical proximity requirement is the practical limiter here.
namespace BleProvisioning {

void begin();

// Call periodically from loop() — only does work while a reconnect
// triggered by the Apply characteristic is in flight.
void poll();

}  // namespace BleProvisioning
