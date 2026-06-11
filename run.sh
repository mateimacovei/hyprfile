#!/bin/bash
set -e

# Build directory
BUILD_DIR="build"

# Parse arguments
CLEAN_BUILD=false
DEST_DIR_SET=false
DEST_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c)
      CLEAN_BUILD=true
      shift
      ;;
    -d)
      if [[ -z "$2" ]]; then
        echo "run.sh: option '-d' requires an argument" >&2
        exit 1
      fi
      DEST_DIR_SET=true
      DEST_DIR="$2"
      shift 2
      ;;
    *)
      echo "run.sh: unknown option '$1'" >&2
      exit 1
      ;;
  esac
done

if $CLEAN_BUILD; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
  echo "Clean complete."
fi

# create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
  mkdir -p "$BUILD_DIR"
fi

# configure and build
cd "$BUILD_DIR"
cmake ..
make -j$(nproc)

if $DEST_DIR_SET; then
  echo "Build complete. Running application from $DEST_DIR..."
  ./hyprfile -d "$DEST_DIR"
else
  echo "Build complete. Running application..."
  ./hyprfile -d ~
  # ./hyprfile
fi
