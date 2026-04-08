#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# CI: Zephyr west workspace + SDK, then CMake builds:
#   - samples/zephyr/ul_ecat_scan  @ native_sim/native/64   (master)
#   - samples/zephyr/ul_ecat_servo @ esp32_devkitc_wroom/esp32/procpu (slave + SPI overlay)
#
# Install build tools on the runner first (see .github/workflows/ci.yml).
#
# Environment:
#   UL_ECAT_ROOT — repo root (default: parent of scripts/)
#   ZEPHYR_REV   — Zephyr tag/branch (default: v3.7.2)
#   ZEPHYR_CI_HOME — work dir for zephyr/.west/sdk (default: UL_ECAT_ROOT/.zephyr-ci)

set -euo pipefail

UL_ECAT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export UL_ECAT_ROOT
ZEPHYR_REV="${ZEPHYR_REV:-v3.7.2}"
ZEPHYR_CI_HOME="${ZEPHYR_CI_HOME:-$UL_ECAT_ROOT/.zephyr-ci}"

python3 -m pip install --user -U pip wheel west
export PATH="$HOME/.local/bin:$PATH"

mkdir -p "$ZEPHYR_CI_HOME"
cd "$ZEPHYR_CI_HOME"

if [[ ! -d zephyr ]]; then
	git clone --depth 1 --branch "$ZEPHYR_REV" \
		https://github.com/zephyrproject-rtos/zephyr.git zephyr
fi

if [[ ! -d .west ]]; then
	west init -l zephyr
fi

# Pull all west projects (hal_espressif, etc.) — required for ESP32 and other SoCs.
# Do not use shallow clones here: manifest pins many modules to specific SHAs that are
# not branch tips; --depth=1 makes those checkouts fail (west reports "multiple projects").
# If west update fails after a partial run, remove $ZEPHYR_CI_HOME/modules (and
# bootloader/, tools/ if present) and run again.
west update

export ZEPHYR_BASE="$ZEPHYR_CI_HOME/zephyr"
python3 -m pip install --user -r "$ZEPHYR_BASE/scripts/requirements-base.txt"

west zephyr-export

SDK_VER="$(cat "$ZEPHYR_BASE/SDK_VERSION")"
SDK_TAR="zephyr-sdk-${SDK_VER}_linux-x86_64_minimal.tar.xz"
SDK_URL="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VER}/${SDK_TAR}"

if [[ ! -d zephyr-sdk ]]; then
	wget -q "$SDK_URL" -O "/tmp/${SDK_TAR}"
	mkdir -p zephyr-sdk
	tar xf "/tmp/${SDK_TAR}" -C zephyr-sdk --strip-components=1
	rm -f "/tmp/${SDK_TAR}"
fi

export ZEPHYR_SDK_INSTALL_DIR="$ZEPHYR_CI_HOME/zephyr-sdk"
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
(
	cd "$ZEPHYR_SDK_INSTALL_DIR"
	./setup.sh -h -c
	# native_sim: host GCC; esp32 slave: Xtensa (ESP32 classic)
	./setup.sh -t xtensa-espressif_esp32_zephyr-elf
)

# CMakeLists append ZEPHYR_EXTRA_MODULES; a duplicate env var triggers a CMake warning.
unset ZEPHYR_EXTRA_MODULES

echo "=== Building ul_ecat_scan (native_sim/native/64) ==="
cmake -B "$UL_ECAT_ROOT/build-ci-zephyr-scan" -GNinja \
	-S "$UL_ECAT_ROOT/samples/zephyr/ul_ecat_scan" \
	-DBOARD=native_sim/native/64
cmake --build "$UL_ECAT_ROOT/build-ci-zephyr-scan"

echo "=== Building ul_ecat_servo (esp32_devkitc_wroom/esp32/procpu) ==="
cmake -B "$UL_ECAT_ROOT/build-ci-zephyr-servo" -GNinja \
	-S "$UL_ECAT_ROOT/samples/zephyr/ul_ecat_servo" \
	-DBOARD=esp32_devkitc_wroom/esp32/procpu
cmake --build "$UL_ECAT_ROOT/build-ci-zephyr-servo"

echo "OK: Zephyr samples built."
