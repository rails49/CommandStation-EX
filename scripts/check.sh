#!/usr/bin/env bash
#
# Gate: fail on a tracked file that .gitignore ignores, run the native Guard
# test, then the check.py tests alongside a compile of the station's
# firmware, config.rails49.h against the ESP32 core, as rails49-release.yml
# does. Installs uv, arduino-cli and the core on first use. Builds a copy of
# the checkout, so a local config.h is untouched.

set -eu

if git ls-files -ci --exclude-standard | grep .; then
  echo "tracked files that .gitignore ignores: git rm --cached them"
  exit 1
fi

if ${CXX:-c++} --version >/dev/null 2>&1; then
  rails49/test-guard.sh
else
  echo "guard: skipped, no host C++ compiler"
fi

UV_VERSION=0.12.6
ARDUINO_CLI_VERSION=1.1.1
ESP32_CORE=2.0.17
ESP32_INDEX=https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

export PATH="$HOME/.local/bin:$PATH"
if ! command -v uv >/dev/null; then
  curl -LsSf "https://astral.sh/uv/$UV_VERSION/install.sh" |
    UV_INSTALL_DIR="$HOME/.local/bin" UV_NO_MODIFY_PATH=1 sh
fi
tmp=$(mktemp -d "${TMPDIR:-/tmp}/tmp.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
PYTHONDONTWRITEBYTECODE=1 uv run rails49/test_check.py >"$tmp/test_check.log" 2>&1 &
tests=$!

if ! command -v arduino-cli >/dev/null; then
  mkdir -p "$HOME/.local/bin"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh |
    BINDIR="$HOME/.local/bin" sh -s "$ARDUINO_CLI_VERSION"
fi
if ! arduino-cli core list | grep -q "^esp32:esp32 *$ESP32_CORE "; then
  arduino-cli core update-index --additional-urls "$ESP32_INDEX"
  arduino-cli core install "esp32:esp32@$ESP32_CORE" --additional-urls "$ESP32_INDEX"
fi

sketch="$tmp/CommandStation-EX"
mkdir "$sketch"
git ls-files -z --cached --others --exclude-standard | tar -cf - --null -T - | tar -xf - -C "$sketch"
cp config.rails49.h "$sketch/config.h"

arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all "$sketch"

status=0
wait "$tests" || status=$?
cat "$tmp/test_check.log"
exit "$status"
