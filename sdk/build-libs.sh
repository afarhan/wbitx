#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Ashhar Farhan
#
# build-libs.sh — makes sdk/esp32c3.tar.xz again: the ESP32-C3 libraries
# the Arduino core 3.3.0 builds against, as Espressif builds them, but with
# power management, so that the talkie can sleep between the access
# point's beacons with its WiFi held (light sleep, 2026-11-25).
#
# Everything is as the core's own libraries were made, from the commits
# their versions.txt names: ESP-IDF b66b5448e0 and arduino-esp32
# 554de56f4, by Espressif's esp32-arduino-lib-builder on release/v5.5. Only
# the four settings below are added to its configs/defconfig.esp32c3.
#
#     sdk/build-libs.sh [work directory]    default esp32-lib-builder beside the repo
#
# It needs git, python3, cmake, ninja, gperf and jq, an hour or so, and
# some 10 GB in the work directory. The Makefile unpacks the result into
# build/ and points arduino-cli at it; nothing is installed.

set -e
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=${1:-$HERE/../../esp32-lib-builder}
IDF_COMMIT=b66b5448e0
ARDUINO_COMMIT=554de56f4

if [ ! -d "$WORK" ]; then
	git clone https://github.com/espressif/esp32-arduino-lib-builder.git "$WORK"
	git -C "$WORK" checkout beff445
fi
cd "$WORK"

if ! grep -q "wbitx" configs/defconfig.esp32c3; then
	cat >> configs/defconfig.esp32c3 <<'EOF'

# wbitx: light sleep between the access point's beacons with the WiFi held
CONFIG_PM_ENABLE=y
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=3
CONFIG_ESP_WIFI_SLP_IRAM_OPT=y
EOF
fi
# this lib-builder lists -n for no ccache and does not take it
sed -i 's/^CCACHE_ENABLE=1/CCACHE_ENABLE=0/' build.sh

# the first run fetches ESP-IDF, the tools and the components; it stops
# when the newest arduino-esp32 wants newer components than 554de56f4 did,
# which is why arduino-esp32 is then put back and the build run again
# without updating anything
if [ ! -d components/arduino ]; then
	./build.sh -t esp32c3 -i "$IDF_COMMIT" -A release/v3.3.x || true
fi
git -C components/arduino fetch -q origin "$ARDUINO_COMMIT" || true
git -C components/arduino checkout -q "$ARDUINO_COMMIT"
rm -f dependencies.lock
(. esp-idf/export.sh > /dev/null && ./build.sh -s -t esp32c3)

cd out/tools/esp32-arduino-libs
cp versions.txt esp32c3/versions.txt
cp versions.txt "$HERE/versions.txt"
tar -cf - esp32c3 | xz -T0 -9 > "$HERE/esp32c3.tar.xz"
grep -q '^CONFIG_PM_ENABLE=y' esp32c3/sdkconfig
echo "sdk/esp32c3.tar.xz made, with power management"
