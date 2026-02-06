# Qt6 Port Roadmap (Replacing Electron Shell)

## What “port to Qt6” means here

Rewritto-ide 2.x is a Theia (web) application packaged as a desktop app via **Electron**. A true “native Qt Widgets/QML rewrite” would effectively be a new IDE (and would lose core capabilities like the existing Theia/Monaco UX and VS Code extension hosting).

This roadmap targets a **Qt6-based desktop shell** that replaces the **Electron main process** while **keeping the existing Theia backend + frontend** intact:

- UI remains the existing Theia/Monaco web UI.
- The desktop container becomes **Qt6 + QtWebEngine** (Chromium) instead of Electron.
- Electron-only APIs used by the frontend are re-provided via a **Qt ↔ JS bridge**.

This is the lowest-risk path to “everything works” on Linux without Electron.

## Scope / assumptions

Initial target: **Linux**.

Kept as-is (initially):
- Theia backend process (Node.js)
- Theia frontend bundle (web app)
- Arduino CLI daemon integration
- VS Code extension hosting

Replaced:
- Electron main process responsibilities:
  - window lifecycle, multi-window, single instance
  - native dialogs
  - native menus + accelerators
  - context menus (native style)
  - “show item in folder / open path”
  - zoom / scale handling
  - app updater (electron-updater replacement)

## Current architecture (today)

- **Electron main** spawns **Theia backend** (Node.js).
- Backend serves the frontend over HTTP on a random local port.
- Electron main opens a BrowserWindow to that local URL.
- Frontend talks to backend via JSON-RPC (websocket).
- Frontend talks to Electron main via Electron IPC for desktop-specific operations.

Key Arduino-specific Electron IPC surface:
- `window.electronArduino` (see `arduino-ide-extension/src/electron-common/electron-arduino.ts`)

Key Theia Electron surface:
- `window.electronTheiaCore` (used for window ops + native menus/context menus)

IDE updates:
- `electron-updater` in `arduino-ide-extension/src/electron-main/ide-updater/ide-updater-impl.ts`

## Target architecture (Qt shell)

```
           +-----------------------------+
           |        Qt Host App          |
           |  (Qt6 Widgets + WebEngine)  |
           |                             |
           |  - spawns backend (node)    |
           |  - manages windows          |
           |  - native menus/dialogs     |
           |  - updater implementation   |
           |  - JS bridge (WebChannel)   |
           +--------------+--------------+
                          |
                          | HTTP/WebSocket to localhost
                          v
           +-----------------------------+
           |        Theia Backend        |
           |       (Node.js process)     |
           |  - serves frontend          |
           |  - JSON-RPC services        |
           |  - talks to arduino-cli     |
           +--------------+--------------+
                          |
                          | loads web app
                          v
           +-----------------------------+
           |        Theia Frontend       |
           |    (Monaco + React etc.)    |
           |  - calls window.electron…   |
           +-----------------------------+
```

## Strategy: staged migration with acceptance gates

### Gate 0 — Define “everything works”
Deliverables:
- A written **feature checklist** and a **manual test matrix**.
- Decide Linux-only vs cross-platform.
- Decide whether QtWebEngine is acceptable (recommended), or whether a native UI rewrite is required (not recommended).

Acceptance:
- Clear definition of “done” (must-have vs nice-to-have).

### Gate 1 — Qt shell MVP (single window)
Deliverables:
- A Qt6 app that:
  - starts the existing backend process
  - loads the frontend in a `QWebEngineView`
  - implements the minimal JS bridge so the IDE boots to a usable UI

Bridge v1 must cover at least:
- `electronArduino.showMessageBox / showOpenDialog / showSaveDialog`
- `electronArduino.openPath`
- `electronTheiaCore.setZoomLevel`
- safe shutdown hooks (`onAboutToClose`, close)

Acceptance:
- IDE renders and basic navigation works.
- Can open an existing sketch folder and view/edit files.

### Gate 2 — Menus + accelerators + context menus
Deliverables:
- Implement `electronArduino.setMenu(menuDto)`:
  - Convert `MenuDto` (contains JS `execute` handlers) into a serializable menu model by assigning stable `nodeId`s.
  - Build native `QMenuBar` and forward action triggers back to the stored JS handlers.
- Implement native context menu popup compatible with Theia expectations (`electronTheiaCore.popup(...)`).

Acceptance:
- Main menu works (File/Edit/Sketch/Tools/Help).
- Key accelerators work.
- Context menus work in editor, explorer, etc.

### Gate 3 — Window management parity
Deliverables:
- Multi-window support (new window, focus existing window for same workspace).
- “single instance” semantics: second launch forwards args to first instance.
- Plotter window support (`CHANNEL_SHOW_PLOTTER_WINDOW` equivalent).
- “recent workspaces” restore (optional, but current Electron code stores window bounds and recent sketches).

Acceptance:
- All window-related flows in IDE2 behave like current Electron build.

### Gate 4 — Updater replacement (Linux)
Deliverables:
- Replace `electron-updater` with a Linux-appropriate updater strategy:
  - “check updates” + show changelog + download link at minimum
  - optional: AppImage update integration if distributing AppImage
- Rebind `IDEUpdater` frontend protocol to a non-Electron transport:
  - recommended: move updater service to Theia backend (websocket JSON-RPC)
  - alternative: implement a Qt-side RPC transport compatible with `ElectronIpcConnectionProvider`

Acceptance:
- “Check for updates” behaves predictably and doesn’t error.

### Gate 5 — Packaging + desktop integration
Deliverables:
- CMake build + packaging artifacts for Linux:
  - AppImage or `.tar.xz` + desktop file + icons
  - file association for `.ino`
  - proper application icon (fix currently handled by Electron-specific AppImage workaround)

Acceptance:
- End-user install/run experience matches Rewritto-ide expectations.

## Major risks / hard parts

- **Electron IPC surface area**: beyond `electronArduino`, Theia Electron integration expects additional APIs (`electronTheiaCore` + menu/context menu plumbing). This must be emulated accurately.
- **Updater**: `electron-updater` is Electron-specific; Qt requires a new implementation and likely a transport change away from `ElectronIpcConnectionProvider`.
- **Security token / localhost server**: Electron has an established flow for starting backend + securing the frontend/backend connection. The Qt shell must preserve equivalent safety.
- **Parity testing**: without a tight checklist, “everything works” becomes unbounded.

## Recommended next decisions (needed before deep implementation)

1. Confirm **QtWebEngine shell** is acceptable (vs native UI rewrite).
2. Confirm **Linux-only** initial target.
3. Confirm updater requirements for Linux:
   - disable updater and link to downloads page (fastest)
   - implement full updater (more work)

