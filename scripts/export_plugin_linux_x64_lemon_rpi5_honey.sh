#!/usr/bin/env bash
# Export unified plugin package with selectable target platforms.
#
# Usage:
#   ./scripts/export_plugin_linux_x64_lemon_rpi5_honey.sh <plugin-name> <server-binary> <client-binary> [output-dir]
#   ./scripts/export_plugin_linux_x64_lemon_rpi5_honey.sh <plugin-name> <server-binary> <client-binary> \
#       --lemon-platform linux-x64 --honey-platform rpi5
#
# Example:
#   ./scripts/export_plugin_linux_x64_lemon_rpi5_honey.sh ssh tea-plugin-ssh-server tea-plugin-ssh-client
#
# Platforms:
#   linux-x64 | rpi5 | macos

if [ -z "${BASH_VERSION:-}" ]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

if [[ "$#" -lt 3 ]]; then
  echo "Usage: $0 <plugin-name> <server-binary> <client-binary> [output-dir] [--lemon-platform <linux-x64|rpi5|macos>] [--honey-platform <linux-x64|rpi5|macos>]" >&2
  exit 1
fi

PLUGIN_NAME="$1"
SERVER_BIN_NAME="$2"
CLIENT_BIN_NAME="$3"
shift 3

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
COMMON_SCRIPT="$SCRIPT_DIR/cross_build_common.sh"

if [[ ! -f "$COMMON_SCRIPT" ]]; then
  echo "[ERROR] missing helper script: $COMMON_SCRIPT" >&2
  exit 1
fi

# shellcheck source=./cross_build_common.sh
source "$COMMON_SCRIPT"

PLUGIN_DIR="$PROJECT_ROOT/plugins/$PLUGIN_NAME"
if [[ ! -d "$PLUGIN_DIR" ]]; then
  echo "[ERROR] plugin directory not found: $PLUGIN_DIR" >&2
  exit 1
fi

normalize_platform() {
  local raw="$1"
  local lowered
  lowered="$(echo "$raw" | tr '[:upper:]' '[:lower:]')"
  case "$lowered" in
    linux-x64|linux64|linux_x64|x64|amd64|x86_64)
      echo "linux-x64"
      ;;
    rpi5|raspberrypi5|raspberry-pi-5|aarch64|arm64)
      echo "rpi5"
      ;;
    macos|darwin|mac)
      echo "macos"
      ;;
    *)
      echo ""
      ;;
  esac
}

BUILD_ROOT="${BUILD_ROOT:-$PROJECT_ROOT/build-cross}"
LINUX_BUILD_DIR="$BUILD_ROOT/linux-x64"
RPI5_BUILD_DIR="$BUILD_ROOT/rpi5"
MACOS_BUILD_DIR="$BUILD_ROOT/macos"
LINUX_TOOLCHAIN_FILE="$LINUX_BUILD_DIR/toolchain.cmake"
RPI5_TOOLCHAIN_FILE="$RPI5_BUILD_DIR/toolchain.cmake"

LEMON_PLATFORM="${LEMON_PLATFORM:-linux-x64}"
HONEY_PLATFORM="${HONEY_PLATFORM:-rpi5}"
OUTPUT_DIR=""

if [[ "$#" -gt 0 ]]; then
  first_extra="$1"
  if [[ "$first_extra" != --* ]]; then
    OUTPUT_DIR="$first_extra"
    shift
  fi
fi

while [[ "$#" -gt 0 ]]; do
  case "$1" in
    --lemon-platform)
      [[ "$#" -lt 2 ]] && { echo "[ERROR] missing value for --lemon-platform" >&2; exit 1; }
      LEMON_PLATFORM="$2"
      shift 2
      ;;
    --honey-platform)
      [[ "$#" -lt 2 ]] && { echo "[ERROR] missing value for --honey-platform" >&2; exit 1; }
      HONEY_PLATFORM="$2"
      shift 2
      ;;
    --output-dir)
      [[ "$#" -lt 2 ]] && { echo "[ERROR] missing value for --output-dir" >&2; exit 1; }
      OUTPUT_DIR="$2"
      shift 2
      ;;
    *)
      echo "[ERROR] unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

LEMON_PLATFORM="$(normalize_platform "$LEMON_PLATFORM")"
HONEY_PLATFORM="$(normalize_platform "$HONEY_PLATFORM")"

if [[ -z "$LEMON_PLATFORM" || -z "$HONEY_PLATFORM" ]]; then
  echo "[ERROR] unsupported platform selection. supported: linux-x64, rpi5, macos" >&2
  exit 1
fi

OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/dist/plugin-exports/$PLUGIN_NAME}"

mkdir -p "$LINUX_BUILD_DIR" "$RPI5_BUILD_DIR" "$MACOS_BUILD_DIR" "$OUTPUT_DIR"

cross_env_ready=0
linux_configured=0
rpi5_configured=0
macos_configured=0

ensure_cross_configured() {
  if [[ "$cross_env_ready" -eq 1 ]]; then
    return
  fi

  echo "[prepare] checking cross compilers and cmake" >&2
  tea_prepare_cross_env

  echo "[prepare] writing cross toolchain files" >&2
  tea_write_toolchain_file "$LINUX_TOOLCHAIN_FILE" "x86_64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "$TEA_LINUX_X64_SYSROOT" "$TEA_LINUX_X64_EXTRA_ROOT"
  tea_write_toolchain_file "$RPI5_TOOLCHAIN_FILE" "aarch64" "$TEA_RPI5_CC" "$TEA_RPI5_CXX" "$TEA_RPI5_SYSROOT" "$TEA_RPI5_EXTRA_ROOT"

  cross_env_ready=1
}

