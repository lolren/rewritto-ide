#!/usr/bin/env bash
set -euo pipefail

# Rewritto Ide - Complete Build Script
# Builds both native executable and AppImage

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SOURCE_DIR="$PROJECT_DIR/arduino-ide/qt-native-app"
BUILD_DIR="$PROJECT_DIR/build"
DIST_DIR="$PROJECT_DIR/dist"

echo "======================================"
echo "Rewritto Ide - Complete Build"
echo "======================================"
echo ""

# Check for Qt6
if ! pkg-config --exists Qt6Core; then
  echo "ERROR: Qt6 not found!"
  echo ""
  echo "Please install Qt6 development packages:"
  echo "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-tools-dev"
  echo "  Arch: sudo pacman -S qt6-base qt6-tools"
  echo "  Fedora: sudo dnf install qt6-qtbase qt6-qttools"
  echo ""
  exit 1
fi

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
    --help)
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --clean          Clean build directories before building"
      echo "  --debug          Build in Debug mode (default: Release)"
      echo "  --no-appimage    Skip AppImage build"
      echo "  --test           Run tests after building"
      echo "  --help           Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

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
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DBUILD_TESTING="$BUILD_TESTING"

# Build
echo ""
echo "Compiling..."
cmake --build "$BUILD_DIR" -j$(nproc)

# Show results
echo ""
echo "Native executable: $BUILD_DIR/rewritto-ide"
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
    BUILD_DIR="$BUILD_DIR" OUT_DIR="$DIST_DIR" bash "$APPIMAGE_SCRIPT"
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
  echo "  AppImage: $DIST_DIR/Rewritto_Ide-x86_64.AppImage"
fi

echo ""
echo "To run Rewritto Ide:"
if [[ "$BUILD_APPIMAGE" = true ]] && [[ -f "$DIST_DIR/Rewritto_Ide-x86_64.AppImage" ]]; then
  echo "  ./dist/Rewritto_Ide-x86_64.AppImage"
else
  echo "  cd $BUILD_DIR"
  echo "  ./rewritto-ide"
fi

echo ""
echo "For installation and usage, see README.md and REQUIREMENTS.txt"
