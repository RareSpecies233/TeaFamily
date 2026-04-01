#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PLUGIN_NAME="monitor"
SERVER_BIN_NAME="tea-plugin-monitor-server"
CLIENT_BIN_NAME="tea-plugin-monitor-client"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-release}"
OUTPUT_DIR="${1:-$PROJECT_ROOT/dist/plugin-exports/$PLUGIN_NAME}"

find_bin() {
  local name="$1"
  local candidate=""
  for p in \
    "$BUILD_DIR/bin/$name" \
    "$PROJECT_ROOT/build/bin/$name" \
    "$BUILD_DIR/$name"; do
    if [[ -f "$p" ]]; then
      candidate="$p"
      break
    fi
  done
  if [[ -z "$candidate" ]]; then
    candidate="$(find "$BUILD_DIR" "$PROJECT_ROOT/build" -type f -name "$name" -perm -111 -print -quit 2>/dev/null || true)"
  fi
  printf '%s' "$candidate"
}

server_bin="$(find_bin "$SERVER_BIN_NAME")"
client_bin="$(find_bin "$CLIENT_BIN_NAME")"

if [[ -z "$server_bin" || -z "$client_bin" ]]; then
  echo "[ERROR] 未找到 $PLUGIN_NAME 插件二进制，请先执行 ./scripts/macOS_build_release.sh" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

runtime_dir="$tmp_dir/runtime"
package_dir="$tmp_dir/package"
mkdir -p "$runtime_dir/server" "$runtime_dir/client" "$package_dir/frontend"

cp "$PROJECT_ROOT/plugins/$PLUGIN_NAME/plugin.json" "$runtime_dir/plugin.json"
cp "$server_bin" "$runtime_dir/server/$SERVER_BIN_NAME"
cp "$client_bin" "$runtime_dir/client/$CLIENT_BIN_NAME"
chmod +x "$runtime_dir/server/$SERVER_BIN_NAME" "$runtime_dir/client/$CLIENT_BIN_NAME"

cp -R "$PROJECT_ROOT/plugins/$PLUGIN_NAME/frontend/"* "$package_dir/frontend/"
cp "$runtime_dir/plugin.json" "$package_dir/plugin.json"
cp -R "$runtime_dir/server" "$package_dir/"
cp -R "$runtime_dir/client" "$package_dir/"

unified_pkg="$OUTPUT_DIR/${PLUGIN_NAME}-unified-macos.tar.gz"

tar -czf "$unified_pkg" -C "$package_dir" plugin.json server client frontend

echo "[OK] 导出完成"
echo "  unified: $unified_pkg"
