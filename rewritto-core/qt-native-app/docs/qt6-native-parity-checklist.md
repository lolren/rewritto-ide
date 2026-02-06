# Qt6 Native Rewrite — Feature Parity Checklist

Use this checklist to drive “everything works” for the native Qt rewrite. Treat each item as pass/fail with a short test note and a reference issue.

## 1) Core UX

- [ ] App starts reliably (cold start)
- [ ] App exits cleanly (no zombie processes)
- [ ] Single instance behavior (2nd launch focuses 1st / forwards args)
- [ ] Crash reporting / logs location defined
- [ ] Restore last session (recent sketches, window layout)

## 2) Sketch / Files

- [ ] Create new sketch
- [ ] Open sketch folder
- [ ] Open recent sketch
- [ ] Save file / Save As
- [ ] Autosave (if desired)
- [ ] Sketchbook management (folders, rename, delete)
- [ ] Examples browser

## 3) Editor

- [ ] Syntax highlighting (C/C++ + Arduino specifics)
- [ ] Line numbers gutter
- [ ] Tabs / multiple open files
- [ ] Undo/redo
- [ ] Find / replace
- [ ] Multi-file search
- [ ] Go to line
- [ ] Auto-indent on newline
- [ ] Indentation / formatting rules
- [ ] Keybindings (basic set)
- [ ] Line endings handling
- [ ] Bracket matching + brace-scope hue highlighting (future enhancement)

## 4) Language intelligence (LSP)

- [ ] Start `arduino-language-server`
- [ ] Diagnostics (errors/warnings)
- [ ] Autocomplete
- [ ] Hover
- [ ] Go to definition
- [ ] Find references
- [ ] Rename symbol (optional)
- [ ] Format document (optional)

## 5) Boards / Ports

- [ ] Detect connected boards
- [ ] Select board (FQBN)
- [ ] Select port
- [ ] Persist selection per sketch (or global, decide)
- [ ] Board details view (optional)

## 6) Build / Upload (arduino-cli)

- [ ] Verify (compile) shows progress and output
- [ ] Upload works for at least: AVR + ARM baseline boards
- [ ] Errors are clickable (jump to location)
- [ ] Advanced options (verbose compile/upload)
- [ ] Sketch preprocess behavior matches IDE expectations

## 7) Boards Manager

- [ ] Update indexes
- [ ] Search platforms
- [ ] Install platform
- [ ] Remove platform
- [ ] Update platform
- [ ] Offline behavior (cache)

## 8) Library Manager

- [ ] Update library index
- [ ] Search libraries
- [ ] Install library
- [ ] Remove library
- [ ] Update library
- [ ] Handle “multiple libraries found” UX

## 9) Serial Monitor + Plotter

- [ ] Serial monitor connect/disconnect
- [ ] Baud rate selection
- [ ] Line ending selection
- [ ] Timestamping (optional)
- [ ] Plotter rendering + controls

## 10) Preferences

- [ ] Preferences UI
- [ ] Persist preferences
- [ ] Theme selection (light/dark)
- [ ] Interface scaling
- [ ] Proxy settings (if needed)
- [ ] Config migration from IDE2 (optional)

## 11) Tools / Integrations

- [ ] Open in file manager (“show item in folder”)
- [ ] External editor integrations (optional)
- [ ] Programmer selection (if needed)

## 12) Debugger (if in scope)

- [ ] Start debug session
- [ ] Breakpoints
- [ ] Step in/over/out
- [ ] Variables/watch
- [ ] Call stack
- [ ] Terminate session cleanly

## 13) i18n / Accessibility

- [ ] Translations load (baseline languages)
- [ ] High DPI rendering
- [ ] Screen reader basics (optional)

## 14) Packaging / AppImage

- [ ] AppImage builds reproducibly
- [ ] Runs on baseline distro without extra deps
- [ ] Desktop file + icon + categories
- [ ] File association for `.ino`
- [ ] Update strategy defined (in-app check vs external)
