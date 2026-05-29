# Roadmap

## 0.1 - Native Shell

- Minimal Win32 app.
- Application lifecycle.
- Basic dark UI surface.
- CMake and Visual Studio build.

## 0.2 - Window Selection

- Enumerate visible top-level windows.
- Show process name, title, and thumbnail placeholder.
- Store selected target window.

## 0.3 - Capture Prototype

- Integrate Windows Graphics Capture.
- Capture frames from the selected window.
- Display capture in the FPSgrade presentation window.

## 0.4 - Direct3D Renderer

- Add D3D11 device and swap chain.
- Render captured texture to the window.
- Implement nearest and bilinear scaling.

## 0.5 - Game Presets

- Competitive low-latency preset.
- Balanced clarity preset.
- Pixel art preset.
- Old game compatibility preset.

## 0.6 - Metrics

- Frame time overlay.
- Capture latency estimate.
- Dropped-frame counter.
