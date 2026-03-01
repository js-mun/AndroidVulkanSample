#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DO_BUILD=0

if [[ "${1:-}" == "--build" ]]; then
  DO_BUILD=1
  shift
fi

MODE="${1:-debug}"
ASSET_PATH="${2:-app/src/main/assets/}"

case "${MODE}" in
  debug)
    BIN_PATH="${ROOT_DIR}/desktop/build/mygame_desktop"
    ;;
  release)
    BIN_PATH="${ROOT_DIR}/desktop/build-release/mygame_desktop"
    ;;
  *)
    echo "Usage: $0 [debug|release] [asset_path]"
    exit 1
    ;;
esac

if [[ "${DO_BUILD}" -eq 1 ]]; then
  "${ROOT_DIR}/build_desktop.sh" "${MODE}"
fi

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Binary not found: ${BIN_PATH}"
  echo "Run ./build_desktop.sh ${MODE} first."
  exit 1
fi

exec "${BIN_PATH}" "${ASSET_PATH}"
