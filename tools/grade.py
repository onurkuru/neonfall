#!/usr/bin/env python3
"""Regrade the city art toward a teal-and-amber night.

The source pixels are magenta and violet. Tinting them in the engine only
darkens that magenta - it cannot move the hue. Pixel art has few enough
colours to regrade properly: build the palette, decide what each entry
becomes, and apply it as a lookup. Identical colours map identically, so
the art keeps its own shading structure.

Magenta and violet mostly become sodium amber; a deterministic minority
stay magenta so the colour still appears, as an accent rather than the
whole city. Cool hues are pushed toward teal. Neutrals are pulled onto a
cold shadow / warm highlight ramp.

  grade.py            regrade the art directories in place
  grade.py --preview  write a before/after strip to /tmp instead
"""
import colorsys, hashlib, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGETS = ["assets/bg", "assets/tiles", "assets/props", "assets/city", "assets/enemy"]

MAGENTA_KEPT = 0.22      # share of magenta palette entries left as magenta
AMBER_H, TEAL_H = 0.085, 0.505


def stable_frac(rgb):
    """A repeatable 0..1 from a colour, so a palette entry always decides the
    same way no matter which file it turns up in."""
    h = hashlib.md5(bytes(rgb)).digest()
    return h[0] / 255.0


def remap(rgb):
    r, g, b = [c / 255.0 for c in rgb]
    h, s, v = colorsys.rgb_to_hsv(r, g, b)

    if s < 0.16:
        # near-neutral: cold in shadow, warm in the highlights
        nh = TEAL_H if v < 0.55 else AMBER_H
        ns = 0.12 + (0.34 if v > 0.75 else 0.16) * (1.0 - v * 0.4)
        nv = v * (0.82 if v < 0.55 else 1.0)
        return tuple(int(c * 255) for c in colorsys.hsv_to_rgb(nh, ns, nv))

    magenta = 0.78 <= h <= 0.97 or h <= 0.03
    violet = 0.68 <= h < 0.78
    if magenta or violet:
        if magenta and stable_frac(rgb) < MAGENTA_KEPT:
            return rgb                                   # kept as the accent
        nh = AMBER_H if v > 0.32 else TEAL_H             # lit turns sodium,
        ns = min(1.0, s * (0.92 if v > 0.32 else 0.70))  # unlit turns cold
        return tuple(int(c * 255) for c in colorsys.hsv_to_rgb(nh, ns, v))

    if 0.40 <= h <= 0.65:                                # blues to true teal
        return tuple(int(c * 255)
                     for c in colorsys.hsv_to_rgb(TEAL_H, min(1.0, s * 1.1), v))
    return rgb


def grade(img):
    a = np.asarray(img)
    rgb, alpha = a[..., :3], a[..., 3]

    flat = rgb.reshape(-1, 3)
    uniq, inverse = np.unique(flat, axis=0, return_inverse=True)
    mapped = np.array([remap(tuple(int(x) for x in c)) for c in uniq], np.uint8)
    out = mapped[inverse].reshape(rgb.shape)

    # never touch what is fully transparent; its colour is meaningless and
    # writing to it only invites edge bleed later
    out = np.where(alpha[..., None] == 0, rgb, out)
    return Image.fromarray(np.dstack([out, alpha]), "RGBA"), len(uniq)


def main():
    if "--preview" in sys.argv:
        src = os.path.join(ROOT, "assets/bg/l5-near.png")
        a = Image.open(src).convert("RGBA")
        b, n = grade(a)
        bg = Image.new("RGBA", a.size, (10, 14, 18, 255))
        strip = Image.new("RGB", (a.width, a.height * 2), (10, 14, 18))
        strip.paste(Image.alpha_composite(bg, a).convert("RGB"), (0, 0))
        strip.paste(Image.alpha_composite(bg, b).convert("RGB"), (0, a.height))
        strip.save("/tmp/grade-preview.png")
        print(f"{n} colours; wrote /tmp/grade-preview.png")
        return

    total = 0
    for d in TARGETS:
        full = os.path.join(ROOT, d)
        if not os.path.isdir(full):
            continue
        for f in sorted(os.listdir(full)):
            if not f.endswith(".png"):
                continue
            path = os.path.join(full, f)
            img, n = grade(Image.open(path).convert("RGBA"))
            img.save(path)
            total += 1
            print(f"  {os.path.relpath(path, ROOT)} ({n} colours)")
    print(f"{total} file(s) regraded")


if __name__ == "__main__":
    main()
