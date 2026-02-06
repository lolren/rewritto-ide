#!/usr/bin/env bash
set -euo pipefail

# Rewritto-ide - Complete Build Script
# Builds both native executable and AppImage

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SOURCE_DIR="$PROJECT_DIR/rewritto-core/qt-native-app"
BUILD_DIR="$PROJECT_DIR/build"
DIST_DIR="$PROJECT_DIR/dist"

echo "======================================"
echo "Rewritto-ide - Complete Build"
echo "======================================"
echo ""

usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --clean                  Clean build/dist directories before building
  --debug                  Build in Debug mode (default: Release)
  --no-appimage            Skip AppImage build
  --test                   Run tests after building
  --qt-prefix <path>       Qt install prefix (or Qt6 CMake dir)
  --no-system-toolchain    Do not auto-switch away from Conda compilers
  --help                   Show this help message

Environment overrides:
  BUILD_TYPE, CLEAN_BUILD, BUILD_APPIMAGE, RUN_TESTS
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

if [[ ! -f "$SOURCE_DIR/CMakeLists.txt" ]]; then
  echo "ERROR: Qt native app source dir not found:"
  echo "  $SOURCE_DIR"
  echo ""
  echo "Expected: $SOURCE_DIR/CMakeLists.txt"
  exit 1
fi

# Parse arguments
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
BUILD_APPIMAGE="${BUILD_APPIMAGE:-true}"
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
    --no-appimage)
      BUILD_APPIMAGE=false
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

echo "Build Configuration:"
echo "  Build Type: $BUILD_TYPE"
echo "  Clean Build: $CLEAN_BUILD"
echo "  Build AppImage: $BUILD_APPIMAGE"
echo "  Run Tests: $RUN_TESTS"
echo ""

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

# Clean if requested
if [[ "$CLEAN_BUILD" = true ]]; then
  echo "Cleaning build directories..."
  rm -rf "$BUILD_DIR"
  rm -rf "$DIST_DIR"
  echo "  Removed: $BUILD_DIR"
  echo "  Removed: $DIST_DIR"
fi

# Create output directories
mkdir -p "$DIST_DIR"

# Build native executable
echo ""
echo "======================================"
echo "Building Native Executable"
echo "======================================"
echo ""

mkdir -p "$BUILD_DIR"
cd "$PROJECT_DIR"

# Configure CMake
echo "Configuring CMake..."
cmake_args=(
  -S "$SOURCE_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
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
echo "Compiling..."
cmake --build "$BUILD_DIR" -j"$(detect_jobs)"
bundle_local_arduino_cli || true

# Show results
echo ""
echo "Native executable: $BUILD_DIR/rewritto-ide"
if [[ -x "$BUILD_DIR/arduino-cli" ]]; then
  echo "Arduino CLI runtime: $BUILD_DIR/arduino-cli"
fi
echo "To run: cd $BUILD_DIR && ./rewritto-ide"
echo ""

# Run tests if requested
if [[ "$RUN_TESTS" = true ]]; then
  echo "Running tests..."
  cd "$BUILD_DIR"
  ctest --output-on-failure
  cd "$PROJECT_DIR"
fi

# Build AppImage if requested
if [[ "$BUILD_APPIMAGE" = true ]]; then
  echo ""
  echo "======================================"
  echo "Building AppImage"
  echo "======================================"
  echo ""

  # Check if AppImage build script exists and is executable
  APPIMAGE_SCRIPT="$SOURCE_DIR/packaging/appimage/build-appimage.sh"

  if [[ -f "$APPIMAGE_SCRIPT" ]]; then
    echo "Running AppImage build script..."
    if ! BUILD_DIR="$BUILD_DIR" OUT_DIR="$DIST_DIR" bash "$APPIMAGE_SCRIPT"; then
      echo "WARNING: AppImage build failed. Native executable is still available."
      echo "You can re-run without AppImage packaging:"
      echo "  $0 --no-appimage"
    fi
  else
    echo "AppImage build script not found or not executable."
    echo "  Script location: $APPIMAGE_SCRIPT"
    echo ""
    echo "To build AppImage, you may need to:"
    echo "  1. Install linuxdeploy and other tools"
    echo "  2. Ensure the build script is executable: chmod +x $APPIMAGE_SCRIPT"
    echo ""
  fi
fi

# Final summary
echo ""
echo "======================================"
echo "Build Complete!"
echo "======================================"
echo ""
echo "Artifacts:"
echo "  Native executable: $BUILD_DIR/rewritto-ide"

if [[ "$BUILD_APPIMAGE" = true ]]; then
  echo "  AppImage: $DIST_DIR/Rewritto-ide-x86_64.AppImage"
fi

echo ""
echo "To run Rewritto-ide:"
if [[ "$BUILD_APPIMAGE" = true ]] && [[ -f "$DIST_DIR/Rewritto-ide-x86_64.AppImage" ]]; then
  echo "  ./dist/Rewritto-ide-x86_64.AppImage"
else
  echo "  cd $BUILD_DIR"
  echo "  ./rewritto-ide"
fi

echo ""
echo "For installation and usage, see README.md and REQUIREMENTS.txt"
