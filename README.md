# Rewritto-ide

A native Qt6 Arduino-compatible IDE for Linux. Rewritto-ide is a lightweight, Electron-free environment built with native Qt6 widgets for performance and system integration.

![Rewritto-ide](https://img.shields.io/badge/version-1.0.0-blue)
![License](https://img.shields.io/badge/license-AGPL--3.0--or--later-green)

## Features

### Core Functionality
- **Sketch Management**: Create, open, save, rename sketches with multi-file support
- **Code Editor**: Syntax highlighting (C/C++), code folding, bracket matching, multi-cursor editing
- **Build & Upload**: Compile/Verify via arduino-cli, upload to boards, export compiled binary
- **Boards Manager**: Install/remove platforms and manage board configurations
- **Library Manager**: Search, install, and manage Arduino libraries
- **Serial Monitor**: Monitor serial port with configurable baud rate and line endings
- **Serial Plotter**: Graph serial data in real-time with multiple series support

### Advanced Features
- **LSP Integration**: Code completion, hover information, go-to-definition with clangd
- **Debugger Support**: GDB/MI2 integration with breakpoints, watches, and call stack
- **Quick Open**: Fuzzy file search across sketch and sketchbook
- **Find in Files**: Search across all files with context previews
- **Session Restore**: Automatically reopen last sketch with cursor positions
- **Recent Sketches**: Pin/unpin sketches with persistent history
- **Themes**: Dark/Light theme support with system integration
- **Interface Scaling**: Configurable UI scaling for HiDPI displays

## Requirements

### Build Dependencies
- CMake >= 3.21
- GCC/G++ >= 11.0 (C++20 support)
- Qt6 >= 6.2.0 (qt6-base-dev, qt6-tools-dev)
- pkg-config

### Runtime Dependencies
- Qt6 Runtime (Core, Widgets, Network)
- arduino-cli (auto-downloaded in AppImage)
- arduino-language-server (auto-downloaded in AppImage)
- clangd-18 (auto-downloaded in AppImage)

## Installation

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y cmake build-essential qt6-base-dev qt6-tools-dev pkg-config
```

### Arch Linux
```bash
sudo pacman -S qt6-base qt6-tools cmake gcc make
```

### Fedora
```bash
sudo dnf install qt6-qtbase qt6-qttools cmake gcc-c++ make
```

## Building from Source

### Native Build
```bash
# Clone the repository
git clone https://github.com/lolren/rewritto-ide.git
cd rewritto-ide/rewritto-core/qt-native-app

# Configure with CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Run
./build/rewritto-ide
```

### AppImage Build
```bash
cd rewritto-core/qt-native-app

# Build native executable first
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Build AppImage (downloads and bundles arduino-cli, language server, clangd)
chmod +x packaging/appimage/build-appimage.sh
./packaging/appimage/build-appimage.sh
```

The AppImage will be created in `dist/Rewritto-ide-x86_64.AppImage`.

### Release Artifacts

Two Linux artifacts are published per release:

- Native tarball: `rewritto-ide-linux-x86_64-native.tar.gz`
- AppImage: `Rewritto-ide-x86_64.AppImage`

Local release output folder:

- `rewritto-core/qt-native-app/dist`

GitHub release workflow:

- `rewritto-core/.github/workflows/release.yml`

## Usage

### First Launch
1. Select your sketchbook location (default: `~/Rewritto-ide`)
2. Install board packages via **Tools > Board > Boards Manager**
3. Select your board from **Tools > Board**
4. Select your port from **Tools > Port**
5. Open a sketch or create a new one

### Keyboard Shortcuts
- **Ctrl+N**: New Sketch
- **Ctrl+O**: Open Sketch
- **Ctrl+S**: Save
- **Ctrl+Shift+S**: Save As...
- **Ctrl+P**: Upload
- **Ctrl+R**: Verify/Compile
- **Ctrl+Shift+F**: Find in Files
- **Ctrl+P**: Quick Open

## Documentation

- [BUILDING.md](rewritto-core/qt-native-app/BUILDING.md) - Detailed build instructions
- [FEATURE_PARITY_TODO.md](rewritto-core/qt-native-app/FEATURE_PARITY_TODO.md) - Feature parity roadmap
- [docs/](rewritto-core/qt-native-app/docs/) - Additional documentation

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](rewritto-core/qt-native-app/docs/CONTRIBUTING.md) for guidelines.

## License

This project is licensed under AGPL-3.0-or-later. See [LICENSE.txt](rewritto-core/qt-native-app/LICENSE.txt) for details.

## Acknowledgments

- Inspired by established Arduino-compatible desktop IDE workflows
- Uses [Arduino CLI](https://github.com/arduino/arduino-cli) for build operations
- Uses [arduino-language-server](https://github.com/arduino/arduino-language-server) for code intelligence
- Built with [Qt6](https://www.qt.io/)

## Support

- **Issues**: [GitHub Issues](https://github.com/lolren/rewritto-ide/issues)
- **Discussions**: [GitHub Discussions](https://github.com/lolren/rewritto-ide/discussions)

---

**Rewritto-ide** - A native Qt6 Arduino-compatible IDE for Linux
