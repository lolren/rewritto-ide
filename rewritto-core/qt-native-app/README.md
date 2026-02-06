# Rewritto-ide (Qt Native)

Native Qt6 Arduino-compatible IDE focused on desktop-first workflow parity.

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
cmake -S rewritto-core/qt-native-app -B rewritto-core/qt-native-app/build -DCMAKE_BUILD_TYPE=Release
cmake --build rewritto-core/qt-native-app/build -j"$(nproc)"
./rewritto-core/qt-native-app/build/rewritto-ide
```

## AppImage Build

```sh
chmod +x rewritto-core/qt-native-app/packaging/appimage/build-appimage.sh
BUILD_DIR="$PWD/rewritto-core/qt-native-app/build" \
OUT_DIR="$PWD/rewritto-core/qt-native-app/dist" \
bash rewritto-core/qt-native-app/packaging/appimage/build-appimage.sh
```

AppImage output:

- `rewritto-core/qt-native-app/dist/Rewritto-ide-x86_64.AppImage`

## Release Artifacts

Native release package (tarball):

- `rewritto-core/qt-native-app/dist/rewritto-ide-linux-x86_64-native.tar.gz`
- `rewritto-core/qt-native-app/dist/rewritto-ide-windows-x86_64.zip`

Create it locally:

```sh
chmod +x rewritto-core/qt-native-app/packaging/release/package-native.sh
BUILD_DIR="$PWD/rewritto-core/qt-native-app/build" \
OUT_DIR="$PWD/rewritto-core/qt-native-app/dist" \
bash rewritto-core/qt-native-app/packaging/release/package-native.sh
```

Create the Windows package (on Windows host):

```powershell
.\build-windows.ps1
```

GitHub Releases are produced by the workflow at:

- `.github/workflows/release.yml`
