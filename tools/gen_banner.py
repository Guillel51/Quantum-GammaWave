#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""assets/branding/quantum_banner.png: the project's identity banner.

Drawn from code so it can be regenerated and reviewed in the diff:

    python3 tools/gen_banner.py

The brief it answers: Solar Heat, a 16-bit digital feel, warm gradients, something technological
and something musical, one clear name, no version number (the banner outlives the releases) and no
type small enough to squint at.

How those are made, so the next person can change one without redrawing the rest:

  * the field     a vertical indigo-to-black gradient with a warm sun rising behind the emblem,
                  quantised through a 4x4 Bayer matrix: the bands are the palette's, the dithering
                  is what makes them read as a 16-bit screen instead of a CSS gradient;
  * the sun       concentric rings of the thermal ramp, each ring dithered against the next, so
                  the disc has the same stepped heat the firmware's meters use;
  * the emblem    a hexagon in the accent, the one geometric mark the QUANTUM theme already uses;
  * the music     a spectrum of warm bars along the bottom, in the palette's heat ramp;
  * the name      Michroma, filled with a gold-to-coral gradient, over a darker shadow pass so it
                  holds on the brightest part of the sun.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1600, 520
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "assets", "fonts")

# the QUANTUM theme's own colours (build/gen/ui_palettes.h, UI_QUANTUM_INDEX) plus the thermal ramp
INK = (6, 4, 14)
DEEP = (22, 10, 48)         # 0x0022-adjacent: the panel's violet shadow
VIOLET = (72, 34, 118)
EMBER = (176, 52, 24)
ORANGE = (240, 108, 20)
AMBER = (255, 170, 0)
GOLD = (255, 214, 92)
CREAM = (255, 246, 224)

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def bayer_mask(h, w, levels):
    """A (h, w) float in 0..1: the quantisation threshold of a 4x4 Bayer cell, per level."""
    t = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w] / 16.0
    return (levels + t) / (levels + 1.0)


def gradient(w, h, stops):
    """Vertical gradient through (position 0..1, rgb) stops, as a float array (h, w, 3)."""
    y = np.linspace(0.0, 1.0, h, dtype=np.float32)
    out = np.zeros((h, 3), dtype=np.float32)
    for k in range(len(stops) - 1):
        p0, c0 = stops[k]
        p1, c1 = stops[k + 1]
        m = (y >= p0) & (y <= p1)
        f = (y[m] - p0) / max(p1 - p0, 1e-6)
        out[m] = np.array(c0, dtype=np.float32) * (1 - f)[:, None] + np.array(c1, dtype=np.float32) * f[:, None]
    out[y < stops[0][0]] = np.array(stops[0][1], dtype=np.float32)
    out[y > stops[-1][0]] = np.array(stops[-1][1], dtype=np.float32)
    return np.repeat(out[:, None, :], w, axis=1)


def sun_field(w, h, cx, cy, r):
    """A warm radial sun with a dithered falloff: 0 (no light) .. 1 (core)."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
    v = np.clip(1.0 - d, 0.0, 1.0) ** 1.6
    levels = 9
    q = np.floor(v * levels + bayer_mask(h, w, levels) - 0.5) / levels
    return np.clip(q, 0.0, 1.0)


def thermal(t):
    """The firmware's heat ramp for t 0..1: violet -> ember -> orange -> amber -> gold -> cream."""
    stops = [(0.0, VIOLET), (0.35, EMBER), (0.6, ORANGE), (0.8, AMBER), (0.93, GOLD), (1.0, CREAM)]
    for k in range(len(stops) - 1):
        p0, c0 = stops[k]
        p1, c1 = stops[k + 1]
        if p0 <= t <= p1:
            f = (t - p0) / max(p1 - p0, 1e-6)
            return tuple(int(c0[i] * (1 - f) + c1[i] * f) for i in range(3))
    return stops[-1][1]


