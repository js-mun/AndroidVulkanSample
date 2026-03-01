#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Binary not found: ${BIN_PATH}"
  echo "Run ./build_desktop.sh ${MODE} first."
  exit 1
fi

exec "${BIN_PATH}" "${ASSET_PATH}"
