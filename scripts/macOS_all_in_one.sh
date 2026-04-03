#!/usr/bin/env bash
# TeaFamily macOS All-in-One Build + Plugin Export Script
# Usage: ./scripts/macOS_all_in_one.sh

set -euo pipefail

if [ -z "${BASH_VERSION:-}" ]; then
    exec /usr/bin/env bash "$0" "$@"
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

ask() {
    local prompt="$1"
    local default="${2:-n}"
    while true; do
        read -rp "$prompt [$default]: " ans
        ans="${ans:-$default}"
        ans="$(echo "$ans" | tr '[:upper:]' '[:lower:]')"
        case "$ans" in
            y|yes|1) return 0 ;;
            n|no|2) return 1 ;;
            *) echo "请输入 1/y 或 2/n" ;;
        esac
    done
}

log() { echo "[all-in-one] $*"; }

should_export_macos=false
should_clean=false
should_export_cross=false
should_export_plugins_macos_macos=false
should_export_plugins_macos_rpi5=false
should_export_plugins_linux_rpi5=false

if ask "是否导出 macOS 版本 LemonTea/GreenTea/HoneyTea？(1或y确认/2或n取消)" n; then
    should_export_macos=true
fi

if ask "是否 clean 构建？(1或y确认/2或n取消)" n; then
    should_clean=true
fi

if ask "是否导出 Linux x64 版本 LemonTea/GreenTea 与 树莓派5 版本 GreenTea/HoneyTea？(1或y确认/2或n取消)" n; then
    should_export_cross=true
fi

if ask "是否导出 macOS-LemonTea/macOS-HoneyTea 的所有插件？(1或y确认/2或n取消)" n; then
    should_export_plugins_macos_macos=true
fi

if ask "是否导出 macOS-LemonTea/树莓派5-HoneyTea 的所有插件？(1或y确认/2或n取消)" n; then
    should_export_plugins_macos_rpi5=true
fi

if ask "是否导出 Linux x64-LemonTea/树莓派5-HoneyTea 的所有插件？(1或y确认/2或n取消)" n; then
    should_export_plugins_linux_rpi5=true
fi

if [[ "$should_export_macos" == true ]]; then
    log "开始 macOS 编译流程"
    if [[ "$should_clean" == true ]]; then
        log "执行 clean 构建（macOS）"
        "$SCRIPT_DIR/macOS_build_release.sh" clean
    fi
    log "执行 macOS build release"
    "$SCRIPT_DIR/macOS_build_release.sh"
fi

if [[ "$should_export_cross" == true ]]; then
    log "开始交叉编译流程（Linux x64 / Raspberry Pi 5）"
    if [[ "$should_clean" == true ]]; then
        log "执行 clean 构建（cross）"
        "$SCRIPT_DIR/build_linux_x64_and_rpi5.sh" clean
    fi
    log "执行 build_linux_x64_and_rpi5"
    "$SCRIPT_DIR/build_linux_x64_and_rpi5.sh"
fi

export_plugins_to() {
    local lemon_platform="$1"
    local honey_platform="$2"
    local tag="$3"

    log "导出插件: LemonTea=$lemon_platform, HoneyTea=$honey_platform ($tag)"

    mkdir -p "$PROJECT_ROOT/dist/plugin-exports"

    local plugins=(ssh file-manager monitor cam-lan-stream)
    local invokers=(
      export_ssh_plugin_linux_x64_lemon_rpi5_honey.sh
      export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh
      export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh
      export_cam_lan_stream_plugin_linux_x64.sh
    )

    local count=${#plugins[@]}
    for ((i=0; i<count; i++)); do
        local plugin=${plugins[i]}
        local invoker_script=${invokers[i]}
        local invoker="$SCRIPT_DIR/$invoker_script"

        if [[ ! -x "$invoker" && ! -f "$invoker" ]]; then
            log "WARNING: 未找到插件导出脚本: $invoker_script (skip)"
            continue
        fi

        log "导出插件 $plugin"
        if [[ "$plugin" == "cam-lan-stream" ]]; then
            "$invoker" --lemon-platform "$lemon_platform" --output-dir "$PROJECT_ROOT/dist/plugin-exports/$plugin" || {
                log "ERROR: 插件 $plugin 导出失败"
            }
        else
            "$invoker" --lemon-platform "$lemon_platform" --honey-platform "$honey_platform" --output-dir "$PROJECT_ROOT/dist/plugin-exports/$plugin" || {
                log "ERROR: 插件 $plugin 导出失败"
            }
        fi
    done
}

if [[ "$should_export_plugins_macos_macos" == true ]]; then
    export_plugins_to "macos" "macos" "macos/macos"
fi

if [[ "$should_export_plugins_macos_rpi5" == true ]]; then
    export_plugins_to "macos" "rpi5" "macos/rpi5"
fi

if [[ "$should_export_plugins_linux_rpi5" == true ]]; then
    export_plugins_to "linux-x64" "rpi5" "linux64/rpi5"
fi

log "全部操作完成。"

cat <<EOF
结果目录：
  macOS dist: $PROJECT_ROOT/dist
  cross dist: $PROJECT_ROOT/dist/cross-targets
  插件导出: $PROJECT_ROOT/dist/plugin-exports
EOF
