#!/usr/bin/env bash
# Export lemon-only plugin package for Linux x64 LemonTea.
# Usage:
#   ./scripts/export_cam_lan_stream_plugin_linux_x64.sh [output-dir]

if [ -z "${BASH_VERSION:-}" ]; then
  exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

PLUGIN_NAME="cam-lan-stream"
SERVER_BIN_NAME="tea-plugin-cam-lan-stream-server"

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
LINUX_TOOLCHAIN_FILE="$LINUX_BUILD_DIR/toolchain.cmake"
OUTPUT_DIR="${1:-$PROJECT_ROOT/dist/plugin-exports/$PLUGIN_NAME}"

mkdir -p "$LINUX_BUILD_DIR" "$OUTPUT_DIR"

echo "[prepare] checking Linux x64 compiler and cmake"
tea_resolve_linux_x64_compilers
tea_require_cmd cmake
tea_require_cmd "$TEA_LINUX_X64_CC"
tea_require_cmd "$TEA_LINUX_X64_CXX"

TEA_LINUX_X64_SYSROOT="$(tea_resolve_sysroot "$TEA_LINUX_X64_CC" "${LINUX_X64_SYSROOT:-}" || true)"
if [[ -z "$TEA_LINUX_X64_SYSROOT" || ! -d "$TEA_LINUX_X64_SYSROOT" ]]; then
  echo "[ERROR] invalid Linux x64 sysroot: ${TEA_LINUX_X64_SYSROOT:-<empty>}" >&2
  exit 1
fi

tea_check_target_runtime_deps "linux-x64" "$TEA_LINUX_X64_SYSROOT"
TEA_LINUX_X64_EXTRA_ROOT="$(tea_prepare_brotli_for_target "linux-x64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "$TEA_LINUX_X64_SYSROOT" "x86_64")"

echo "[prepare] writing Linux x64 toolchain"
tea_write_toolchain_file "$LINUX_TOOLCHAIN_FILE" "x86_64" "$TEA_LINUX_X64_CC" "$TEA_LINUX_X64_CXX" "$TEA_LINUX_X64_SYSROOT" "$TEA_LINUX_X64_EXTRA_ROOT"

echo "[1/2] configure Linux x64 build"
tea_configure_target "$PROJECT_ROOT" "$LINUX_BUILD_DIR" "$LINUX_TOOLCHAIN_FILE"

echo "[2/2] build server binary: $SERVER_BIN_NAME"
tea_build_targets "$LINUX_BUILD_DIR" "$SERVER_BIN_NAME"

server_bin="$(tea_find_binary "$LINUX_BUILD_DIR" "$SERVER_BIN_NAME")"
if [[ -z "$server_bin" || ! -f "$server_bin" ]]; then
  echo "[ERROR] Linux x64 server binary not found: $SERVER_BIN_NAME" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

package_dir="$tmp_dir/package"
mkdir -p "$package_dir/server" "$package_dir/frontend"

cp "$PLUGIN_DIR/plugin.json" "$package_dir/plugin.json"
cp "$server_bin" "$package_dir/server/$SERVER_BIN_NAME"
chmod +x "$package_dir/server/$SERVER_BIN_NAME" || true
cp -R "$PLUGIN_DIR/frontend/." "$package_dir/frontend/"

package_name="${PLUGIN_NAME}-unified-lemon-linux-x64.tar.gz"
package_path="$OUTPUT_DIR/$package_name"

tar -czf "$package_path" -C "$package_dir" plugin.json server frontend

echo "[OK] export complete"
echo "  package: $package_path"
