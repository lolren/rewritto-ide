#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
APP_NAME="rewritto-ide"
DESKTOP_ID="com.rewritto.ide"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BIN_PATH="${BUILD_DIR}/${APP_NAME}"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/dist}"

TOOLS_DIR="${ROOT_DIR}/.tools/appimage"
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="${TOOLS_DIR}/appimagetool-x86_64.AppImage"

ALS_VERSION="0.7.7"
ALS_URL="https://github.com/arduino/arduino-language-server/releases/download/${ALS_VERSION}/arduino-language-server_${ALS_VERSION}_Linux_64bit.tar.gz"
ARDUINO_CLI_URL="${ARDUINO_CLI_URL:-https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz}"

ICON_SRC="${ROOT_DIR}/resources/icons/app-icon.svg"
APPSTREAM_META_SRC="${ROOT_DIR}/packaging/appimage/${DESKTOP_ID}.appdata.xml"
DESKTOP_FILE="${ROOT_DIR}/packaging/appimage/${DESKTOP_ID}.desktop"

mkdir -p "${TOOLS_DIR}" "${OUT_DIR}"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Binary not found: ${BIN_PATH}"
  echo "Build first:"
  echo "  cmake -S ${ROOT_DIR} -B ${BUILD_DIR}"
  echo "  cmake --build ${BUILD_DIR} -j"
  exit 1
fi

if [[ ! -f "${DESKTOP_FILE}" ]]; then
  echo "Desktop file not found: ${DESKTOP_FILE}"
  exit 1
fi

if [[ ! -f "${ICON_SRC}" ]]; then
  echo "Icon not found: ${ICON_SRC}"
  exit 1
fi

download_if_missing() {
  local url="$1"
  local out="$2"
  if [[ -x "${out}" ]]; then
    return 0
  fi
  echo "Downloading ${out}"
  curl -L -o "${out}" "${url}"
  chmod +x "${out}"
}

# linuxdeploy and plugin-qt "continuous" builds
download_if_missing \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
  "${LINUXDEPLOY}"
download_if_missing \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "${LINUXDEPLOY_QT}"
download_if_missing \
  "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" \
  "${APPIMAGETOOL}"

ln -sf "$(basename "${LINUXDEPLOY}")" "${TOOLS_DIR}/linuxdeploy"
ln -sf "$(basename "${LINUXDEPLOY_QT}")" "${TOOLS_DIR}/linuxdeploy-plugin-qt"

ensure_arduino_language_server() {
  local dest_dir="${TOOLS_DIR}/arduino-language-server/${ALS_VERSION}"
  local dest_bin="${dest_dir}/arduino-language-server"
  if [[ ! -x "${dest_bin}" ]]; then
    mkdir -p "${dest_dir}"
    echo "Downloading arduino-language-server ${ALS_VERSION}"
    curl -L -o "${dest_dir}/als.tgz" "${ALS_URL}"
    tar -xzf "${dest_dir}/als.tgz" -C "${dest_dir}"
    chmod +x "${dest_bin}"
  fi
  echo "${dest_bin}"
}

ensure_arduino_cli() {
  local dest_dir="${TOOLS_DIR}/arduino-cli"
  local dest_bin="${dest_dir}/arduino-cli"
  if [[ ! -x "${dest_bin}" ]]; then
    mkdir -p "${dest_dir}"
    echo "Downloading arduino-cli"
    curl -L -o "${dest_dir}/arduino-cli.tgz" "${ARDUINO_CLI_URL}"
    tar -xzf "${dest_dir}/arduino-cli.tgz" -C "${dest_dir}"
    chmod +x "${dest_bin}"
  fi
  echo "${dest_bin}"
}

bundle_extra_qt_platform_plugins() {
  local qmake_bin="${QMAKE:-}"
  if [[ -z "${qmake_bin}" ]]; then
    qmake_bin="$(command -v qmake6 || command -v qmake || true)"
  fi

  local plugins_dir=""
  if [[ -n "${qmake_bin}" ]]; then
    plugins_dir="$("${qmake_bin}" -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  fi
  if [[ -z "${plugins_dir}" ]]; then
    plugins_dir="/usr/lib/x86_64-linux-gnu/qt6/plugins"
  fi

  local src_dir="${plugins_dir}/platforms"
  local dest_dir="${APPDIR}/usr/plugins/platforms"
  mkdir -p "${dest_dir}"

  local plugin
  for plugin in libqoffscreen.so libqminimal.so; do
    if [[ -f "${src_dir}/${plugin}" ]]; then
      cp "${src_dir}/${plugin}" "${dest_dir}/"
    fi
  done
}

