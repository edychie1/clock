# AGENTS.md — AIClock C++ Desktop Widget

## Project Overview

AIClock is a Windows desktop clock widget written in C++20 using the Win32 API and GDI+. It displays a customizable time/date overlay with system tray integration, drag-to-reposition, and a settings dialog. The UI text is in Traditional Chinese (zh-TW). Licensed under CC BY-NC-ND 4.0.

## Build Commands

The project uses **Zig** as the C++ compiler (not MSVC or MinGW directly). Zig must be on PATH.

```powershell
# PowerShell build (preferred — includes security hardening linker flags)
.\build.ps1

# Batch build (simpler, fewer linker flags)
.\build.bat
```

The PowerShell build script uses `Join-Path` to handle the non-ASCII source path (`E:\祈\時鐘`) safely. The batch script uses `subst Z:` for compatibility. The compiled output goes to `執行檔(C++)\AIClock.exe`. There is no separate test suite or lint step.

### Key compiler/linker flags

- `-std=c++20 -O2`
- `-Xlinker /subsystem:windows` (GUI subsystem, no console)
- PowerShell adds: `--dynamicbase`, `--nxcompat`, `--high-entropy-va`
- Linked libraries: `gdi32 gdiplus shell32 comctl32 comdlg32 advapi32 ole32 uuid`

## Architecture

### File structure

| File | Purpose |
|---|---|
| `main.cpp` | Entire application: WinMain, WndProc, SettingsWndProc, GDI+ rendering, tray icon, context menu, drag logic (~1045 lines) |
| `config.h` / `config.cpp` | Config struct, hand-rolled JSON parser/serializer, registry auto-start, UTF-8/UTF-16 conversion |
| `clock.rc` | Resource script: embeds `clock.ico` (resource ID 1), VERSIONINFO, and `app.manifest` |
| `app.manifest` | Declares DPI awareness, OS compatibility (Win7–11), asInvoker UAC level |
| `build.ps1` / `build.bat` | Build scripts (see above) |
| `執行檔(C++)/setup.ps1` | Admin install script: copies exe to Program Files, adds Defender exclusion, creates desktop shortcut |
| `執行檔(C++)/config.json` | Sample/default config file shipped alongside the exe |

### Runtime flow

1. `WinMain` initializes GDI+, common controls, registers window classes
2. Creates a layered popup window (`WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST`)
3. Loads `config.json` from the exe's directory (falls back to `%APPDATA%\AIClock\config.json` for migration)
4. 1-second timer drives `UpdateClockText()` + `InvalidateRect`
5. `WM_PAINT` double-buffers via memory DC → `DrawClock()` renders with GDI+
6. Window is always-topmost via `WM_WINDOWPOSCHANGING` override
7. Dragging saves position to config on mouse-up
8. Settings window is a separate `WNDCLASS` (`AIClockSettings`), not a dialog resource — all controls are created programmatically in `WM_CREATE`

### Configuration

Config is stored as `config.json` next to the exe. The JSON parser in `config.cpp` is custom (no external library) — it uses string search, not a proper tokenizer. When adding new config fields:

1. Add field to `Config` struct in `config.h` with a default value
2. Add `FindJson*` call in `Config::_ParseJson()`
3. Add serialization in `Config::Save()` using the appropriate `add_*` lambda
4. Wire it into the settings UI in `SettingsWndProc` (control creation in `WM_CREATE`, read-back in `ID_SAVE` handler, slider handling in `WM_HSCROLL`)

### Timezone handling

Timezones are implemented as fixed UTC offsets (no DST). The mapping lives in three parallel arrays: `g_tzList[]` (IANA names), `g_tzLabels[]` (display labels), and the offset table inside `GetTzOffsetHours()`. Adding a timezone requires updating all three.

## Conventions & Gotchas

- **All strings are wide (`std::wstring`, `L"..."`)**. The project is UNICODE-only (`#define UNICODE` / `_UNICODE`). File I/O converts between UTF-8 (on disk) and UTF-16 (in memory) via `WstringToUtf8`/`Utf8ToWstring`.
- **No external dependencies**. Everything is Win32 API + GDI+. Do not introduce third-party libraries without strong justification.
- **Font resource ID is hardcoded as `1`** in both `clock.rc` and `LoadIconW(g_hInst, MAKEINTRESOURCEW(1))`. Changing the icon resource ID requires updating both places.
- **Settings window control IDs** start at 3001 (enum in main.cpp). One anonymous edit control uses magic ID `301` for the font color hex display.
- **Layout math is coupled**: changing `font_size` auto-scales `width` (`fontSize * 6`) and `line_width` (`fontSize * 5`) in the slider handler. The `ComputeLayout()` function calculates vertical positions based on multiple gap config values.
- **Transparent background mode** uses `LWA_COLORKEY` — the `bg_color` becomes the transparency key. This means any pixel matching `bg_color` is fully transparent regardless of alpha.
- **Zone.Identifier removal**: Both `build.ps1` and `WinMain` strip the Zone.Identifier ADS to prevent SmartScreen warnings on freshly-built executables.
- **Version is defined in two places**: `clock.rc` (VERSIONINFO resource) and `version_info.txt` (Python-format, possibly for an external tool). Keep them in sync when bumping versions.
- **`MaybeCrash()`** at line 90 is an empty stub (likely a leftover debug hook). Safe to ignore.
- **No DPI scaling**: The manifest does not declare per-monitor DPI awareness. The app renders at system DPI only.
