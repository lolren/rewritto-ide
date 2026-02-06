#!/usr/bin/env bash
set -euo pipefail

# Rewritto Ide - Native Linux Build Script
# This script builds the native executable for Rewritto Ide

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SOURCE_DIR="$PROJECT_DIR/arduino-ide/qt-native-app"
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_PREFIX="$PROJECT_DIR/install"

echo "======================================"
echo "Rewritto Ide - Native Build Script"
echo "======================================"
echo ""

# Parse arguments
BUILD_TYPE="${BUILD_TYPE:-Release}"
CLEAN_BUILD="${CLEAN_BUILD:-false}"
RUN_TESTS="${RUN_TESTS:-false}"

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
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --clean    Clean build directory before building"
      echo "  --debug    Build in Debug mode (default: Release)"
      echo "  --test     Run tests after building"
      echo "  --help     Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
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

BUILD_TESTING=OFF
if [[ "$RUN_TESTS" = true ]]; then
  BUILD_TESTING=ON
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

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DBUILD_TESTING="$BUILD_TESTING"

# Build
echo ""
echo "Building Rewritto Ide..."
cmake --build "$BUILD_DIR" -j$(nproc)

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
echo ""
echo "To run Rewritto Ide:"
echo "  cd $BUILD_DIR"
echo "  ./rewritto-ide"
echo ""
echo "To install (optional):"
echo "  cmake --install $BUILD_DIR"