ensure_clangd_18() {
  local clangd_path=""
  if command -v clangd-18 >/dev/null 2>&1; then
    clangd_path="$(command -v clangd-18)"
  elif command -v clangd >/dev/null 2>&1; then
    clangd_path="$(command -v clangd)"
  fi

  if [[ -n "${clangd_path}" ]]; then
    echo "Using clangd from PATH: ${clangd_path}"
    cp "${clangd_path}" "${APPDIR}/usr/bin/clangd"
    return 0
  fi

  if ! command -v apt-get >/dev/null 2>&1 || ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "WARNING: clangd not found and apt tools are unavailable."
    echo "WARNING: Continuing without bundled clangd (LSP features may be limited)."
    return 0
  fi

  local root="${TOOLS_DIR}/clangd-18/root"
  local clangd_src="${root}/usr/lib/llvm-18/bin/clangd"

  mkdir -p "${root}"
  local debdir="${TOOLS_DIR}/clangd-18/debs"
  mkdir -p "${debdir}"

  download_and_extract_pkg() {
    local pkg="$1"
    echo "Downloading ${pkg} (no root required)"
    if ! (cd "${debdir}" && apt-get download "${pkg}" >/dev/null 2>&1); then
      return 1
    fi
    local deb
    deb="$(ls -t "${debdir}/${pkg}"_*.deb 2>/dev/null | head -n 1 || true)"
    if [[ -z "${deb}" ]]; then
      return 1
    fi
    dpkg-deb -x "${deb}" "${root}"
  }

  download_and_extract_first_available() {
    local pkg
    for pkg in "$@"; do
      if download_and_extract_pkg "${pkg}"; then
        return 0
      fi
    done
    return 1
  }

  if [[ ! -x "${clangd_src}" ]]; then
    echo "Preparing clangd-18 and dependencies (no root required)"
    if ! download_and_extract_first_available clangd-18; then
      echo "WARNING: Could not download clangd-18 via apt."
      echo "WARNING: Continuing without bundled clangd (install clangd-18 to include it)."
      return 0
    fi
    download_and_extract_first_available libclang-cpp18 || true
    download_and_extract_first_available libllvm18 || true
    download_and_extract_first_available libabsl20220623t64 libabsl20220623 || true
    download_and_extract_first_available libgrpc++1.51t64 libgrpc++1.51 || true
    download_and_extract_first_available libgrpc29t64 libgrpc29 || true
    download_and_extract_first_available libprotobuf32t64 libprotobuf32 || true
    download_and_extract_first_available libprotoc32t64 libprotoc32 || true
    download_and_extract_first_available libclang-common-18-dev || true
  fi

  # Handle incremental updates: older caches might miss newly added dependencies.
  if ! compgen -G "${root}/usr/lib/x86_64-linux-gnu/libprotoc.so.32*" >/dev/null; then
    download_and_extract_first_available libprotoc32t64 libprotoc32 || true
  fi

  if [[ ! -x "${clangd_src}" ]]; then
    echo "WARNING: clangd-18 could not be prepared (missing ${clangd_src})."
    echo "WARNING: Continuing without bundled clangd."
    return 0
  fi

  cp "${clangd_src}" "${APPDIR}/usr/bin/clangd"

  # clangd dependencies and resource headers
  mkdir -p "${APPDIR}/usr/lib/x86_64-linux-gnu"
  if compgen -G "${root}/usr/lib/x86_64-linux-gnu/*.so*" >/dev/null; then
    cp -a "${root}/usr/lib/x86_64-linux-gnu/"*.so* \
      "${APPDIR}/usr/lib/x86_64-linux-gnu/"
  fi
  if [[ -d "${root}/usr/lib/llvm-18/lib" ]]; then
    mkdir -p "${APPDIR}/usr/lib/llvm-18"
    cp -a "${root}/usr/lib/llvm-18/lib" "${APPDIR}/usr/lib/llvm-18/"
  fi
}

APPDIR="$(mktemp -d)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${APPDIR}" "${WORKDIR}"' EXIT

mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
mkdir -p "${APPDIR}/usr/share/metainfo"

cp "${BIN_PATH}" "${APPDIR}/usr/bin/${APP_NAME}"
cp "${DESKTOP_FILE}" "${APPDIR}/usr/share/applications/${DESKTOP_ID}.desktop"
cp "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${DESKTOP_ID}.svg"
if [[ -f "${APPSTREAM_META_SRC}" ]]; then
  cp "${APPSTREAM_META_SRC}" "${APPDIR}/usr/share/metainfo/$(basename "${APPSTREAM_META_SRC}")"
fi

ensure_clangd_18
ALS_BIN="$(ensure_arduino_language_server)"
CLI_BIN="$(ensure_arduino_cli)"

export PATH="${TOOLS_DIR}:${PATH}"
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$(command -v qmake6 || command -v qmake)"
export QT_QMAKE_EXECUTABLE="${QMAKE}"
export LD_LIBRARY_PATH="${APPDIR}/usr/lib/x86_64-linux-gnu:${APPDIR}/usr/lib/llvm-18/lib:${APPDIR}/usr/lib:${LD_LIBRARY_PATH:-}"

exe_args=(--executable "${APPDIR}/usr/bin/${APP_NAME}")
if [[ -x "${APPDIR}/usr/bin/clangd" ]]; then
  exe_args+=(--executable "${APPDIR}/usr/bin/clangd")
fi

# Build AppDir with Qt deps first.
(cd "${WORKDIR}" && \
  "${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    "${exe_args[@]}" \
    --desktop-file "${APPDIR}/usr/share/applications/${DESKTOP_ID}.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${DESKTOP_ID}.svg" \
    --plugin qt)

bundle_extra_qt_platform_plugins

# Copy tools after linuxdeploy so they won't get patched/stripped (some ELF patching breaks als).
if [[ -x "${ALS_BIN}" ]]; then
  cp "${ALS_BIN}" "${APPDIR}/usr/bin/arduino-language-server"
fi
if [[ -x "${CLI_BIN}" ]]; then
  cp "${CLI_BIN}" "${APPDIR}/usr/bin/arduino-cli"
fi

# Package into AppImage.
(cd "${WORKDIR}" && APPIMAGE_EXTRACT_AND_RUN=1 \
  "${APPIMAGETOOL}" "${APPDIR}" "Rewritto-ide-x86_64.AppImage")

shopt -s nullglob
for img in "${WORKDIR}"/*.AppImage; do
  mv -f "${img}" "${OUT_DIR}/"
done

echo "Done. Output in: ${OUT_DIR}"
