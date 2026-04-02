#!/usr/bin/env bash
# Build target matrix:
# - Linux x64: GreenTea + LemonTea
# - Raspberry Pi 5 (aarch64): GreenTea + HoneyTea
#
# Usage:
#   ./scripts/build_linux_x64_and_rpi5.sh
#   ./scripts/build_linux_x64_and_rpi5.sh clean
#
# Optional environment variables:
#   BUILD_ROOT=/path/to/build-root
#   DIST_ROOT=/path/to/dist-root
#   LINUX_X64_CC=...
#   LINUX_X64_CXX=...
#   LINUX_X64_SYSROOT=/path/to/sysroot
#   RPI5_CC=...
#   RPI5_CXX=...
#   RPI5_SYSROOT=/path/to/sysroot

if [ -z "${BASH_VERSION:-}" ]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
COMMON_SCRIPT="$SCRIPT_DIR/cross_build_common.sh"

if [[ ! -f "$COMMON_SCRIPT" ]]; then
  echo "[ERROR] missing helper script: $COMMON_SCRIPT" >&2
  exit 1
fi

# shellcheck source=./cross_build_common.sh
source "$COMMON_SCRIPT"

BUILD_ROOT="${BUILD_ROOT:-$PROJECT_ROOT/build-cross}"
DIST_ROOT="${DIST_ROOT:-$PROJECT_ROOT/dist/cross-targets}"
LINUX_BUILD_DIR="$BUILD_ROOT/linux-x64"
RPI5_BUILD_DIR="$BUILD_ROOT/rpi5"
LINUX_TOOLCHAIN_FILE="$LINUX_BUILD_DIR/toolchain.cmake"
RPI5_TOOLCHAIN_FILE="$RPI5_BUILD_DIR/toolchain.cmake"

if [[ "${1:-}" == "clean" ]]; then
  echo "[clean] removing $BUILD_ROOT and $DIST_ROOT"
  rm -rf "$BUILD_ROOT" "$DIST_ROOT"
fi

mkdir -p "$LINUX_BUILD_DIR" "$RPI5_BUILD_DIR"

echo "[prepare] checking compilers and cmake"
tea_prepare_cross_env

echo "[prepare] writing toolchain files"
tea_write_toolchain_file "$LINUX_TOOLCHAIN_FILE" "x86_64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "$TEA_LINUX_X64_SYSROOT" "$TEA_LINUX_X64_EXTRA_ROOT"
tea_write_toolchain_file "$RPI5_TOOLCHAIN_FILE" "aarch64" "$TEA_RPI5_CC" "$TEA_RPI5_CXX" "$TEA_RPI5_SYSROOT" "$TEA_RPI5_EXTRA_ROOT"

echo "[1/4] configure Linux x64"
tea_configure_target "$PROJECT_ROOT" "$LINUX_BUILD_DIR" "$LINUX_TOOLCHAIN_FILE"

echo "[2/4] build Linux x64 (GreenTea, LemonTea)"
tea_build_targets "$LINUX_BUILD_DIR" GreenTea LemonTea

echo "[3/4] configure Raspberry Pi 5 (aarch64)"
tea_configure_target "$PROJECT_ROOT" "$RPI5_BUILD_DIR" "$RPI5_TOOLCHAIN_FILE"

echo "[4/4] build Raspberry Pi 5 (GreenTea, HoneyTea)"
tea_build_targets "$RPI5_BUILD_DIR" GreenTea HoneyTea

LINUX_DIST="$DIST_ROOT/linux-x64"
RPI5_DIST="$DIST_ROOT/rpi5"

echo "[pack] collecting runtime binaries"
tea_copy_binary_or_fail "$LINUX_BUILD_DIR" GreenTea "$LINUX_DIST/bin"
tea_copy_binary_or_fail "$LINUX_BUILD_DIR" LemonTea "$LINUX_DIST/bin"
tea_copy_binary_or_fail "$RPI5_BUILD_DIR" GreenTea "$RPI5_DIST/bin"
tea_copy_binary_or_fail "$RPI5_BUILD_DIR" HoneyTea "$RPI5_DIST/bin"

echo "[pack] collecting config files"
mkdir -p "$LINUX_DIST/config" "$RPI5_DIST/config"
cp "$PROJECT_ROOT/GreenTea/config.json" "$LINUX_DIST/config/GreenTea.json"
cp "$PROJECT_ROOT/LemonTea/config.json" "$LINUX_DIST/config/LemonTea.json"
cp "$PROJECT_ROOT/GreenTea/config.json" "$RPI5_DIST/config/GreenTea.json"
cp "$PROJECT_ROOT/HoneyTea/config.json" "$RPI5_DIST/config/HoneyTea.json"

echo ""
echo "Build complete."
echo "  Linux x64 output: $LINUX_DIST"
echo "  Raspberry Pi 5 output: $RPI5_DIST"
