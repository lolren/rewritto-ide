# Rewritto Ide (Qt Native)

Native Qt6 Arduino-compatible IDE focused on Arduino IDE 2.x workflow parity.

## Build Dependencies (Linux)

- `cmake` (>= 3.21)
- `build-essential`
- `pkg-config`
- `qt6-base-dev`
- `qt6-tools-dev`

Example (Ubuntu/Debian):

```sh
sudo apt update
sudo apt install -y cmake build-essential pkg-config qt6-base-dev qt6-tools-dev
```

## Native Build

```sh
cmake -S arduino-ide/qt-native-app -B arduino-ide/qt-native-app/build -DCMAKE_BUILD_TYPE=Release
cmake --build arduino-ide/qt-native-app/build -j"$(nproc)"
./arduino-ide/qt-native-app/build/rewritto-ide
```

## AppImage Build

```sh
chmod +x arduino-ide/qt-native-app/packaging/appimage/build-appimage.sh
BUILD_DIR="$PWD/arduino-ide/qt-native-app/build" \
OUT_DIR="$PWD/arduino-ide/qt-native-app/dist" \
bash arduino-ide/qt-native-app/packaging/appimage/build-appimage.sh
```

AppImage output:

- `arduino-ide/qt-native-app/dist/Rewritto_Ide-x86_64.AppImage`

## Release Artifacts

Native release package (tarball):

- `arduino-ide/qt-native-app/dist/rewritto-ide-linux-x86_64-native.tar.gz`

Create it locally:

```sh
chmod +x arduino-ide/qt-native-app/packaging/release/package-native.sh
BUILD_DIR="$PWD/arduino-ide/qt-native-app/build" \
OUT_DIR="$PWD/arduino-ide/qt-native-app/dist" \
bash arduino-ide/qt-native-app/packaging/release/package-native.sh
```

GitHub Releases are produced by the workflow at:

- `arduino-ide/.github/workflows/release.yml`
