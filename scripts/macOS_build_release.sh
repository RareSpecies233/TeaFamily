#!/bin/bash
# TeaFamily macOS Release Build Script
# Usage: ./scripts/macOS_build_release.sh [clean]

# Allow invoking with `sh script.sh ...` by re-execing with bash.
if [ -z "${BASH_VERSION:-}" ]; then
    exec /usr/bin/env bash "$0" "$@"
fi

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-release"
INSTALL_DIR="$PROJECT_ROOT/dist"

echo "=============================="
echo " TeaFamily macOS Release Build"
echo "=============================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Clean build if requested
if [[ "${1:-}" == "clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR" "$INSTALL_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "[1/4] Configuring CMake (Release)..."
cmake "$PROJECT_ROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

# Build
NPROC=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
echo "[2/4] Building with $NPROC parallel jobs..."
cmake --build . --config Release -j "$NPROC"

# Collect artifacts
echo "[3/4] Collecting build artifacts..."
mkdir -p "$INSTALL_DIR"/{bin,plugins,config}

# OrangeTea runtime must not include pre-installed frontend plugins.
rm -rf "$INSTALL_DIR/frontend_plugins"
mkdir -p "$INSTALL_DIR/frontend_plugins"

# Keep dist runtime-only (no headers/cmake metadata from third-party install rules).
rm -rf "$INSTALL_DIR/include" "$INSTALL_DIR/lib" "$INSTALL_DIR/share" "$INSTALL_DIR/cmake"

# Main executables
for bin in GreenTea HoneyTea LemonTea; do
    lower=$(echo "$bin" | tr '[:upper:]' '[:lower:]')
    src=""

    # Preferred modern output location
    if [[ -f "$BUILD_DIR/bin/$bin" ]]; then
        src="$BUILD_DIR/bin/$bin"
    elif [[ -f "$BUILD_DIR/$bin/$bin" ]]; then
        src="$BUILD_DIR/$bin/$bin"
    elif [[ -f "$BUILD_DIR/$bin/tea-$lower" ]]; then
        src="$BUILD_DIR/$bin/tea-$lower"
    else
        src=$(find "$BUILD_DIR" -type f \( -name "$bin" -o -name "tea-$lower" \) -perm -111 -print -quit 2>/dev/null || true)
    fi

    if [[ -n "$src" && -f "$src" ]]; then
        cp "$src" "$INSTALL_DIR/bin/$bin"
        chmod +x "$INSTALL_DIR/bin/$bin" || true
        echo "  -> $bin"
    else
        echo "  [WARN] main executable not found: $bin"
    fi
done

# Plugin executables
# Prefer manifest-declared paths (binary/server_binary/client_binary). This keeps
# runtime layout consistent with plugin.json and avoids manual copies.
echo "[3.2/4] Collecting plugin executables from manifests..."
for plugin_manifest in "$PROJECT_ROOT"/plugins/*/plugin.json; do
    [[ -f "$plugin_manifest" ]] || continue
    plugin_name=$(basename "$(dirname "$plugin_manifest")")
    mkdir -p "$INSTALL_DIR/plugins/$plugin_name"

    if command -v python3 >/dev/null 2>&1; then
        tmp_bins_file=$(mktemp)
        python3 - "$plugin_manifest" > "$tmp_bins_file" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, 'r', encoding='utf-8') as f:
    j = json.load(f)

vals = []
for key in ("binary", "server_binary", "client_binary"):
    v = j.get(key)
    if isinstance(v, str) and v:
        vals.append(v)

for v in vals:
    print(v)
PY

        while IFS= read -r rel_bin; do
            [[ -n "$rel_bin" ]] || continue
            dest="$INSTALL_DIR/plugins/$plugin_name/$rel_bin"
            mkdir -p "$(dirname "$dest")"

            src_name=$(basename "$rel_bin")
            src=""

            # Common output location for plugin targets
            if [[ -f "$BUILD_DIR/bin/$src_name" ]]; then
                src="$BUILD_DIR/bin/$src_name"
            elif [[ -f "$BUILD_DIR/$src_name" ]]; then
                src="$BUILD_DIR/$src_name"
            else
                # Fallback: search entire build tree
                src=$(find "$BUILD_DIR" -type f -name "$src_name" -perm -111 -print -quit 2>/dev/null || true)
            fi

            if [[ -n "$src" && -f "$src" ]]; then
                cp "$src" "$dest"
                chmod +x "$dest" || true
                echo "  -> plugin: $plugin_name/$rel_bin"
            else
                echo "  [WARN] plugin executable not found for $plugin_name: $rel_bin"
            fi
        done < "$tmp_bins_file"

        rm -f "$tmp_bins_file"
    else
        echo "  [WARN] python3 not found; plugin manifest-based executable copy skipped for $plugin_name"
    fi
done

# Config files
for cfg in GreenTea/config.json HoneyTea/config.json LemonTea/config.json; do
    if [[ -f "$PROJECT_ROOT/$cfg" ]]; then
        component=$(dirname "$cfg")
        mkdir -p "$INSTALL_DIR/config"
        cp "$PROJECT_ROOT/$cfg" "$INSTALL_DIR/config/${component}.json"
    fi
done

# Normalize GreenTea monitored process paths for dist layout.
if [[ -f "$INSTALL_DIR/config/GreenTea.json" ]] && command -v python3 >/dev/null 2>&1; then
    python3 - "$INSTALL_DIR/config/GreenTea.json" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, 'r', encoding='utf-8') as f:
    cfg = json.load(f)

for p in cfg.get('processes', []):
    name = p.get('name', '')
    if name == 'LemonTea':
        p['binary'] = 'bin/LemonTea'
        p['args'] = ['--config', 'config/LemonTea.json']
        p['working_dir'] = '.'
    elif name == 'HoneyTea':
        p['binary'] = 'bin/HoneyTea'
        p['args'] = ['--config', 'config/HoneyTea.json']
        p['working_dir'] = '.'

with open(path, 'w', encoding='utf-8') as f:
    json.dump(cfg, f, ensure_ascii=False, indent=4)
PY
    echo "  -> normalized GreenTea.json process paths for dist layout"
fi

# Plugin manifests
for manifest in "$PROJECT_ROOT"/plugins/*/plugin.json; do
    plugin_name=$(basename "$(dirname "$manifest")")
    mkdir -p "$INSTALL_DIR/plugins/$plugin_name"
    cp "$manifest" "$INSTALL_DIR/plugins/$plugin_name/"
done

# Frontend plugin files
for frontend_dir in "$PROJECT_ROOT"/plugins/*/frontend; do
    plugin_name=$(basename "$(dirname "$frontend_dir")")
    if [[ -d "$frontend_dir" ]]; then
        mkdir -p "$INSTALL_DIR/plugins/$plugin_name/frontend"
        cp -r "$frontend_dir"/* "$INSTALL_DIR/plugins/$plugin_name/frontend/"
    fi
done

echo ""
echo "[4/4] Building OrangeTea frontend..."
if command -v npm &>/dev/null; then
    cd "$PROJECT_ROOT/OrangeTea"
    if [[ ! -d "node_modules" ]]; then
        echo "  Installing npm dependencies..."
        npm install
    fi
    echo "  Building for production..."
    npm run build
    mkdir -p "$INSTALL_DIR/frontend"
    cp -r dist/* "$INSTALL_DIR/frontend/"
    echo "  -> OrangeTea frontend built"
else
    echo "  [WARN] npm not found, skipping OrangeTea frontend build"
fi

echo ""
echo "=============================="
echo " Build Complete!"
echo "=============================="
echo "Output directory: $INSTALL_DIR"
echo ""
ls -la "$INSTALL_DIR/bin/" 2>/dev/null || true
echo ""
echo "To run:"
echo "  cd $INSTALL_DIR"
echo "  ./bin/GreenTea    # Start watchdog daemon"
echo "  ./bin/LemonTea    # Start server"
echo "  ./bin/HoneyTea    # Start client"
