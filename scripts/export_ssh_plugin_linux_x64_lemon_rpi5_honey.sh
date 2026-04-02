#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/export_plugin_linux_x64_lemon_rpi5_honey.sh" \
  ssh \
  tea-plugin-ssh-server \
  tea-plugin-ssh-client \
  "$@"
