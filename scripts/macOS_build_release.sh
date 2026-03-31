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

# Plugin executables
for plugin_dir in "$BUILD_DIR"/plugins/*/; do
    plugin_name=$(basename "$plugin_dir")
    for exe in "$plugin_dir"*; do
        if [[ -x "$exe" && -f "$exe" ]]; then
            mkdir -p "$INSTALL_DIR/plugins/$plugin_name"
            cp "$exe" "$INSTALL_DIR/plugins/$plugin_name/"
            echo "  -> plugin: $plugin_name/$(basename "$exe")"
        fi
    done
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
