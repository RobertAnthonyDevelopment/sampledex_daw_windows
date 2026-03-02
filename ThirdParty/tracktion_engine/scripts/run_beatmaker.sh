#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TARGET="BeatMakerNoRecord"
APP_PATH="${BUILD_DIR}/examples/BeatMakerNoRecord/BeatMakerNoRecord_artefacts/BeatMakerNoRecord.app"
OPEN_APP=1
CONFIGURE_IF_NEEDED=1

print_help() {
  cat <<EOF
Usage: $(basename "$0") [options]

Builds BeatMakerNoRecord and optionally opens the app.

Options:
  --no-open         Build only, do not open the .app
  --skip-configure  Skip CMake configure step
  -h, --help        Show this help message
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-open)
      OPEN_APP=0
      shift
      ;;
    --skip-configure)
      CONFIGURE_IF_NEEDED=0
      shift
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      print_help
      exit 1
      ;;
  esac
done

if [[ "${CONFIGURE_IF_NEEDED}" -eq 1 ]]; then
  echo "[run_beatmaker] Configuring CMake (JUCE via CPM if needed)..."
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DJUCE_CPM_DEVELOP=ON
fi

echo "[run_beatmaker] Building ${TARGET}..."
cmake --build "${BUILD_DIR}" --target "${TARGET}" -j8

if [[ ! -d "${APP_PATH}" ]]; then
  echo "[run_beatmaker] Build finished but app not found at:" >&2
  echo "  ${APP_PATH}" >&2
  exit 1
fi

if [[ "${OPEN_APP}" -eq 1 ]]; then
  echo "[run_beatmaker] Opening app..."
  open "${APP_PATH}"
else
  echo "[run_beatmaker] App ready at:"
  echo "  ${APP_PATH}"
fi
