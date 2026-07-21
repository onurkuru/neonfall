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
| Crouch / take cover | Down / S | D-pad down |
| Dash | Shift / C | Circle |
| Attack | X / J | Square |
| Fire | L / V | R |
| Parry | Ctrl / K | L |
| Quit | Esc | Start + Select |
| Fullscreen | F | - |

## Development helpers

| Variable | Effect |
|---|---|
| `NEONFALL_SCALE=2` | Desktop window scale (1-4) |
| `NEONFALL_FULLSCREEN=1` | Start fullscreen (F toggles it at any time) |
| `NEONFALL_STATS=1` | Report draw calls every 60 frames |
| `NEONFALL_SHOT=path.bmp` | Capture a frame and exit |
| `NEONFALL_SHOT_FRAME=n` | Which frame to capture (default 120) |
| `NEONFALL_ASSETS=dir` | Asset root (default `assets`) |
| `NEONFALL_LAYER=n` | Draw only parallax layer n (debugging) |

## Art

All art is CC0 (public domain), so it can live in this repository and ship
inside the VPK without restriction:

- **Warped City** and **Warped City 2** by [ansimuz](https://ansimuz.itch.io/) —
  city layers, walkway tiles, props, enemies, vehicles.
- **Particle Pack** by [Kenney](https://kenney.nl/assets/particle-pack) —
  glows, lens flares, rain, splash rings, steam, sparks and muzzle flashes.

The player character, Kaya, was generated with [PixelLab](https://www.pixellab.ai/)
and is owned outright. Only the east-facing view exists; the engine flips it.

The mood does not come from the sprites: `src/fx.c` drives the neon sources,
their pools on the wet walkway, two depth bands of rain, splash rings and
drifting steam. The walkway reflects the scene by way of a single mirror flag
in the batcher, so anything drawn reflects for free.

## Generated art pipeline

Painted art is generated at high resolution onto a green chroma field with a
white outline, and lands in `assets/raw/`. `tools/ingest.py` keys the green
out, eats the outline back off, trims each layer to its painted band, slices
the cut-out sheets into individual parts, and writes `assets/parts.json`:

```sh
python3 tools/ingest.py all      # or: layers | parts | vfx
```

Third-party packs downloaded as `.unitypackage` can be unpacked with:

```sh
python3 tools/unitypackage.py pack.unitypackage assets/vendor/pack --flat
```

`assets/vendor/` is git-ignored: packs whose licence does not allow
redistribution must stay on the machine that downloaded them.

This path is kept for generated painterly art; the shipping set is pixel art
and uses plain frame animation.

`src/third_party/stb_image.h` is public domain (Sean Barrett).

## Generating sprites with PixelLab

`.mcp.json` wires up PixelLab's MCP server, which generates pixel-art sprites
and animations. It reads the token from the environment, so nothing secret
lives in this repository:

```sh
export PIXELLAB_SECRET=your_token   # from pixellab.ai, free tier is enough
```

Restart Claude Code afterwards so it picks the server up.
