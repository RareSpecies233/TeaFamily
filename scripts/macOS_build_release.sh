#!/bin/bash
# TeaFamily macOS Release Build Script
# Usage: ./scripts/macOS_build_release.sh [clean]

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

# Try to run `cmake --install` if the project provides install rules. This will
# let CMake place targets into the specified prefix when supported.
echo "[3.1/4] Running 'cmake --install' (if supported)..."
if cmake --install . --config Release --prefix "$INSTALL_DIR" 2>/dev/null; then
    echo "  -> cmake --install completed"
else
    echo "  -> cmake --install skipped or no install rules (continuing with fallback copies)"
fi

# Main executables
for bin in GreenTea HoneyTea LemonTea; do
    if [[ -f "$BUILD_DIR/$bin/$bin" ]]; then
        cp "$BUILD_DIR/$bin/$bin" "$INSTALL_DIR/bin/"
        echo "  -> $bin"
    elif [[ -f "$BUILD_DIR/$bin/tea-$(echo "$bin" | tr '[:upper:]' '[:lower:]')" ]]; then
        cp "$BUILD_DIR/$bin/tea-$(echo "$bin" | tr '[:upper:]' '[:lower:]')" "$INSTALL_DIR/bin/"
        echo "  -> $bin"
    fi
done

# Fallback: if the expected executables were not copied by the above rules or by
# `cmake --install`, try to locate them anywhere under the build directory and
# copy the first match. This handles different CMake configurations/output paths.
for bin in GreenTea HoneyTea LemonTea; do
    if [[ ! -f "$INSTALL_DIR/bin/$bin" && ! -f "$INSTALL_DIR/bin/tea-$(echo "$bin" | tr '[:upper:]' '[:lower:]')" ]]; then
        found=$(find "$BUILD_DIR" -type f -name "$bin" -perm -111 -print -quit 2>/dev/null || true)
        if [[ -n "$found" ]]; then
            cp "$found" "$INSTALL_DIR/bin/"
            echo "  -> found and copied $bin from $found"
            continue
        fi
        found2=$(find "$BUILD_DIR" -type f -name "tea-$(echo "$bin" | tr '[:upper:]' '[:lower:]')" -perm -111 -print -quit 2>/dev/null || true)
        if [[ -n "$found2" ]]; then
            cp "$found2" "$INSTALL_DIR/bin/"
            echo "  -> found and copied $(basename "$found2") from $found2"
        fi
    fi
done

# Plugin executables
# Copy plugin executables from expected build plugins directory
for plugin_dir in "$BUILD_DIR"/plugins/*/; do
    if [[ -d "$plugin_dir" ]]; then
        plugin_name=$(basename "$plugin_dir")
        for exe in "$plugin_dir"*; do
            if [[ -x "$exe" && -f "$exe" ]]; then
                mkdir -p "$INSTALL_DIR/plugins/$plugin_name"
                cp "$exe" "$INSTALL_DIR/plugins/$plugin_name/"
                echo "  -> plugin: $plugin_name/$(basename "$exe")"
            fi
        done
    fi
done

# Fallback: attempt to find plugin executables anywhere under the build tree
for plugin_manifest in "$PROJECT_ROOT"/plugins/*/plugin.json; do
    plugin_name=$(basename "$(dirname "$plugin_manifest")")
    if [[ -d "$INSTALL_DIR/plugins/$plugin_name" ]]; then
        continue
    fi
    # look for executables matching plugin name or 'tea-plugin-*'
    found=$(find "$BUILD_DIR" -type f \( -name "$plugin_name" -o -name "tea-plugin-$plugin_name" -o -name "tea-plugin-*" \) -perm -111 -print -quit 2>/dev/null || true)
    if [[ -n "$found" ]]; then
        mkdir -p "$INSTALL_DIR/plugins/$plugin_name"
        cp "$found" "$INSTALL_DIR/plugins/$plugin_name/"
        echo "  -> plugin fallback: $plugin_name/$(basename "$found")"
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
