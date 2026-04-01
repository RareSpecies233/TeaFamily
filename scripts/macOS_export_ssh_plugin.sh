#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
PLUGIN_NAME="ssh"
SERVER_BIN_NAME="tea-plugin-ssh-server"
CLIENT_BIN_NAME="tea-plugin-ssh-client"
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
frontend_dir="$tmp_dir/frontend"
mkdir -p "$runtime_dir/server" "$runtime_dir/client" "$frontend_dir"

cp "$PROJECT_ROOT/plugins/$PLUGIN_NAME/plugin.json" "$runtime_dir/plugin.json"
cp "$server_bin" "$runtime_dir/server/$SERVER_BIN_NAME"
cp "$client_bin" "$runtime_dir/client/$CLIENT_BIN_NAME"
chmod +x "$runtime_dir/server/$SERVER_BIN_NAME" "$runtime_dir/client/$CLIENT_BIN_NAME"

cp -R "$PROJECT_ROOT/plugins/$PLUGIN_NAME/frontend/"* "$frontend_dir/"

runtime_pkg="$OUTPUT_DIR/${PLUGIN_NAME}-runtime-macos.tar.gz"
frontend_pkg="$OUTPUT_DIR/${PLUGIN_NAME}-frontend-macos.tar.gz"

tar -czf "$runtime_pkg" -C "$runtime_dir" plugin.json server client
tar -czf "$frontend_pkg" -C "$frontend_dir" .

echo "[OK] 导出完成"
echo "  runtime : $runtime_pkg"
echo "  frontend: $frontend_pkg"
