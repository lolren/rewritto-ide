# Qt6 Native UI Rewrite Roadmap (No Web UI)

This document is for a **full native Qt6 UI rewrite** of Rewritto-ide 2.x on **Linux**, producing an **AppImage**.

It is intentionally staged: the only realistic way to reach “everything works” is to define *exact* parity targets and deliver them gate-by-gate with continuous manual + automated regression checks.

## Executive summary / reality check

Rewritto-ide 2.x today is **Theia + Monaco + VS Code extension hosting**, packaged with Electron. A “full Qt UI rewrite” means replacing **all** of the frontend (and usually a large chunk of UI-adjacent services). That is a multi-month (often multi-year) effort depending on parity expectations, especially for:

- editor features (Monaco-level UX),
- Language Server Protocol (LSP) integration,
- debugger integration (cortex-debug parity),
- VS Code extension ecosystem.

To reduce risk, this roadmap assumes we **reuse as much of the non-UI logic as possible** (arduino-cli, language server, existing data formats) and we treat UI parity as a checklist.

## Key decisions (need answers early)

1. **Backend strategy**
   - **A (recommended):** keep existing Node/Theia backend pieces as a *headless service* and consume them from Qt via JSON-RPC.
     - Pros: reuses boards/library logic, daemon plumbing, download/index update logic.
     - Cons: still ships Node.js; some APIs are Theia-shaped and may need refactoring into “headless” modules.
   - **B:** rewrite backend in C++/Rust (new “IDE services”).
     - Pros: pure native stack.
     - Cons: essentially a new IDE; highest cost/risk.

2. **Editor component**
   - QScintilla/KTextEditor/CodeMirror-in-WebView are typical choices.
   - For “full Qt UI”, avoid WebView editor; pick a native editor widget and build LSP integration.

3. **Debugging scope**
   - Full IDE2 parity includes “Debugger” (cortex-debug style) on supported targets.
   - Decide: required from day 1, or later gate.

4. **Cloud features / login**
   - Arduino Cloud sketchbook integration, auth flows, etc. are non-trivial.
   - Decide: in scope or out of scope.

5. **Distribution**
   - AppImage target is accepted; decide whether to also produce `.tar.xz` or distro packages.

## Definition of done (“everything works”)

You must agree on a concrete list; otherwise the project can’t converge. Suggested categories:

### Must-have (Gate-complete)
- Create/open/save sketch (incl. examples)
- Board/port selection
- Compile/Upload (via arduino-cli)
- Serial Monitor
- Library Manager + Boards Manager (search/install/remove/update)
- Preferences
- Sketchbook management
- Find/replace, multi-file search
- Syntax highlighting + autocomplete + diagnostics (LSP)
- Basic debugging (if in scope)
- AppImage packaging + desktop integration (icons, .desktop, file association)

### Nice-to-have
- Themes close to IDE2
- Full extension system parity
- Remote sketchbook / Cloud
- Advanced refactorings

## Staged plan with acceptance gates

### Gate 0 — Requirements + parity test matrix
Deliverables:
- A feature checklist (UI + workflows) with pass/fail criteria.
- A manual test plan covering Linux desktop environments (GNOME/KDE, X11/Wayland).
- A data compatibility plan (settings paths, sketchbook location, caches).

Acceptance:
- “Done” is measurable.

### Gate 1 — Native Qt shell + project model (no build/upload yet)
Deliverables:
- Qt app skeleton:
  - main window, dockable panels (sketchbook tree, output, serial)
  - central editor placeholder (even a plain text editor initially)
  - settings storage (QSettings) matching IDE2 config locations where possible
- File operations: open sketch folder, list files, open in editor, save.

Acceptance:
- Day-to-day editing (without compile/upload) works reliably.

### Gate 2 — Integrate arduino-cli (compile/upload + board/port)
Deliverables:
- A process manager for `arduino-cli`:
  - run compile/upload commands
  - parse output into an “Output” panel
  - surface errors/warnings and jump-to-location (even if basic)
- Board discovery + port listing
- Cached indexes management (download/update)

Acceptance:
- Compile and upload works for at least one AVR + one ARM board.

### Gate 3 — Library/Boards manager UI
Deliverables:
- Boards manager UI with install/remove/update flows
- Library manager UI with install/remove/update flows
- Progress reporting + cancelation

Acceptance:
- Same core flows as IDE2.

### Gate 4 — LSP + editor features
Deliverables:
- LSP client (JSON-RPC over stdio) for `arduino-language-server`
- Editor integrations:
  - diagnostics (squiggles/list)
  - completion
  - go to definition
  - hover
  - formatting (if supported)

Acceptance:
- Comparable “it helps me code” experience.

### Gate 5 — Serial monitor + plotter parity
Deliverables:
- Serial monitor using `QSerialPort`
- Plotter:
  - data parsing
  - graphing (Qt Charts or other)

Acceptance:
- Equivalent functionality to IDE2 plotter flows.

### Gate 6 — Debugger parity (if required)
Deliverables:
- Debug adapter integration or direct GDB/OpenOCD integration
- Breakpoints, stepping, variables, call stack

Acceptance:
- Parity for a defined set of boards and toolchains.

### Gate 7 — Packaging + AppImage
Deliverables:
- `AppDir` layout + `.desktop` + icons + file associations
- Bundle Qt dependencies via `linuxdeploy`/`linuxdeployqt`
- Bundle runtime dependencies (arduino-cli, language server, etc. per decisions)
- CI build script producing AppImage artifact

Acceptance:
- AppImage runs on a target baseline distro (e.g., Ubuntu 20.04) without extra installs.

## AppImage notes

Recommended tooling:
- `linuxdeploy` + `linuxdeploy-plugin-qt` (or `linuxdeployqt`)
- `appimagetool`

You will need to decide a **baseline distro** for compatibility and build the AppImage in an environment that matches it (or use a container).

## Next step: choose architecture A vs B

Before coding too far: confirm whether shipping a Node-based headless service is acceptable. If not, the backend rewrite becomes the critical path and the roadmap must change accordingly.

