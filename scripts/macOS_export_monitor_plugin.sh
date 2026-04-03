#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LEMON_PLATFORM="${LEMON_PLATFORM:-macos}"
export HONEY_PLATFORM="${HONEY_PLATFORM:-macos}"

exec "$SCRIPT_DIR/export_plugin_linux_x64_lemon_rpi5_honey.sh" \
  monitor \
  tea-plugin-monitor-server \
  tea-plugin-monitor-client \
  "$@"
