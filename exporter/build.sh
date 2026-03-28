#!/bin/bash
# Build script for eggsfm WAV exporter
# Native CLI app (not web)

set -euo pipefail

# Create output directory
mkdir -p exported

# Find ymfm source directory
YMFM_DIR="../../my-ym2612-plugin/build/_deps/ymfm-src/src"
IMGU_DIR="../../bowling/3rdparty/imgui"
SDL_DIR="../../bowling/3rdparty/SDL"

echo "Building exporter..."
echo "Using ymfm from: $YMFM_DIR"

# Check if ymfm exists
if [ ! -d "$YMFM_DIR" ]; then
    echo "Error: ymfm not found at $YMFM_DIR"
    echo "Please ensure dependencies are available"
    exit 1
fi

# Compile - OPN + required engines (SSG, ADPCM for YM2608/2610 support)
g++ -std=c++17 -O2 \
    -I"$SDL_DIR/include" \
    -I"$IMGU_DIR" \
    -I"$YMFM_DIR" \
    -I.. \
    "$YMFM_DIR/ymfm_misc.cpp" \
    "$YMFM_DIR/ymfm_ssg.cpp" \
    "$YMFM_DIR/ymfm_adpcm.cpp" \
    "$YMFM_DIR/ymfm_opn.cpp" \
    "$IMGU_DIR/imgui.cpp" \
    "$IMGU_DIR/imgui_draw.cpp" \
    "$IMGU_DIR/imgui_tables.cpp" \
    "$IMGU_DIR/imgui_widgets.cpp" \
    ../xfm_export.cpp \
    exporter.cpp \
    -o exporter \
    -lm

echo "Build complete!"
echo "Running exporter..."
echo ""

# Run exporter
./exporter ./exported

echo ""
echo "Done! Check the ./exported/ directory for WAV files."
