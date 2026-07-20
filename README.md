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

## Art pipeline

Painted art is generated at high resolution onto a green chroma field with a
white outline, and lands in `assets/raw/`. `tools/ingest.py` keys the green
out, eats the outline back off, trims each layer to its painted band, slices
the cut-out sheets into individual parts, and writes `assets/parts.json`:

```sh
python3 tools/ingest.py all      # or: layers | parts | vfx
```

The character is not animated frame by frame. Painted body parts are driven by
a bone hierarchy in `src/skel.c`, and the poses (idle, run, air, attack, dash)
are generated procedurally, so the run cycle stays in step with actual speed.

`src/third_party/stb_image.h` is public domain (Sean Barrett).
