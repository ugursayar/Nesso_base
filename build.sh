#!/usr/bin/env bash
# Nesso_base build script
#
# Uses the sketch-local libraries/ copy so global library updates never
# overwrite our patches to ESP32-audioI2S.
#
# Usage:
#   ./build.sh                  — compile only
#   ./build.sh upload           — compile + upload to COM36
#   ./build.sh upload COM42     — compile + upload to a specific port

set -e

SKETCH_DIR="$(cd "$(dirname "$0")" && pwd)"
FQBN="esp32:esp32:arduino_nesso_n1"
BUILD_DIR="$SKETCH_DIR/build_temp"
LIBS_DIR="$SKETCH_DIR/libraries"
ACTION="${1:-}"
PORT="${2:-COM36}"

mkdir -p "$BUILD_DIR"

echo "============================================"
echo " Nesso_base — compile"
echo " Board : $FQBN"
echo " Libs  : $LIBS_DIR"
echo " Build : $BUILD_DIR"
echo "============================================"

arduino-cli compile \
  --fqbn "$FQBN" \
  --libraries "$LIBS_DIR" \
  --build-path "$BUILD_DIR" \
  "$SKETCH_DIR"

echo ""
echo "============================================"
echo " Compile OK"
echo "============================================"

if [[ "$ACTION" == "upload" ]]; then
  echo ""
  echo "============================================"
  echo " Upload → $PORT"
  echo "============================================"
  arduino-cli upload \
    --fqbn "$FQBN" \
    --port "$PORT" \
    --input-dir "$BUILD_DIR" \
    "$SKETCH_DIR"
  echo ""
  echo "============================================"
  echo " Upload OK"
  echo "============================================"
fi
