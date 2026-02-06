# Rewritto Ide parity — TODO (Qt Native)

Goal: reach **feature parity with Arduino IDE 2.x** for the core workflows on Linux (Ubuntu 24.04+), shipped as an **AppImage**.

Legend:
- **P0** = must-have to be a practical replacement
- **P1** = expected IDE2 behaviors / quality parity
- **P2** = nice-to-have / longer-term

## Already implemented (high-signal)
- [x] AppImage packaging + headless smoke test
- [x] Compile/Upload/Export (via `arduino-cli`)
- [x] Boards Manager + Library Manager
- [x] Serial Monitor + Serial Plotter
- [x] LSP basics: completion/hover/definition/references/rename/formatting + “Go to Symbol…”
- [x] Debugger scaffolding (MI2) + breakpoints (enable/disable/condition/hit count) + watches persistence
- [x] External file change detection (auto-reload + prompt)
- [x] Right-side toolbar (Sketchbook / Boards / Libraries / Debug / Search)
- [x] Board-specific **Tools** menu options (from `arduino-cli board details`)

## P0 — Core parity (must work end-to-end)

### Sketch & project model
- [x] **Sketch structure parity**: open multi-file sketch tabs from the sketch root (`.ino`, headers, sources) with “primary `.ino`” behavior.
- [x] **Session restore**: reopen last sketch + previously open tabs + cursor/scroll positions.
- [x] **Recent sketches parity**: pin/clear list, remove missing entries, and open recent from welcome/menus.
  - [x] File → Open Recent menu (open/clear) with missing-entry filtering.
  - [x] Pin/unpin sketches (persistent) + pinned section in Open Recent.
  - [x] Welcome screen pinned/recent list.
- [x] **“Save As…” sketch clone**: copy folder + rename primary `.ino` + keep additional files (include updates TBD).
- [x] **Sketch renaming**: rename sketch folder and primary `.ino` with validity checks.
- [x] **Library include injection parity**: insert `#include` at a reasonable location and avoid duplicates.

### Board/port selection & correctness
- [x] **Board/port persistence rules**: persist globally + per-sketch (migration nuances may remain).
- [x] **Port discovery robustness**: hotplug, disappearing ports while monitor/debug running, and clear UX for stale selections.
  - [x] Status message when the selected port disconnects.
  - [x] Visually mark stale selections in the port picker (e.g. missing port styling).
  - [x] Live port updates via `arduino-cli board list --watch` (fallback to polling).
  - [x] Upload warns before proceeding when the selected port is not currently detected.
  - [x] Debug/session edge cases when a port disappears mid-debug (stop + actionable error).
- [x] **Network ports** (where supported): show/select/upload with `arduino-cli` network discovery.
- [x] **Programmer selection parity**: per-board programmer lists, persistence, and applying it consistently to programmer-based flows.
- [x] **Board options correctness**: ensure selected `config_options` are reflected in the FQBN everywhere (build/upload/debug).

### Build / Upload / Diagnostics
- [x] **Unified build pipeline**: avoid using `compile --upload` as a proxy for `upload` where IDE2 expects separate flows; support both accurately per platform/board.
- [x] **Progress + cancellation parity**: cancellable compile/upload with accurate busy state across all QProcesses.
  - [x] Cancel compile/upload cleanly (no core-install prompt on cancel; don’t start upload after cancelling compile).
  - [x] Status bar busy indicator reflects any background job.
  - [x] Global Stop cancels boards/libraries manager + debug + background refresh.
- [x] **Problems panel parity**: richer parsing (linker errors, multi-line GCC diagnostics, library conflict notes), clickable quick fixes where possible.
  - [x] Multi-line GCC diagnostics (snippet/caret capture).
  - [x] Library conflict notes ("Multiple libraries were found…").
  - [x] GCC "fatal error" diagnostics normalized to error severity.
  - [x] Arduino CLI summary errors ("exit status …", "Compilation error: …").
  - [x] Broader linker/tool error coverage + better attribution.
    - [x] Attribute common `undefined reference to …` to the emitting object file (when present).
    - [x] Parse `ld: cannot find …`.
    - [x] Parse common upload tool errors (e.g. `avrdude:`) + upload summary lines.
  - [x] Clickable quick fixes (where possible).
    - [x] Missing header (`…: No such file or directory`) → “Search Library Manager…”
    - [x] Missing platform/core (`platform not installed: vendor:arch`) → “Search Boards Manager…”
    - [x] Missing tool (`exec: "tool": executable file not found…`, `tool: command not found`) → “Search Boards Manager…”