configure_platform() {
  local platform="$1"

  case "$platform" in
    linux-x64)
      ensure_cross_configured
      if [[ "$linux_configured" -eq 0 ]]; then
        echo "[configure] Linux x64" >&2
        tea_configure_target "$PROJECT_ROOT" "$LINUX_BUILD_DIR" "$LINUX_TOOLCHAIN_FILE"
        linux_configured=1
      fi
      ;;
    rpi5)
      ensure_cross_configured
      if [[ "$rpi5_configured" -eq 0 ]]; then
        echo "[configure] Raspberry Pi 5" >&2
        tea_configure_target "$PROJECT_ROOT" "$RPI5_BUILD_DIR" "$RPI5_TOOLCHAIN_FILE"
        rpi5_configured=1
      fi
      ;;
    macos)
      if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "[ERROR] macOS target can only be built on macOS host" >&2
        exit 1
      fi
      if [[ "$macos_configured" -eq 0 ]]; then
        echo "[configure] macOS native" >&2
        rm -f "$MACOS_BUILD_DIR/CMakeCache.txt"
        cmake -S "$PROJECT_ROOT" -B "$MACOS_BUILD_DIR" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES="$(uname -m)"
        macos_configured=1
      fi
      ;;
    *)
      echo "[ERROR] unsupported platform: $platform" >&2
      exit 1
      ;;
  esac
}

build_dir_for_platform() {
  local platform="$1"
  case "$platform" in
    linux-x64) echo "$LINUX_BUILD_DIR" ;;
    rpi5) echo "$RPI5_BUILD_DIR" ;;
    macos) echo "$MACOS_BUILD_DIR" ;;
    *) return 1 ;;
  esac
}

BUILT_BINARY_PATH=""

build_binary_for_platform() {
  local platform="$1"
  local target_name="$2"

  configure_platform "$platform"

  local build_dir
  build_dir="$(build_dir_for_platform "$platform")"

  echo "[build:$platform] $target_name" >&2
  tea_build_targets "$build_dir" "$target_name"

  local found
  found="$(tea_find_binary "$build_dir" "$target_name")"
  if [[ -z "$found" || ! -f "$found" ]]; then
    echo "[ERROR] binary not found after build: platform=$platform target=$target_name" >&2
    exit 1
  fi

  BUILT_BINARY_PATH="$found"
}

echo "[plan] LemonTea platform: $LEMON_PLATFORM"
echo "[plan] HoneyTea platform: $HONEY_PLATFORM"

build_binary_for_platform "$LEMON_PLATFORM" "$SERVER_BIN_NAME"
server_bin="$BUILT_BINARY_PATH"

build_binary_for_platform "$HONEY_PLATFORM" "$CLIENT_BIN_NAME"
client_bin="$BUILT_BINARY_PATH"

if [[ -z "$server_bin" || ! -f "$server_bin" ]]; then
  echo "[ERROR] server binary not found: $SERVER_BIN_NAME ($LEMON_PLATFORM)" >&2
  exit 1
fi

if [[ -z "$client_bin" || ! -f "$client_bin" ]]; then
  echo "[ERROR] client binary not found: $CLIENT_BIN_NAME ($HONEY_PLATFORM)" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

package_dir="$tmp_dir/package"
mkdir -p "$package_dir/server" "$package_dir/client" "$package_dir/frontend"

cp "$PLUGIN_DIR/plugin.json" "$package_dir/plugin.json"
cp "$server_bin" "$package_dir/server/$SERVER_BIN_NAME"
cp "$client_bin" "$package_dir/client/$CLIENT_BIN_NAME"
chmod +x "$package_dir/server/$SERVER_BIN_NAME" "$package_dir/client/$CLIENT_BIN_NAME" || true

if [[ -d "$PLUGIN_DIR/frontend" ]]; then
  cp -R "$PLUGIN_DIR/frontend/." "$package_dir/frontend/"
fi

if [[ "$LEMON_PLATFORM" == "macos" && "$HONEY_PLATFORM" == "macos" ]]; then
  package_name="${PLUGIN_NAME}-unified-macos.tar.gz"
elif [[ "$LEMON_PLATFORM" == "linux-x64" && "$HONEY_PLATFORM" == "rpi5" ]]; then
  package_name="${PLUGIN_NAME}-unified-lemon-linux-x64-honey-rpi5.tar.gz"
else
  package_name="${PLUGIN_NAME}-unified-lemon-${LEMON_PLATFORM}-honey-${HONEY_PLATFORM}.tar.gz"
fi
package_path="$OUTPUT_DIR/$package_name"

tar_items=(plugin.json server client)
if [[ -d "$package_dir/frontend" ]] && [[ -n "$(ls -A "$package_dir/frontend" 2>/dev/null || true)" ]]; then
  tar_items+=(frontend)
fi

tar -czf "$package_path" -C "$package_dir" "${tar_items[@]}"

echo "[OK] export complete"
echo "  package: $package_path"
