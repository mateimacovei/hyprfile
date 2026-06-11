#!/usr/bin/env bash
set -euo pipefail

# Parse args
BUILD_PROFILE=""
CLEAN=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--clean)
      CLEAN=1
      shift
      ;;
    -p|--profile)
      if [[ -n "${2-}" && "${2:0:1}" != "-" ]]; then
        BUILD_PROFILE="$2"
        shift 2
      else
        echo "[test.sh] Error: --profile requires an argument" >&2
        exit 2
      fi
      ;;
    --profile=*)
      BUILD_PROFILE="${1#*=}"
      shift
      ;;
    *)
      # unknown / positional - pass through
      shift
      ;;
  esac
done

# Default profile if none provided
if [[ -z "$BUILD_PROFILE" ]]; then
  echo "[test.sh] No build profile specified; defaulting to Debug"
  BUILD_PROFILE="Debug"
fi

# Validate profile
PROFILE_UPPER="$(echo "$BUILD_PROFILE" | tr '[:lower:]' '[:upper:]')"
if [[ "$PROFILE_UPPER" != "DEBUG" && "$PROFILE_UPPER" != "RELEASE" ]]; then
  echo "[test.sh] Error: invalid build profile '${BUILD_PROFILE}'" >&2
  echo "[test.sh] Expected values: Debug, Release" >&2
  exit 2
fi

# Normalize profile case
if [[ "$PROFILE_UPPER" == "DEBUG" ]]; then
  BUILD_PROFILE="Debug"
else
  BUILD_PROFILE="Release"
fi

# Build directory (separate per profile)
BUILD_DIR="build/${BUILD_PROFILE}"

NUM_PROCS=1
if command -v nproc >/dev/null 2>&1; then
  NUM_PROCS=$(nproc)
fi

if [ "$CLEAN" -eq 1 ]; then
  echo "[test.sh] Cleaning build directory: ${BUILD_DIR}"
  # Safety: ensure we're not about to rm -rf an important path
  if [ -z "${BUILD_DIR}" ] || [ "${BUILD_DIR}" = "/" ]; then
    echo "Refusing to remove dangerous build dir: ${BUILD_DIR}" >&2
    exit 2
  fi
  rm -rf "${BUILD_DIR}"
fi

echo "[test.sh] Configuring (build dir: ${BUILD_DIR}, profile: ${BUILD_PROFILE})"
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_PROFILE}"

echo "[test.sh] Building (parallel: ${NUM_PROCS})"
cmake --build "${BUILD_DIR}" --config "${BUILD_PROFILE}" -- -j"${NUM_PROCS}"

echo "[test.sh] Running tests"
ctest --output-on-failure -j "${NUM_PROCS}" -C "${BUILD_PROFILE}" --test-dir "${BUILD_DIR}"

echo "[test.sh] All done"
