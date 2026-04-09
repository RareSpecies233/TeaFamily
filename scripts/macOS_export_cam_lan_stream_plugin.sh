#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LEMON_PLATFORM="${LEMON_PLATFORM:-macos}"
export HONEY_PLATFORM="${HONEY_PLATFORM:-macos}"

exec "$SCRIPT_DIR/export_cam_lan_stream_plugin_linux_x64.sh" \
  "$@"
