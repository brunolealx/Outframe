# Outframe

Outframe is a clean-room Windows upscaling and presentation engine for games.
It is designed to be independent code under the Apache-2.0 license.

The project is intentionally separate from the Magpie-derived GPL repository.
Do not copy code, shaders, assets, project files, or detailed implementation
structure from Magpie into this repository.

## Goals

- Capture a selected game/window using official Windows APIs.
- Render to a borderless presentation window.
- Provide low-latency scaling presets for games.
- Add original shaders and effects with clear licenses.
- Keep the codebase small, understandable, and commercially usable.

## Current Status

This is the first clean-room baseline. It contains:

- Apache-2.0 license.
- CMake project.
- Minimal Win32 desktop app.
- Initial visible-window enumeration.
- Click or double-click window selection.
- Prototype scaled preview window using Windows Graphics Capture and Direct3D 11.
- Initial engineering notes and roadmap.

## Prototype Controls

- `R`: refresh visible windows.
- Click a row: select a target window.
- `Enter`: open a GPU-rendered preview of the selected window.
- Double-click a row: select and preview.
- `Esc` in the preview window: close preview.

## Build

Requirements:

- Windows 10 or Windows 11.
- Visual Studio 2022 with Desktop development with C++.
- CMake 3.24 or newer.

From a Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Run:

```powershell
.\build\Debug\Outframe.exe
```

## License

Apache License 2.0. See `LICENSE`.