- [ ] **Build profiles**: Release/Debug build configurations + persistent “sketch build settings” UI (where IDE2 exposes them).
  - [x] “Optimize for Debugging” toggle (per-sketch) wired to `arduino-cli compile --optimize-for-debug`.
- [x] **Core/toolchain install prompts**: when compiling for an uninstalled FQBN, offer install/open Boards Manager.
  - [x] Missing toolchain/tool prompts (beyond missing platform): detect “tool not found” failures and offer Boards Manager.
- [ ] **Pluggable discovery/tools support**: honor tools required by modern cores (esp32, rp2040, etc.) and surface missing tool errors cleanly.

### Boards Manager parity
- [x] **Better filtering**: installed / updatable, vendor/category filters, search-as-you-type, and version pinning UX.
  - [x] Installed/Updatable/Not installed filters (client-side).
  - [x] Vendor + Architecture filters (from platform id).
  - [x] Type/Category filters (from platform release types).
  - [x] Search-as-you-type (debounced).
  - [x] Version pinning UX (pin installed version to prevent upgrades).
- [x] **Index update UX**: automatic update + visible status + retry on failures.
  - [x] Auto update when opening (24h cadence, 10m retry cooldown).
  - [x] Visible “Index: …” last-updated status + manual Update Index button.
- [x] **Offline/airgapped behavior**: clear messaging and fallback when indexes cannot be fetched.

### Library Manager parity
- [x] **Dependencies UX**: show dependency tree, install dependencies, and warn on conflicts.
  - [x] Show direct dependencies (incl. version constraints) in details for selected version.
  - [x] “Install deps” toggle (uses `arduino-cli lib install --no-deps` when unchecked).
  - [x] Dependencies status dialog (resolved versions + installed/missing/mismatch).
- [x] **Version pinning**: install a specific version and prevent auto-upgrade when pinned.
  - [x] Pin installed version to prevent upgrades (and exclude from Upgrade All).
- [x] **“Include library” workflow parity**: surface examples + include actions in a single flow like IDE2.
  - [x] Search-as-you-type (debounced).

### Serial Monitor / Plotter parity
- [x] **Disconnect/error recovery**: handle unplug while connected; auto-reconnect option.
- [x] **Line ending + timestamp behavior**: ensure output formatting matches IDE2 for mixed CR/LF streams.
  - [x] Treat `\r` as carriage return (overwrite) rather than normalizing it to `\n`.
- [x] **Send history**: up/down history + persistence (macros/snippets still TBD).
- [ ] **Plotter UX**: series naming, color assignment, pause/resume, autoscale controls, and parsing compatibility with common core outputs.
  - [x] Series naming from `label=value` / `label:value` streams + CSV headers.
  - [x] Legend with per-series visibility toggles.
  - [x] Pause/Resume.
  - [x] Autoscale controls (manual range / freeze).

## P1 — Editor & UX parity (expected daily-use behavior)

### Editor (Monaco-like) features
- [x] **Code folding** (based on braces + optional LSP folding ranges).
- [x] **Multiple cursors / multi-selection**.
- [x] **Bracket matching + scope highlight** (and the requested “brace hue” enhancement can live here later).
- [x] **Indent guides + whitespace rendering options**.
- [x] **Snippets** (basic) + tab-stop navigation (where LSP provides snippets).
- [x] **Line numbers / current line highlight** (minimap optional).
- [ ] **Large file performance**: avoid UI freezes on big logs or large source files.
  - [x] Output panel caps at 20k lines (prevents unbounded growth).
  - [x] Debounced folding recomputation for larger documents.
  - [x] Large source file mode (disable highlighting/folding over ~512KB).

