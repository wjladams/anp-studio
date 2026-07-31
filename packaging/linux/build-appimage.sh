#!/usr/bin/env bash
# Build a portable AppImage for ANP Studio using linuxdeploy + Qt plugin.
#
# Usage:
#   packaging/linux/build-appimage.sh <built-anpstudio-binary> <output-AppImage-path>
#
# Requires: linuxdeploy, linuxdeploy-plugin-qt on PATH (or downloaded by CI).
# Expects QMAKE (or Qt's qmake6) available for the Qt plugin.

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <built-anpstudio-binary> <output-AppImage-path>" >&2
  exit 1
fi

BINARY="$(realpath "$1")"
OUTPUT="$(realpath -m "$2")"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
STAGE="$(mktemp -d)"

if [[ ! -x "${BINARY}" ]]; then
  echo "Binary not found or not executable: ${BINARY}" >&2
  exit 1
fi

mkdir -p "${STAGE}/usr/bin" "${STAGE}/usr/share/applications" \
  "${STAGE}/usr/share/icons/hicolor/256x256/apps" \
  "${STAGE}/usr/share/anpstudio"
cp "${BINARY}" "${STAGE}/usr/bin/anpstudio"
chmod +x "${STAGE}/usr/bin/anpstudio"
cp "${SCRIPT_DIR}/anpstudio.desktop" "${STAGE}/usr/share/applications/anpstudio.desktop"
cp "${REPO_ROOT}/gui/resources/anpstudio-256.png" \
  "${STAGE}/usr/share/icons/hicolor/256x256/apps/anpstudio.png"
cp -r "${REPO_ROOT}/samples" "${STAGE}/usr/share/anpstudio/samples"

# AppDir root icon (linuxdeploy also uses this naming convention).
cp "${REPO_ROOT}/gui/resources/anpstudio-256.png" "${STAGE}/anpstudio.png"

if ! command -v linuxdeploy >/dev/null 2>&1; then
  echo "linuxdeploy not found on PATH" >&2
  exit 1
fi

# Prefer qmake from the Qt install used to build; fall back to qmake6/qmake.
if [[ -z "${QMAKE:-}" ]]; then
  if command -v qmake6 >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake6)"
  elif command -v qmake >/dev/null 2>&1; then
    export QMAKE="$(command -v qmake)"
  else
    echo "QMAKE not set and qmake/qmake6 not found on PATH" >&2
    exit 1
  fi
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "${STAGE}" "${WORKDIR}"' EXIT

export APPIMAGE_EXTRACT_AND_RUN=1
(
  cd "${WORKDIR}"
  linuxdeploy \
    --appdir "${STAGE}" \
    --executable "${STAGE}/usr/bin/anpstudio" \
    --desktop-file "${STAGE}/usr/share/applications/anpstudio.desktop" \
    --icon-file "${STAGE}/anpstudio.png" \
    --plugin qt \
    --output appimage
)

BUILT="$(find "${WORKDIR}" -maxdepth 1 -name '*.AppImage' -print -quit)"
if [[ -z "${BUILT}" ]]; then
  echo "linuxdeploy did not produce an AppImage in ${WORKDIR}" >&2
  exit 1
fi

mkdir -p "$(dirname "${OUTPUT}")"
mv "${BUILT}" "${OUTPUT}"
chmod +x "${OUTPUT}"
echo "Wrote ${OUTPUT}"
