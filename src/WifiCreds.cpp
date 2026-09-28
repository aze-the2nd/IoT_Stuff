#include "WifiCreds.h"

#include <Preferences.h>

#include "secrets.h"

namespace {

constexpr const char* NVS_NAMESPACE = "wificreds";
constexpr const char* KEY_SSID = "ssid";
constexpr const char* KEY_PASSWORD = "password";

Preferences g_prefs;

// Buffers backing ssid()/password() — Preferences::getString() needs
// somewhere to land, and the rest of the app expects plain const char*
// (matching the old WIFI_SSID/WIFI_PASSWORD constexpr pointers).
char g_ssid[33] = {0};
char g_password[65] = {0};

}  // namespace

void WifiCreds::begin() {
  g_prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);

  if (!g_prefs.isKey(KEY_SSID)) {
    // First-ever boot: seed NVS from the compiled defaults so behavior is
    // unchanged until someone reconfigures over Bluetooth.
    g_prefs.putString(KEY_SSID, WIFI_SSID);
    g_prefs.putString(KEY_PASSWORD, WIFI_PASSWORD);
  }

  String ssid = g_prefs.getString(KEY_SSID, WIFI_SSID);
  String password = g_prefs.getString(KEY_PASSWORD, WIFI_PASSWORD);
  strncpy(g_ssid, ssid.c_str(), sizeof(g_ssid) - 1);
  strncpy(g_password, password.c_str(), sizeof(g_password) - 1);
}

const char* WifiCreds::ssid() { return g_ssid; }

const char* WifiCreds::password() { return g_password; }

void WifiCreds::save(const char* newSsid, const char* newPassword) {
  strncpy(g_ssid, newSsid, sizeof(g_ssid) - 1);
  g_ssid[sizeof(g_ssid) - 1] = '\0';
  strncpy(g_password, newPassword, sizeof(g_password) - 1);
  g_password[sizeof(g_password) - 1] = '\0';

  g_prefs.putString(KEY_SSID, g_ssid);
  g_prefs.putString(KEY_PASSWORD, g_password);
}
