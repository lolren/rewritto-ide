#!/usr/bin/env bash
set -euo pipefail

# Rewritto-ide - Native Linux Build Script
# This script builds the native executable for Rewritto-ide

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SOURCE_DIR="$PROJECT_DIR/rewritto-core/qt-native-app"
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_PREFIX="$PROJECT_DIR/install"

echo "======================================"
echo "Rewritto-ide - Native Build Script"
echo "======================================"
echo ""

usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --clean                  Clean build directory before building
  --debug                  Build in Debug mode (default: Release)
  --test                   Run tests after building
  --qt-prefix <path>       Qt install prefix (or Qt6 CMake dir)
  --no-system-toolchain    Do not auto-switch away from Conda compilers
  --help                   Show this help message

Environment overrides:
  BUILD_TYPE, CLEAN_BUILD, RUN_TESTS
  QT_PREFIX or QT6_DIR
EOF
}

detect_jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return 0
  fi
  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
    return 0
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
    return 0
  fi
  echo 1
}

ensure_local_arduino_cli() {
  local cli_tools_dir="$SOURCE_DIR/.tools/appimage/arduino-cli"
  local cli_bin="$cli_tools_dir/arduino-cli"
  local cli_archive="$cli_tools_dir/arduino-cli.tgz"
  local cli_url="${ARDUINO_CLI_URL:-https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz}"

  if [[ -x "${cli_bin}" ]]; then
    echo "  Arduino CLI runtime: ${cli_bin}"
    return 0
  fi

  mkdir -p "${cli_tools_dir}"
  echo "Preparing local arduino-cli runtime for native app..."
  if command -v curl >/dev/null 2>&1; then
    if ! curl -fL -o "${cli_archive}" "${cli_url}"; then
      echo "WARNING: Failed to download arduino-cli from: ${cli_url}"
      return 1
    fi
  elif command -v wget >/dev/null 2>&1; then
    if ! wget -O "${cli_archive}" "${cli_url}"; then
      echo "WARNING: Failed to download arduino-cli from: ${cli_url}"
      return 1
    fi
  else
    echo "WARNING: Neither curl nor wget is available; cannot download arduino-cli."
    return 1
  fi

  if ! tar -xzf "${cli_archive}" -C "${cli_tools_dir}"; then
    echo "WARNING: Failed to extract arduino-cli archive."
    return 1
  fi

  chmod +x "${cli_bin}" 2>/dev/null || true
  if [[ -x "${cli_bin}" ]]; then
    echo "  Arduino CLI runtime: ${cli_bin}"
    return 0
  fi

  echo "WARNING: arduino-cli binary not found after extraction."
  return 1
}

bundle_local_arduino_cli() {
  local cli_src="$SOURCE_DIR/.tools/appimage/arduino-cli/arduino-cli"
  local cli_dst="$BUILD_DIR/arduino-cli"

  if [[ ! -x "${cli_src}" ]]; then
    echo "WARNING: Local arduino-cli runtime missing; native app may fall back to PATH."
    return 1
  fi

  cp -f "${cli_src}" "${cli_dst}"
  chmod +x "${cli_dst}" 2>/dev/null || true
  echo "  Bundled arduino-cli next to native binary: ${cli_dst}"
  return 0
}

resolve_qt6_dir_from_prefix() {
  local prefix="$1"
  local candidate
  local candidates=(
    "$prefix"
    "$prefix/lib/cmake/Qt6"
    "$prefix/lib64/cmake/Qt6"
    "$prefix/lib/x86_64-linux-gnu/cmake/Qt6"
    "$prefix/lib/aarch64-linux-gnu/cmake/Qt6"
    "$prefix/lib/arm-linux-gnueabihf/cmake/Qt6"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}/Qt6Config.cmake" ]]; then
      echo "$candidate"
      return 0
    fi
  done

  return 1
}

