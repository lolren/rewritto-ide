# Rewritto Ide

Rewritto Ide is a native Qt6 rewrite focused on Arduino IDE 2.x parity.

## Build (Native)

```sh
cmake -S qt-native-app -B qt-native-app/build -DCMAKE_BUILD_TYPE=Release
cmake --build qt-native-app/build -j"$(nproc)"
./qt-native-app/build/rewritto-ide
```

## Build (AppImage)

```sh
chmod +x qt-native-app/packaging/appimage/build-appimage.sh
BUILD_DIR="$PWD/qt-native-app/build" \
OUT_DIR="$PWD/qt-native-app/dist" \
bash qt-native-app/packaging/appimage/build-appimage.sh
```

## Release Outputs

Artifacts are published in GitHub Releases:

- `rewritto-ide-linux-x86_64-native.tar.gz`
- `Rewritto_Ide-x86_64.AppImage`

Release workflow:

- `arduino-ide/.github/workflows/release.yml`

Detailed instructions:

- `arduino-ide/qt-native-app/README.md`
