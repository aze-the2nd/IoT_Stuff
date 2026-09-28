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

NimBLECharacteristic* g_statusChar = nullptr;

// Staging area: the app writes SSID then Password as two separate
// characteristic writes, then triggers Apply — these hold whatever's been
// written so far until Apply fires.
String g_pendingSsid;
String g_pendingPassword;

bool g_reconnectPending = false;
uint32_t g_reconnectStartMs = 0;
constexpr uint32_t RECONNECT_TIMEOUT_MS = 15000;

void setStatus(const String& value) {
  if (!g_statusChar) return;
  g_statusChar->setValue(value.c_str());
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
    if (g_pendingSsid.isEmpty()) {
      setStatus("failed:no ssid");
      return;
    }

    WifiCreds::save(g_pendingSsid.c_str(), g_pendingPassword.c_str());
    setStatus("connecting");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WifiCreds::ssid(), WifiCreds::password());

    g_reconnectPending = true;
    g_reconnectStartMs = millis();
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

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(SERVICE_UUID);

  NimBLECharacteristic* ssidChar =
      service->createCharacteristic(CHAR_SSID_UUID, NIMBLE_PROPERTY::WRITE);
  ssidChar->setCallbacks(new SsidCallbacks());

  NimBLECharacteristic* passwordChar =
      service->createCharacteristic(CHAR_PASSWORD_UUID, NIMBLE_PROPERTY::WRITE);
  passwordChar->setCallbacks(new PasswordCallbacks());

  NimBLECharacteristic* applyChar =
      service->createCharacteristic(CHAR_APPLY_UUID, NIMBLE_PROPERTY::WRITE);
  applyChar->setCallbacks(new ApplyCallbacks());

  g_statusChar = service->createCharacteristic(
      CHAR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  g_statusChar->setValue("idle");

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();
}

void BleProvisioning::poll() {
  if (!g_reconnectPending) return;

  if (WiFi.status() == WL_CONNECTED) {
    g_reconnectPending = false;
    setStatus("connected:" + String(WifiCreds::ssid()) + ":" + WiFi.localIP().toString());
    return;
  }

  if (millis() - g_reconnectStartMs > RECONNECT_TIMEOUT_MS) {
    g_reconnectPending = false;
    setStatus("failed:timeout");
  }
}