discover_qt6_dir() {
  local override_prefix="$1"
  local prefix=""
  local qt_dir=""

  if [[ -n "${QT6_DIR:-}" ]] && [[ -f "${QT6_DIR}/Qt6Config.cmake" ]]; then
    echo "${QT6_DIR}"
    return 0
  fi

  if [[ -n "${override_prefix}" ]]; then
    if qt_dir="$(resolve_qt6_dir_from_prefix "${override_prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  if command -v qtpaths6 >/dev/null 2>&1; then
    prefix="$(qtpaths6 --query QT_INSTALL_PREFIX 2>/dev/null || true)"
    if [[ -n "${prefix}" ]] && qt_dir="$(resolve_qt6_dir_from_prefix "${prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  if command -v qtpaths >/dev/null 2>&1; then
    prefix="$(qtpaths --query QT_INSTALL_PREFIX 2>/dev/null || true)"
    if [[ -n "${prefix}" ]] && qt_dir="$(resolve_qt6_dir_from_prefix "${prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  if command -v qmake6 >/dev/null 2>&1; then
    prefix="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
    if [[ -n "${prefix}" ]] && qt_dir="$(resolve_qt6_dir_from_prefix "${prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  if command -v qmake >/dev/null 2>&1; then
    prefix="$(qmake -query QT_INSTALL_PREFIX 2>/dev/null || true)"
    if [[ -n "${prefix}" ]] && qt_dir="$(resolve_qt6_dir_from_prefix "${prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists Qt6Core; then
    prefix="$(pkg-config --variable=prefix Qt6Core 2>/dev/null || true)"
    if [[ -n "${prefix}" ]] && qt_dir="$(resolve_qt6_dir_from_prefix "${prefix}")"; then
      echo "${qt_dir}"
      return 0
    fi
  fi

  return 1
}

choose_system_compilers() {
  if [[ -x /usr/bin/cc ]] && [[ -x /usr/bin/c++ ]]; then
    echo "/usr/bin/cc;/usr/bin/c++"
    return 0
  fi
  if command -v gcc >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
    echo "$(command -v gcc);$(command -v g++)"
    return 0
  fi
  return 1
}

cached_cxx_compiler() {
  local cache_file="$1/CMakeCache.txt"
  if [[ -f "${cache_file}" ]]; then
    grep '^CMAKE_CXX_COMPILER:FILEPATH=' "${cache_file}" | head -n 1 | cut -d= -f2-
  fi
}

cached_source_dir() {
  local cache_file="$1/CMakeCache.txt"
  if [[ -f "${cache_file}" ]]; then
    grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' "${cache_file}" | head -n 1 | cut -d= -f2-
  fi
}

# Parse arguments
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
RUN_TESTS="${RUN_TESTS:-false}"
QT_PREFIX_OVERRIDE="${QT_PREFIX:-}"
USE_SYSTEM_TOOLCHAIN="${USE_SYSTEM_TOOLCHAIN:-true}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean)
      CLEAN_BUILD=true
      shift
      ;;
    --debug)
      BUILD_TYPE=Debug
      shift
      ;;
    --test)
      RUN_TESTS=true
      shift
      ;;
    --qt-prefix)
      if [[ $# -lt 2 ]]; then
        echo "ERROR: --qt-prefix requires a path."
        usage
        exit 1
      fi
      QT_PREFIX_OVERRIDE="$2"
      shift 2
      ;;
    --no-system-toolchain)
      USE_SYSTEM_TOOLCHAIN=false
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

if [[ ! -f "$SOURCE_DIR/CMakeLists.txt" ]]; then
  echo "ERROR: Qt native app source dir not found:"
  echo "  $SOURCE_DIR"
  echo ""
  echo "Expected: $SOURCE_DIR/CMakeLists.txt"
  exit 1
fi

if ! ensure_local_arduino_cli; then
  fallback_cli="$(command -v arduino-cli || true)"
  if [[ -n "${fallback_cli}" ]]; then
    echo "  Falling back to system arduino-cli: ${fallback_cli}"
    if [[ "${fallback_cli}" == /snap/* ]]; then
      echo "WARNING: Snap arduino-cli can hide boards/packages in native builds."
      echo "         Prefer a non-Snap arduino-cli or set ARDUINO_CLI_PATH."
    fi
  else
    echo "WARNING: No arduino-cli found in PATH; board features may not work."
  fi
fi

BUILD_TESTING=OFF
if [[ "$RUN_TESTS" = true ]]; then
  BUILD_TESTING=ON
fi

# Detect Qt6 CMake package location
QT6_DIR_CMAKE=""
if QT6_DIR_CMAKE="$(discover_qt6_dir "${QT_PREFIX_OVERRIDE}")"; then
  :
else
  QT6_DIR_CMAKE=""
fi

# Clean build if requested
if [[ "$CLEAN_BUILD" = true ]]; then
  echo "Cleaning build directory..."
  rm -rf "$BUILD_DIR"
fi

# Create build directory
echo "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$PROJECT_DIR"

# Configure with CMake
echo "Configuring with CMake..."
echo "  Build Type: $BUILD_TYPE"
echo "  Build Dir: $BUILD_DIR"

cmake_args=(
  -S "$SOURCE_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
  -DBUILD_TESTING="$BUILD_TESTING"
)

cached_source="$(cached_source_dir "${BUILD_DIR}" || true)"
if [[ -n "${cached_source:-}" ]] && [[ "${cached_source}" != "${SOURCE_DIR}" ]]; then
  echo "  Resetting CMake cache (source dir changed: ${cached_source} -> ${SOURCE_DIR})"
  rm -f "${BUILD_DIR}/CMakeCache.txt"
  rm -rf "${BUILD_DIR}/CMakeFiles"
fi

if [[ -n "${QT6_DIR_CMAKE}" ]]; then
  echo "  Qt6 Dir: ${QT6_DIR_CMAKE}"
  cmake_args+=(-DQt6_DIR="${QT6_DIR_CMAKE}")
fi

system_cc=""
system_cxx=""
if [[ "${USE_SYSTEM_TOOLCHAIN}" = true ]] && [[ -z "${CC:-}" ]] && [[ -z "${CXX:-}" ]]; then
  cached_cxx="$(cached_cxx_compiler "${BUILD_DIR}" || true)"
  if [[ -n "${CONDA_PREFIX:-}" ]] || [[ "${cached_cxx:-}" == *"miniconda"* ]] || [[ "${cached_cxx:-}" == *"/conda/"* ]]; then
    if compiler_pair="$(choose_system_compilers)"; then
      system_cc="${compiler_pair%%;*}"
      system_cxx="${compiler_pair##*;}"

      if [[ -n "${cached_cxx:-}" ]] && [[ "${cached_cxx}" != "${system_cxx}" ]]; then
        echo "  Resetting CMake cache (compiler change: ${cached_cxx} -> ${system_cxx})"
        rm -f "${BUILD_DIR}/CMakeCache.txt"
        rm -rf "${BUILD_DIR}/CMakeFiles"
      fi

      echo "  Using system compiler toolchain: ${system_cxx}"
      cmake_args+=(
        -DCMAKE_C_COMPILER="${system_cc}"
        -DCMAKE_CXX_COMPILER="${system_cxx}"
      )
    fi
  fi
fi

if ! cmake "${cmake_args[@]}"; then
  echo ""
  echo "ERROR: CMake configuration failed."
  if [[ -z "${QT6_DIR_CMAKE}" ]]; then
    echo "Qt6 CMake files were not auto-detected."
    echo "Try one of:"
    echo "  1) Install Qt6 development packages"
    echo "  2) Re-run with: $0 --qt-prefix /path/to/qt/prefix"
    echo "  3) Export Qt6_DIR=/path/to/Qt6/cmake/dir"
  fi
  if [[ -n "${CONDA_PREFIX:-}" ]]; then
    echo ""
    echo "Conda environment detected: ${CONDA_PREFIX}"
    echo "If this still fails, deactivate Conda and try again:"
    echo "  conda deactivate"
  fi
  exit 1
fi

# Build
echo ""
echo "Building Rewritto-ide..."
cmake --build "$BUILD_DIR" -j"$(detect_jobs)"
bundle_local_arduino_cli || true

# Run tests if requested
if [[ "$RUN_TESTS" = true ]]; then
  echo ""
  echo "Running tests..."
  cd "$BUILD_DIR"
  ctest --output-on-failure
fi

# Show build results
echo ""
echo "======================================"
echo "Build complete!"
echo "======================================"
echo ""
echo "Executable: $BUILD_DIR/rewritto-ide"
if [[ -x "$BUILD_DIR/arduino-cli" ]]; then
  echo "Arduino CLI: $BUILD_DIR/arduino-cli"
fi
echo ""
echo "To run Rewritto-ide:"
echo "  cd $BUILD_DIR"
echo "  ./rewritto-ide"
echo ""
echo "To install (optional):"
echo "  cmake --install $BUILD_DIR"
