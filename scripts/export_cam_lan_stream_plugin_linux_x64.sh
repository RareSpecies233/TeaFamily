#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/export_plugin_linux_x64_lemon_rpi5_honey.sh" \
  cam-lan-stream \
  tea-plugin-cam-lan-stream-server \
  tea-plugin-cam-lan-stream-client \
  "$@"
