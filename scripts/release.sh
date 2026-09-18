#!/usr/bin/env bash
# Builds the firmware, tags it, and publishes a GitHub release with
# firmware.bin attached — devices running GithubOta pick it up automatically
# on their next check (see src/GithubOta.cpp).
#
# Usage: bump FW_VERSION in src/Config.h, then run this script.
set -euo pipefail

cd "$(dirname "$0")/.."

if command -v pio >/dev/null 2>&1; then
  PIO=pio
elif [ -x "$HOME/.venvs/esp-tools/bin/pio" ]; then
  PIO="$HOME/.venvs/esp-tools/bin/pio"
else
  echo "pio (PlatformIO) not found on PATH. Install it or set PIO=/path/to/pio" >&2
  exit 1
fi

VERSION=$(grep -oP 'FW_VERSION = "\K[0-9]+' src/Config.h)
if [ -z "$VERSION" ]; then
  echo "Could not read FW_VERSION from src/Config.h" >&2
  exit 1
fi
TAG="v${VERSION}"

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag $TAG already exists — bump FW_VERSION in src/Config.h first." >&2
  exit 1
fi

echo "Building firmware for release ${TAG}..."
"$PIO" run -e cyd

BIN=".pio/build/cyd/firmware.bin"
if [ ! -f "$BIN" ]; then
  echo "Build did not produce $BIN" >&2
  exit 1
fi

echo "Tagging ${TAG}..."
git tag "$TAG"
git push origin "$TAG"

echo "Creating GitHub release ${TAG}..."
gh release create "$TAG" "$BIN" --title "$TAG" --notes "Release ${TAG}"

echo "Done — devices will pick this up on their next GitHub OTA check."
