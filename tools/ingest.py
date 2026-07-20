#!/usr/bin/env python3
"""Turn generated art in assets/raw into engine-ready PNGs in assets/.

The generator cannot emit alpha, so sprites arrive on a pure green field with a
white outline that keeps anti-aliased edges from blending into the green. Here
we key the green out, eat the outline back off, and kill any green that bled
into the sprite itself.

  ingest.py layers     parallax layers + walkway
  ingest.py parts      cut-out sheets -> one PNG per part + manifest
  ingest.py vfx        additive effects (black field -> luminance alpha)
  ingest.py all
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "raw")
OUT = os.path.join(ROOT, "assets")

# Vita has little graphics memory; layers ship downscaled.
LAYER_WIDTH = 1024
PARTS_MAX = 512


def load(name):
    return np.asarray(Image.open(os.path.join(RAW, name)).convert("RGB")).astype(np.int16)


def green_mask(rgb):
    """True where the pixel belongs to the chroma field."""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    # green dominates both other channels by a wide margin
    return (g > 90) & (g - r > 45) & (g - b > 45)


def white_mask(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mn = np.minimum(np.minimum(r, g), b)
    mx = np.maximum(np.maximum(r, g), b)
    return (mn > 185) & (mx - mn < 42)


def dilate(mask, radius):
    """Grow a boolean mask by `radius` pixels (separable, cheap)."""
    out = mask.copy()
    for _ in range(radius):
        g = out.copy()
        g[1:, :] |= out[:-1, :]
        g[:-1, :] |= out[1:, :]
        g[:, 1:] |= out[:, :-1]
        g[:, :-1] |= out[:, 1:]
        out = g
    return out


def despill(rgb, alpha):
    """Pull green back toward the other channels where it leaked into the art."""
    rgb = rgb.astype(np.float32)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    cap = np.maximum(r, b)
    spill = (g > cap + 8) & (alpha > 0)
    g[spill] = cap[spill] + 8
    return np.clip(rgb, 0, 255).astype(np.uint8)


def key_green(name, max_outline=24):
    """RGBA array with the chroma field and its white outline removed.

    The outline is whatever white the chroma field can reach by flooding
    inward, so its thickness does not have to be known up front.
    """
    rgb = load(name)
    field = green_mask(rgb)
    white = white_mask(rgb)

    drop = field.copy()
    for _ in range(max_outline):
        grown = dilate(drop, 1) & white
        if not (grown & ~drop).any():
            break
        drop |= grown

    # one more pixel so no anti-aliased fringe survives
    drop = dilate(drop, 1)
    alpha = np.where(drop, 0, 255).astype(np.uint8)

    rgb = despill(rgb, alpha)
    return np.dstack([rgb, alpha])


def save(arr, rel):
    path = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(arr, "RGBA").save(path)
    h, w = arr.shape[:2]
    print(f"  + {rel}  {w}x{h}")


def resize_to_width(img, width):
    w, h = img.size
    if w <= width:
        return img
    return img.resize((width, max(1, round(h * width / w))), Image.LANCZOS)


def crop_to_content(img, pad=4):
    """Trim fully transparent rows so a layer's painted band is its whole height."""
    a = np.asarray(img)
    rows = np.where(a[..., 3].max(axis=1) > 8)[0]
    if rows.size == 0:
        return img
    top = max(0, rows[0] - pad)
    bot = min(a.shape[0], rows[-1] + 1 + pad)
    return img.crop((0, top, img.width, bot))


