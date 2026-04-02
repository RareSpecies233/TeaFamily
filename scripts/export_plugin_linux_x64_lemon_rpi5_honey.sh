#!/usr/bin/env bash
# Export unified plugin package:
# - server binary target: LemonTea (Linux x64)
# - client binary target: HoneyTea (Raspberry Pi 5 aarch64)
#
# Usage:
#   ./scripts/export_plugin_linux_x64_lemon_rpi5_honey.sh <plugin-name> <server-binary> <client-binary> [output-dir]
#
# Example:
#   ./scripts/export_plugin_linux_x64_lemon_rpi5_honey.sh ssh tea-plugin-ssh-server tea-plugin-ssh-client

if [ -z "${BASH_VERSION:-}" ]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

if [[ "$#" -lt 3 ]]; then
  echo "Usage: $0 <plugin-name> <server-binary> <client-binary> [output-dir]" >&2
  exit 1
fi

PLUGIN_NAME="$1"
SERVER_BIN_NAME="$2"
CLIENT_BIN_NAME="$3"

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

BUILD_ROOT="${BUILD_ROOT:-$PROJECT_ROOT/build-cross}"
LINUX_BUILD_DIR="$BUILD_ROOT/linux-x64"
RPI5_BUILD_DIR="$BUILD_ROOT/rpi5"
LINUX_TOOLCHAIN_FILE="$LINUX_BUILD_DIR/toolchain.cmake"
RPI5_TOOLCHAIN_FILE="$RPI5_BUILD_DIR/toolchain.cmake"
OUTPUT_DIR="${4:-$PROJECT_ROOT/dist/plugin-exports/$PLUGIN_NAME}"

mkdir -p "$LINUX_BUILD_DIR" "$RPI5_BUILD_DIR" "$OUTPUT_DIR"

echo "[prepare] checking compilers and cmake"
tea_prepare_cross_env

echo "[prepare] writing toolchain files"
tea_write_toolchain_file "$LINUX_TOOLCHAIN_FILE" "x86_64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "LINUX_X64_SYSROOT"
tea_write_toolchain_file "$RPI5_TOOLCHAIN_FILE" "aarch64" "$TEA_RPI5_CC" "$TEA_RPI5_CXX" "RPI5_SYSROOT"

echo "[1/4] configure Linux x64 build"
tea_configure_target "$PROJECT_ROOT" "$LINUX_BUILD_DIR" "$LINUX_TOOLCHAIN_FILE"

echo "[2/4] build server binary: $SERVER_BIN_NAME"
tea_build_targets "$LINUX_BUILD_DIR" "$SERVER_BIN_NAME"

echo "[3/4] configure Raspberry Pi 5 build"
tea_configure_target "$PROJECT_ROOT" "$RPI5_BUILD_DIR" "$RPI5_TOOLCHAIN_FILE"

echo "[4/4] build client binary: $CLIENT_BIN_NAME"
tea_build_targets "$RPI5_BUILD_DIR" "$CLIENT_BIN_NAME"

server_bin="$(tea_find_binary "$LINUX_BUILD_DIR" "$SERVER_BIN_NAME")"
client_bin="$(tea_find_binary "$RPI5_BUILD_DIR" "$CLIENT_BIN_NAME")"

if [[ -z "$server_bin" || ! -f "$server_bin" ]]; then
  echo "[ERROR] Linux x64 server binary not found: $SERVER_BIN_NAME" >&2
  exit 1
fi

if [[ -z "$client_bin" || ! -f "$client_bin" ]]; then
  echo "[ERROR] Raspberry Pi 5 client binary not found: $CLIENT_BIN_NAME" >&2
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

package_name="${PLUGIN_NAME}-unified-lemon-linux-x64-honey-rpi5.tar.gz"
package_path="$OUTPUT_DIR/$package_name"

tar -czf "$package_path" -C "$package_dir" plugin.json server client frontend

echo "[OK] export complete"
echo "  package: $package_path"
