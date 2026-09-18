#pragma once

// Periodically checks the GitHub repo's latest release for a newer firmware
// version than the one currently running, and self-updates if found. This
// is the "real" field-update mechanism (works without PlatformIO/USB
// nearby) — see LanOta for the dev-loop-convenience alternative.
//
// Publishing a new version: bump FW_VERSION in Config.h, then run
// scripts/release.sh, which builds, tags, and attaches firmware.bin to a
// GitHub release. The device finds it via the GitHub API
// (repos/{owner}/{repo}/releases/latest) — no server of our own needed.
namespace GithubOta {

// Call every loop() iteration. No-ops until Wi-Fi is connected; checks once
// on the first opportunity, then again every GITHUB_OTA_CHECK_INTERVAL_MS.
// A successful update reboots the device from inside this call and never
// returns.
void poll();

}  // namespace GithubOta