def main():
    # The layout is a grid, so nothing can drift into anything else: the name block owns x 96..820 and
    # y 64..410, the emblem its own disc on the right (centre 1330,190), and the spectrum a band at the
    # foot that no type enters (y 440..496).
    # 1. the field: indigo at the top, near-black at the foot, with the emblem's sun mixed in
    base = gradient(W, H, [(0.0, DEEP), (0.5, INK), (1.0, (3, 2, 8))])
    cx, cy, r = 1330, 190, 300
    light = sun_field(W, H, cx, cy, r)[..., None]
    warm = np.array((ORANGE[0] * 0.5, ORANGE[1] * 0.28, ORANGE[2] * 0.10), dtype=np.float32)
    field = np.clip(base + light * warm, 0, 255)

    # 2. the pixel grid and the scanlines: the 16-bit screen the whole thing is drawn on
    field[::4, :, :] *= 0.97
    grid = (np.indices((H, W)).sum(axis=0) % 8 == 0)
    field[grid] *= 1.04
    img = Image.fromarray(np.clip(field, 0, 255).astype(np.uint8), "RGB")
    d = ImageDraw.Draw(img, "RGBA")

    # 3. the sun: one soft core, then rings of the thermal ramp around the emblem
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([cx - 210, cy - 210, cx + 210, cy + 210], fill=EMBER + (52,))
    gd.ellipse([cx - 118, cy - 118, cx + 118, cy + 118], fill=AMBER + (60,))
    glow = glow.filter(ImageFilter.GaussianBlur(52))
    img = Image.alpha_composite(img.convert("RGBA"), glow).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    for k in range(18):
        rr = 150 + 9 * k
        col = thermal(0.30 + 0.035 * k)
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=col + (46 + k * 2,), width=1 + (k % 3 == 0))

    # 4. the emblem: the hexagon the QUANTUM theme itself uses, in the accent, over the sun's core
    hr = 104
    hexa = [(cx + hr * math.cos(math.radians(60 * k + 30)), cy + hr * math.sin(math.radians(60 * k + 30)))
            for k in range(6)]
    d.polygon(hexa, outline=GOLD + (255,), width=5)
    inner = [(cx + hr * 0.6 * math.cos(math.radians(60 * k + 30)),
              cy + hr * 0.6 * math.sin(math.radians(60 * k + 30))) for k in range(6)]
    d.polygon(inner, outline=ORANGE + (235,), width=3)
    d.ellipse([cx - 19, cy - 19, cx + 19, cy + 19], fill=AMBER + (255,))
    d.ellipse([cx - 7, cy - 7, cx + 7, cy + 7], fill=CREAM + (255,))

    # 5. the name: Michroma, measured to fit the block, filled with a gold-to-ember gradient
    tx, ty = 96, 62
    title_f = font("Michroma-Regular.ttf", 96)
    words = ["QUANTUM", "GAMMAWAVE"]
    widest = max(d.textbbox((0, 0), w_, font=title_f)[2] for w_ in words)
    if widest > 720:                                  # the name always fits its column
        title_f = font("Michroma-Regular.ttf", int(96 * 720 / widest))
        widest = max(d.textbbox((0, 0), w_, font=title_f)[2] for w_ in words)
    word_h = d.textbbox((0, 0), "QUANTUM", font=title_f)[3]
    lead = 18
    mask = Image.new("L", (widest + 8, word_h * 2 + lead), 0)
    md = ImageDraw.Draw(mask)
    md.text((0, 0), words[0], font=title_f, fill=255)
    md.text((0, word_h + lead), words[1], font=title_f, fill=255)
    fill = gradient(mask.width, mask.height, [(0.0, GOLD), (0.42, AMBER), (0.75, ORANGE), (1.0, EMBER)])
    fill_img = Image.fromarray(np.clip(fill, 0, 255).astype(np.uint8), "RGB")
    shadow = Image.new("RGBA", mask.size, (0, 0, 0, 0))
    shadow.putalpha(mask.filter(ImageFilter.GaussianBlur(10)).point(lambda v: int(v * 0.9)))
    img.paste((0, 0, 0), (tx + 4, ty + 7), shadow.split()[3])
    img.paste(fill_img, (tx, ty), mask)

    # 6. what it is: two lines, both large, and no version number anywhere
    ty2 = ty + word_h * 2 + lead + 54
    d.rectangle([tx, ty2 - 26, tx + 104, ty2 - 22], fill=ORANGE + (255,))
    d.text((tx, ty2), "OPEN FIRMWARE FOR THE M-VAVE FM-1",
           font=font("ChakraPetch-SemiBold.ttf", 34), fill=CREAM + (255,))
    d.text((tx, ty2 + 50), "SYNTHESIS  /  SEQUENCING  /  PERFORMANCE",
           font=font("IBMPlexMono-SemiBold.ttf", 24), fill=(206, 178, 156, 255))

    # 7. the spectrum: the one musical element, in its own band at the foot
    rng = np.random.default_rng(7)
    n = 72
    x0, x1 = 96, W - 96
    base_y = H - 22
    step = (x1 - x0) / n
    heights = 0.10 + 0.90 * np.abs(np.sin(np.linspace(0, math.pi * 3.1, n))) * (0.45 + 0.55 * rng.random(n))
    for i, hv in enumerate(heights):
        bh = int(hv * 44)
        col = thermal(0.22 + 0.78 * float(hv))
        bx = int(x0 + i * step)
        d.rectangle([bx, base_y - bh, bx + int(step) - 3, base_y], fill=col + (200,))
        d.rectangle([bx, base_y - bh, bx + int(step) - 3, base_y - bh + 2], fill=CREAM + (160,))
    d.rectangle([96, H - 21, W - 96, H - 20], fill=ORANGE + (120,))

    out = os.path.join(ROOT, "assets", "branding", "quantum_banner.png")
    img.save(out, optimize=True)
    print("gen_banner: %dx%d -> %s (%d bytes)" % (W, H, out, os.path.getsize(out)))


if __name__ == "__main__":
    main()
