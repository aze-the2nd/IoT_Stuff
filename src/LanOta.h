#pragma once

// Wraps ArduinoOTA: lets PlatformIO flash this device over Wi-Fi
// (`pio run -t upload --upload-port <device-ip>`) instead of USB, during
// active development. Password-protected (see secrets.h). Not the
// field-update mechanism — see GithubOta for that.
namespace LanOta {

// Call every loop() iteration. No-ops until Wi-Fi is connected, then lazily
// starts the OTA listener once and handles incoming requests from then on.
void poll();

}  // namespace LanOta
