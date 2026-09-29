#include "BleProvisioning.h"

#include <NimBLEDevice.h>
#include <WiFi.h>

#include "Config.h"
#include "WifiCreds.h"

namespace {

constexpr const char* SERVICE_UUID = "bbf1ab36-e36e-4b35-a238-e33e32684d8f";
constexpr const char* CHAR_SSID_UUID = "e2ee8fe3-eda6-4b0c-8795-5d127191a83b";
constexpr const char* CHAR_PASSWORD_UUID = "64f853c5-3007-4986-8f0d-1442969bdf42";
constexpr const char* CHAR_APPLY_UUID = "15b55d7c-c1ec-4e21-ab83-56da84ca9dbf";
constexpr const char* CHAR_STATUS_UUID = "ef3f6a85-2f63-490f-9803-bb29ba25cf64";

// Max characteristic value lengths, in UTF-8 bytes — matches the contract
// (SSID 32, Password 64) and what WifiCreds can hold. Set explicitly on the
// characteristics (rather than relying on NimBLE's 512-byte default) so an
// oversized write is rejected by the stack instead of silently accepted.
constexpr uint16_t SSID_MAX_LEN = 32;
constexpr uint16_t PASSWORD_MAX_LEN = 64;

// Requested to match the Android app's post-connect MTU request (see #946)
// — without this NimBLE falls back to its own default preferred MTU, which
// may negotiate lower than what the app asked for.
constexpr uint16_t PREFERRED_MTU = 247;

NimBLECharacteristic* g_statusChar = nullptr;

// Staging area: the app writes SSID then Password as two separate
// characteristic writes, then triggers Apply — these hold whatever's been
// written so far until Apply fires.
String g_pendingSsid;
String g_pendingPassword;

bool g_reconnectPending = false;
uint32_t g_reconnectStartMs = 0;
uint32_t g_lastHeartbeatMs = 0;
constexpr uint32_t RECONNECT_TIMEOUT_MS = 15000;
// Re-notify "connecting" at least this often so Android doesn't treat a
// quiet-but-open BLE connection as stale during the 10-30s Wi-Fi reconnect
// window (#946 point 4).
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000;

void setStatus(const String& value) {
  if (!g_statusChar) return;
  // Explicit length, NOT c_str() — NimBLECharacteristic::setValue(const
  // char*) includes the terminating NUL in the GATT value (verified on
  // hardware: a bare "idle" read back as 5 bytes, 'i','d','l','e','\0'),
  // which broke exact-match string comparisons on the Kotlin side.
  g_statusChar->setValue(reinterpret_cast<const uint8_t*>(value.c_str()), value.length());
  g_statusChar->notify();
}

class SsidCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    g_pendingSsid = pChar->getValue().c_str();
  }
};

class PasswordCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    g_pendingPassword = pChar->getValue().c_str();
  }
};

class ApplyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string value = pChar->getValue();
    if (value.empty() || static_cast<uint8_t>(value[0]) != 0x01) return;

    // Defense in depth: the characteristics already cap write length at the
    // ATT layer, but re-check here too (e.g. against a stale/partial write)
    // before touching NVS or the Wi-Fi stack. Empty password is allowed —
    // WiFi.begin() treats "" as an open network.
    if (g_pendingSsid.isEmpty() || g_pendingSsid.length() > SSID_MAX_LEN ||
        g_pendingPassword.length() > PASSWORD_MAX_LEN) {
      setStatus("failed:invalid");
      return;
    }

    WifiCreds::save(g_pendingSsid.c_str(), g_pendingPassword.c_str());

    // No reboot: just a live reconnect on the existing Wi-Fi stack (#946
    // point 4 confirmed this reading).
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WifiCreds::ssid(), WifiCreds::password());

    g_reconnectPending = true;
    g_reconnectStartMs = millis();
    g_lastHeartbeatMs = g_reconnectStartMs;
    setStatus("connecting");
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer* pServer) override {
    // NimBLE stops advertising once a central connects; restart it so the
    // device stays reachable for the next reconfiguration attempt.
    NimBLEDevice::startAdvertising();
  }
};

}  // namespace

void BleProvisioning::begin() {
  NimBLEDevice::init(OTA_HOSTNAME);
  NimBLEDevice::setMTU(PREFERRED_MTU);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(SERVICE_UUID);

  NimBLECharacteristic* ssidChar = service->createCharacteristic(
      CHAR_SSID_UUID, NIMBLE_PROPERTY::WRITE, SSID_MAX_LEN);
  ssidChar->setCallbacks(new SsidCallbacks());

  NimBLECharacteristic* passwordChar = service->createCharacteristic(
      CHAR_PASSWORD_UUID, NIMBLE_PROPERTY::WRITE, PASSWORD_MAX_LEN);
  passwordChar->setCallbacks(new PasswordCallbacks());

  NimBLECharacteristic* applyChar =
      service->createCharacteristic(CHAR_APPLY_UUID, NIMBLE_PROPERTY::WRITE, 1);
  applyChar->setCallbacks(new ApplyCallbacks());

  g_statusChar = service->createCharacteristic(
      CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  g_statusChar->setValue(reinterpret_cast<const uint8_t*>("idle"), 4);

  service->start();

  // Complete 128-bit service UUID in the advertising payload (not just scan
  // response) so the app can scan-filter by UUID rather than device name
  // (#946). Flags(3B) + complete-128-bit-UUID(18B) = 21B, comfortably under
  // the 31B legacy ADV payload limit; NimBLEAdvertising puts the device name
  // in the separate scan-response packet by default.
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
}

void BleProvisioning::poll() {
  if (!g_reconnectPending) return;

  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    g_reconnectPending = false;
    setStatus("connected:" + String(WifiCreds::ssid()) + ":" + WiFi.localIP().toString());
    return;
  }

  if (status == WL_NO_SSID_AVAIL) {
    g_reconnectPending = false;
    setStatus("failed:notfound");
    return;
  }

  if (status == WL_CONNECT_FAILED) {
    g_reconnectPending = false;
    setStatus("failed:auth");
    return;
  }

  uint32_t now = millis();
  if (now - g_reconnectStartMs > RECONNECT_TIMEOUT_MS) {
    g_reconnectPending = false;
    setStatus("failed:timeout");
    return;
  }

  if (now - g_lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    g_lastHeartbeatMs = now;
    setStatus("connecting");
  }
}