def seam_blend(img, blend=64):
    """Cross-fade the right edge into the left so the layer tiles without a seam."""
    a = np.asarray(img).astype(np.float32)
    h, w = a.shape[:2]
    blend = min(blend, w // 4)
    ramp = np.linspace(0.0, 1.0, blend, dtype=np.float32)[None, :, None]
    left = a[:, :blend].copy()
    right = a[:, -blend:].copy()
    a[:, :blend] = left * ramp + right * (1.0 - ramp)
    a = a[:, : w - blend]
    return Image.fromarray(a.astype(np.uint8), "RGBA")


# ---------------------------------------------------------------- layers

LAYERS = [
    ("layer1-sky.png",      "bg/layer1-sky.png",      False, False),
    ("layer2-skyline.png",  "bg/layer2-skyline.png",  True,  True),
    ("layer3-mid.png",      "bg/layer3-mid.png",      True,  True),
    ("layer4-near.png",     "bg/layer4-near.png",     True,  True),
    ("layer4-near2.png",    "bg/layer4-near2.png",    True,  True),
    ("layer5-fore.png",     "bg/layer5-fore.png",     True,  True),
    ("walkway.png",         "tiles/walkway.png",      True,  False),
]


def do_layers():
    print("layers:")
    for src, dst, keyed, seam in LAYERS:
        if not os.path.exists(os.path.join(RAW, src)):
            print(f"  ! missing {src}")
            continue
        if keyed:
            img = crop_to_content(Image.fromarray(key_green(src), "RGBA"))
        else:
            img = Image.open(os.path.join(RAW, src)).convert("RGBA")
        img = resize_to_width(img, LAYER_WIDTH)
        if seam:
            img = seam_blend(img)
        save(np.asarray(img), dst)


# ---------------------------------------------------------------- parts

def components(alpha, min_area=900):
    """Label connected opaque regions without pulling in scipy."""
    h, w = alpha.shape
    solid = alpha > 32
    labels = np.zeros((h, w), np.int32)
    boxes = []
    cur = 0
    stack = []
    for y0 in range(h):
        row = solid[y0]
        for x0 in range(w):
            if not row[x0] or labels[y0, x0]:
                continue
            cur += 1
            labels[y0, x0] = cur
            stack.append((y0, x0))
            minx = maxx = x0
            miny = maxy = y0
            area = 0
            while stack:
                y, x = stack.pop()
                area += 1
                if x < minx: minx = x
                if x > maxx: maxx = x
                if y < miny: miny = y
                if y > maxy: maxy = y
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w and solid[ny, nx] and not labels[ny, nx]:
                        labels[ny, nx] = cur
                        stack.append((ny, nx))
            boxes.append((cur, area, minx, miny, maxx, maxy))
    return labels, [b for b in boxes if b[1] >= min_area]


PART_SHEETS = [
    ("kaya-parts.png",  "player"),
    ("drone-parts.png", "drone"),
    ("guard-parts.png", "guard"),
    ("props.png",       "props"),
]


def do_parts():
    print("parts:")
    manifest = {}
    for src, group in PART_SHEETS:
        if not os.path.exists(os.path.join(RAW, src)):
            print(f"  ! missing {src}")
            continue
        rgba = key_green(src)
        labels, boxes = components(rgba[..., 3])
        # reading order: top-to-bottom in bands, then left-to-right
        boxes.sort(key=lambda b: (b[3] // 200, b[2]))
        entries = []
        for i, (lab, area, x0, y0, x1, y1) in enumerate(boxes):
            piece = rgba[y0:y1 + 1, x0:x1 + 1].copy()
            keep = labels[y0:y1 + 1, x0:x1 + 1] == lab
            piece[..., 3] = np.where(keep, piece[..., 3], 0)
            img = Image.fromarray(piece, "RGBA")
            if max(img.size) > PARTS_MAX:
                sc = PARTS_MAX / max(img.size)
                img = img.resize((max(1, round(img.width * sc)),
                                  max(1, round(img.height * sc))), Image.LANCZOS)
            rel = f"{group}/{group}-{i:02d}.png"
            save(np.asarray(img), rel)
            entries.append({"file": rel, "index": i,
                            "src_box": [x0, y0, x1, y1],
                            "w": img.width, "h": img.height})
        manifest[group] = entries
        print(f"  {group}: {len(entries)} parts")

    path = os.path.join(OUT, "parts.json")
    with open(path, "w") as f:
        json.dump(manifest, f, indent=1)
    print(f"  + parts.json")


# ---------------------------------------------------------------- vfx

def do_vfx():
    print("vfx:")
    src = "vfx.png"
    if not os.path.exists(os.path.join(RAW, src)):
        print(f"  ! missing {src}")
        return
    rgb = load(src).astype(np.float32)
    # additive art: brightness is the alpha, colour stays as painted
    lum = rgb.max(axis=2)
    alpha = np.clip(lum * 1.25, 0, 255).astype(np.uint8)
    rgba = np.dstack([rgb.astype(np.uint8), alpha])
    labels, boxes = components(alpha, min_area=1500)
    boxes.sort(key=lambda b: (b[3] // 200, b[2]))
    for i, (lab, area, x0, y0, x1, y1) in enumerate(boxes):
        piece = rgba[y0:y1 + 1, x0:x1 + 1].copy()
        keep = labels[y0:y1 + 1, x0:x1 + 1] == lab
        piece[..., 3] = np.where(keep, piece[..., 3], 0)
        img = Image.fromarray(piece, "RGBA")
        if max(img.size) > PARTS_MAX:
            sc = PARTS_MAX / max(img.size)
            img = img.resize((max(1, round(img.width * sc)),
                              max(1, round(img.height * sc))), Image.LANCZOS)
        save(np.asarray(img), f"vfx/vfx-{i:02d}.png")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("layers", "all"): do_layers()
    if what in ("parts", "all"):  do_parts()
    if what in ("vfx", "all"):    do_vfx()
