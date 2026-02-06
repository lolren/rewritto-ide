#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/dist}"
BIN_PATH="${BUILD_DIR}/rewritto-ide"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Native binary not found: ${BIN_PATH}"
  echo "Build first:"
  echo "  cmake -S ${ROOT_DIR} -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release"
  echo "  cmake --build ${BUILD_DIR} -j\$(nproc)"
  exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

PKG_NAME="rewritto-ide-linux-x86_64"
PKG_DIR="${WORK_DIR}/${PKG_NAME}"
mkdir -p "${PKG_DIR}"
mkdir -p "${OUT_DIR}"

cp "${BIN_PATH}" "${PKG_DIR}/rewritto-ide"
cp "${ROOT_DIR}/LICENSE.txt" "${PKG_DIR}/LICENSE.txt"
cp "${ROOT_DIR}/packaging/appimage/com.rewritto.ide.desktop" "${PKG_DIR}/rewritto-ide.desktop"

cat > "${PKG_DIR}/README.txt" <<'EOF'
Rewritto Ide (native Linux binary)

Run:
  ./rewritto-ide

Notes:
- Use the same machine profile used for building (Linux x86_64).
- For a fully bundled package with dependencies, use the AppImage artifact.
EOF

ARCHIVE_PATH="${OUT_DIR}/${PKG_NAME}-native.tar.gz"
tar -C "${WORK_DIR}" -czf "${ARCHIVE_PATH}" "${PKG_NAME}"

echo "Native package created:"
echo "  ${ARCHIVE_PATH}"
