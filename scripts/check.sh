#!/usr/bin/env bash
#
# Gate: compile the station's firmware, config.rails49.h against the ESP32
# core, as rails49-release.yml does. Installs arduino-cli and the core on
# first use. Builds a copy of the checkout, so a local config.h is untouched.

set -eu

ARDUINO_CLI_VERSION=1.1.1
ESP32_CORE=2.0.17
ESP32_INDEX=https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

export PATH="$HOME/.local/bin:$PATH"
if ! command -v arduino-cli >/dev/null; then
  mkdir -p "$HOME/.local/bin"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh |
    BINDIR="$HOME/.local/bin" sh -s "$ARDUINO_CLI_VERSION"
fi
if ! arduino-cli core list | grep -q "^esp32:esp32 *$ESP32_CORE "; then
  arduino-cli core update-index --additional-urls "$ESP32_INDEX"
  arduino-cli core install "esp32:esp32@$ESP32_CORE" --additional-urls "$ESP32_INDEX"
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
sketch="$tmp/CommandStation-EX"
mkdir "$sketch"
git ls-files -z --cached --others --exclude-standard | tar -cf - --null -T - | tar -xf - -C "$sketch"
cp config.rails49.h "$sketch/config.h"

arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all "$sketch"
