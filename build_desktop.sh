#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-debug}"

case "${MODE}" in
  debug)
    BUILD_DIR="${ROOT_DIR}/desktop/build"
    CMAKE_BUILD_TYPE="Debug"
    ;;
  release)
    BUILD_DIR="${ROOT_DIR}/desktop/build-release"
    CMAKE_BUILD_TYPE="Release"
    ;;
  *)
    echo "Usage: $0 [debug|release]"
    exit 1
    ;;
esac

# First configure only when build files do not exist.
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  cmake -S "${ROOT_DIR}/desktop" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
fi

cmake --build "${BUILD_DIR}" -j

echo "Desktop build complete (${MODE})."
