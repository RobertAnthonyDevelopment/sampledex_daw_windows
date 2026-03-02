#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TARGET="TheSampledexWorkflow"
APP_PATH="${BUILD_DIR}/BeatMakerNoRecord_build/TheSampledexWorkflow_artefacts/TheSampledexWorkflow.app"
TRACKTION_ROOT="${ROOT_DIR}/../ThirdParty/tracktion_engine"
OPEN_APP=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-open)
      OPEN_APP=0
      shift
      ;;
    -h|--help)
      echo "Usage: $(basename "$0") [--no-open]"
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! -d "${TRACKTION_ROOT}" ]]; then
  echo "[BeatMakerApp] Engine folder not found at ${TRACKTION_ROOT}" >&2
  exit 1
fi

if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
  cached_home="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${BUILD_DIR}/CMakeCache.txt" | head -n 1 || true)"
  if [[ -n "${cached_home}" && "${cached_home}" != "${ROOT_DIR}" ]]; then
    echo "[BeatMakerApp] Clearing stale build cache (source moved from ${cached_home})..."
    rm -rf "${BUILD_DIR}"
  fi
fi

echo "[BeatMakerApp] Configuring..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DJUCE_CPM_DEVELOP=ON

echo "[BeatMakerApp] Building ${TARGET}..."
cmake --build "${BUILD_DIR}" --target "${TARGET}" -j8

if [[ ! -d "${APP_PATH}" ]]; then
  echo "[BeatMakerApp] App not found at ${APP_PATH}" >&2
  exit 1
fi

if [[ "${OPEN_APP}" -eq 1 ]]; then
  echo "[BeatMakerApp] Opening app..."
  open "${APP_PATH}"
else
  echo "[BeatMakerApp] App ready: ${APP_PATH}"
fi
