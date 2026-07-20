# Neonfall

A side-scrolling 2.5D cyberpunk action platformer for the PlayStation Vita,
with a desktop build for development. Real perspective depth (vitaGL / OpenGL),
pixel-art layers placed at true Z distances, so parallax falls out of the camera
rather than being faked per layer.

Zone 1 "Undercity" vertical slice is in progress: movement, camera, layered
city, animated player.

## Build

Desktop (macOS/Linux, needs SDL2):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/neonfall
```

PS Vita (needs VitaSDK with vitaGL):

```sh
export VITASDK=$HOME/vitasdk
cmake -S . -B build-vita -DBUILD_VITA=ON
cmake --build build-vita --target neonfall.vpk-vpk
```

## Controls

| Action | Keyboard | Vita |
|---|---|---|
| Move | Arrows / A D | Left stick, D-pad |
| Jump (double) | Space / Z | Cross |
| Dash | Shift / C | Circle |
| Attack | X / J | Square |
| Fire | L / V | R |
| Parry | Ctrl / K | L |
| Quit | Esc | Start + Select |

## Development helpers

| Variable | Effect |
|---|---|
| `NEONFALL_SCALE=2` | Desktop window scale (1-4) |
| `NEONFALL_SHOT=path.bmp` | Capture a frame and exit |
| `NEONFALL_SHOT_FRAME=n` | Which frame to capture (default 120) |
| `NEONFALL_ASSETS=dir` | Asset root (default `assets`) |

## Art credits

Pixel art from **Warped City** and **Warped City 2** by
[ansimuz](https://ansimuz.itch.io/), released under CC0 (public domain).
Recoloured and relit in engine.

`src/third_party/stb_image.h` is public domain (Sean Barrett).