### Navigation & search
- [x] **Quick Open parity**: fuzzy match across sketch + open tabs + sketchbook; show file icons and paths.
- [x] **Find in Files parity**: include/exclude globs, ignore build folders, and show context previews.
- [x] **Refactor actions**: expose LSP code actions and organize imports where supported.

### Windowing & docks
- [ ] **Dock layout parity**: default layout matches IDE2; save/restore docking state reliably.
  - [x] Default layout keeps left-side tabs (Sketchbook/Search/Managers/Outline) and hides Outline by default.
  - [x] “Reset Layout” action restores the captured default dock/toolbar state.
  - [x] Restore-state fallback to default when saved state is invalid.
- [x] **Status bar parity**: richer compile/upload status + selected board/port + background tasks indicator.
  - [x] Board/Port indicator + cursor position.
  - [x] Busy spinner + task label (compile/upload/index refresh/etc).
  - [x] Post-build size/usage summary in status bar (when available).
- [x] **Notifications**: non-blocking toasts for installs, errors, and background operations.
  - [x] Basic toast widget (bottom-right).
  - [x] Success toasts for common CLI jobs (compile/upload/index update/debug check/install).
  - [x] Error toasts for failed jobs (quick action: Show Output).
  - [x] Contextual toast actions (e.g. Open Boards/Library Manager for missing-core/tool cases).
  - [x] Toasts for long-running background tasks (index update, port watch reconnect, installs).

### Preferences parity
- [x] Theme selection (system/light/dark) + interface scale.
- [x] Sketchbook folder selection (default `~/Rewritto`, fallback to legacy `~/Arduino`).
- [x] Additional Boards Manager URLs.
- [x] Editor settings: font, tab size, insert spaces, indent guides, whitespace, word wrap, zoom.
- [x] Compiler warnings level + verbose compile/upload toggles.
- [x] **Language/locale selection** + restart prompt.
- [x] **Keymap preferences** (or at least configurable shortcuts) + export/import.
- [ ] **Proxy/network settings** for Boards/Library downloads.
- [ ] **Editor behavior prefs**: autosave, trim trailing whitespace, end-of-line mode, default line ending.
  - [x] Default line ending preference (LF/CRLF) for new/empty files.
  - [x] Trim trailing whitespace on save.
  - [x] Autosave.
  - [x] End-of-line mode (per-file override / preserve vs convert).

## P1 — Debugger parity (for supported boards)
- [ ] **Variables tree**: expandable structs/arrays + pretty printing where possible.
- [x] **Watches editing**: edit existing watch expressions (evaluate-on-enter still TBD).
- [ ] **Hover evaluate** while stopped (and selection evaluate).
- [ ] **Breakpoint IDs + sync**: track GDB breakpoint IDs to avoid full re-sync and support “remove” precisely.
- [ ] **Conditional/logpoints**: emulate Arduino IDE logpoints (print message) where feasible with MI commands.
- [ ] **Attach/Detach flows** (if IDE2 supports it for a core) and clearer error reporting from `arduino-cli debug`.

## P2 — Distribution & system integration
- [ ] **Branding/trademark pass**: remove/replace remaining “Arduino” UI strings and metadata where appropriate, while keeping toolchain identifiers that must remain (`arduino-cli`, `arduino-language-server`) and attribution where required.
- [ ] **.desktop / MIME integration**: open `.ino` and sketch folders from file manager.
- [ ] **Icons & theming polish**: consistent icon set, dark-mode correctness, HiDPI assets.
- [ ] **AppStream metadata** polish to satisfy `appstreamcli validate`.
- [ ] **Update strategy**: release channel + update notification (even if AppImage-based).

## P2 — Backend modernization (optional / long-term)
- [ ] Define a stable internal “backend API” (compile/upload/install/debug) so UI is decoupled from transport (`arduino-cli` vs native C++).
- [ ] Prototype a native C++ backend that can replace `arduino-cli` gradually (starting with board/port discovery).

## Test plan (needed for parity confidence)
- [ ] Expand unit tests around: FQBN option composition/persistence, board/port selection, compile/upload argument generation, and parsing diagnostics.
- [ ] Add integration smoke tests using a small fixture sketch (compile-only in CI).
- [ ] Add headless UI smoke tests for docking/layout restore, tab restore, and file reload prompts.
