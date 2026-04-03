#!/usr/bin/env bash
# TeaFamily macOS All-in-One Build + Plugin Export Script
# Usage: ./scripts/macOS_all_in_one.sh
# 1. 是否导出 macOS 版本 LemonTea/GreenTea/HoneyTea
# 2. 是否 clean 构建
# 3. 是否导出 Linux x64 LemonTea/GreenTea + Raspberry Pi 5 GreenTea/HoneyTea
# 4. 是否导出 macOS-LemonTea/macOS-HoneyTea 的所有插件
# 5. 是否导出 macOS-LemonTea/rpi5-HoneyTea 的所有插件
# 6. 是否导出 linux-x64-LemonTea/rpi5-HoneyTea 的所有插件

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
        case "${ans,,}" in
            y|yes) return 0 ;; 
            n|no) return 1 ;; 
            *) echo "请输入 y 或 n（大小写均可）" ;; 
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

if ask "是否导出 macOS 版本 LemonTea/GreenTea/HoneyTea？(y/n)" n; then
    should_export_macos=true
fi

if ask "是否 clean 构建？(y/n)" n; then
    should_clean=true
fi

if ask "是否导出 Linux x64 版本 LemonTea/GreenTea 与 树莓派5 版本 GreenTea/HoneyTea？(y/n)" n; then
    should_export_cross=true
fi

if ask "是否导出 macOS-LemonTea/macOS-HoneyTea 的所有插件？(y/n)" n; then
    should_export_plugins_macos_macos=true
fi

if ask "是否导出 macOS-LemonTea/树莓派5-HoneyTea 的所有插件？(y/n)" n; then
    should_export_plugins_macos_rpi5=true
fi

if ask "是否导出 Linux x64-LemonTea/树莓派5-HoneyTea 的所有插件？(y/n)" n; then
    should_export_plugins_linux_rpi5=true
fi

# Build macOS runtime
if [[ "$should_export_macos" == true ]]; then
    log "开始 macOS 编译流程"
    if [[ "$should_clean" == true ]]; then
        log "执行 clean 构建（macOS）"
        "$SCRIPT_DIR/macOS_build_release.sh" clean
    fi
    log "执行 macOS build release"
    "$SCRIPT_DIR/macOS_build_release.sh"
fi

# Build cross targets
if [[ "$should_export_cross" == true ]]; then
    log "开始交叉编译流程（Linux x64 / Raspberry Pi 5）"
    if [[ "$should_clean" == true ]]; then
        log "执行 clean 构建（cross）"
        "$SCRIPT_DIR/build_linux_x64_and_rpi5.sh" clean
    fi
    log "执行 build_linux_x64_and_rpi5"
    "$SCRIPT_DIR/build_linux_x64_and_rpi5.sh"
fi

# Helper: 插件导出函数
export_plugins_to() {
    local lemon_platform="$1"
    local honey_platform="$2"
    local tag="$3"

    log "导出插件: LemonTea=$lemon_platform, HoneyTea=$honey_platform ($tag)"

    mkdir -p "$PROJECT_ROOT/dist/plugin-exports"

    # 支持插件目录中可用插件
    local plugins=(ssh file-manager monitor cam-lan-stream)
    for plugin in "${plugins[@]}"; do
        local invoker="$SCRIPT_DIR/export_${plugin//-/}_plugin_linux_x64_lemon_rpi5_honey.sh"
        if [[ ! -x "$invoker" && ! -f "$invoker" ]]; then
            invoker="$SCRIPT_DIR/export_plugin_linux_x64_lemon_rpi5_honey.sh"
        fi

        if [[ "$plugin" == "cam-lan-stream" ]]; then
            invoker="$SCRIPT_DIR/export_cam_lan_stream_plugin_linux_x64_lemon_rpi5_honey.sh"
        fi

        if [[ ! -x "$invoker" && ! -f "$invoker" ]]; then
            log "WARNING: 未找到插件导出脚本: $plugin (skip)"
            continue
        fi

        log "导出插件 $plugin"
        "$invoker" "$plugin" "tea-plugin-${plugin//-/}" "tea-plugin-${plugin//-/}" --lemon-platform "$lemon_platform" --honey-platform "$honey_platform" || {
            log "ERROR: 插件 $plugin 导出失败"
        }
    done
}

# macOS-LemonTea/macOS-HoneyTea
if [[ "$should_export_plugins_macos_macos" == true ]]; then
    export_plugins_to "macos" "macos" "macos/macos"
fi

# macOS-LemonTea/rpi5-HoneyTea
if [[ "$should_export_plugins_macos_rpi5" == true ]]; then
    export_plugins_to "macos" "rpi5" "macos/rpi5"
fi

# linux-x64-LemonTea/rpi5-HoneyTea
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
