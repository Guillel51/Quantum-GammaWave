#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""assets/branding/quantum_banner.png: the identity banner at the top of the README.

Reproduced from here rather than hand-edited, so the artwork can be regenerated:
    python3 tools/gen_banner.py

The title carries the project's name only -- no hardware/product code in it --
and the emblem, grid and palette are the ones the firmware's QUANTUM theme uses.
"""
import os
from PIL import Image, ImageDraw, ImageFont

W, H = 1200, 340
BG = (11, 15, 25)           # #0B0F19, the panel's own background
TEAL = (0, 255, 163)        # #00FFA3, QUANTUM's accent
AMBER = (255, 170, 0)       # #FFAA00, its warm accent
WHITE = (255, 255, 255)
GREY = (160, 170, 176)      # #A0AAB0
GRID = (18, 60, 52)         # the grid, dim enough to stay under the type

FONTS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "fonts")


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def hexagon(cx, cy, r):
    """A flat-top regular hexagon as a point list."""
    import math
    return [
        (cx + r * math.cos(math.radians(60 * k)), cy + r * math.sin(math.radians(60 * k)))
        for k in range(6)
    ]


def main():
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)

    # the faint grid the banner sits on
    for x in range(0, W, 40):
        d.line([(x, 0), (x, H)], fill=GRID, width=1)
    for y in range(0, H, 40):
        d.line([(0, y), (W, y)], fill=GRID, width=1)

    # the frame
    d.rectangle([8, 8, W - 9, H - 9], outline=TEAL, width=3)

    # the emblem: two nested hexagons around the core
    cx, cy = 168, H // 2
    d.polygon(hexagon(cx, cy, 96), outline=TEAL, width=5)
    d.polygon(hexagon(cx, cy, 62), outline=AMBER, width=5)
    d.ellipse([cx - 26, cy - 26, cx + 26, cy + 26], fill=TEAL)

    x0 = 330
    title = font("ChakraPetch-Bold.ttf", 64)
    sub = font("IBMPlexMono-SemiBold.ttf", 21)
    body = font("InterTight[wght].ttf", 21)
    item = font("ChakraPetch-Medium.ttf", 18)

    # the name, and what it is, with no product code in the title
    d.text((x0, 46), "QUANTUM GAMMAWAVE", font=title, fill=TEAL)
    d.text((x0, 126), "ADVANCED SYNTHESIZER & GROOVEBOX FIRMWARE // M-VAVE FM-1",
           font=sub, fill=WHITE)
    d.text((x0, 160), "High-performance dual-core DSP engine and integrated Web Studio workstation",
           font=body, fill=GREY)

    left = [
        ("• 10-Voice Psychoacoustic Polyphony", AMBER),
        ("• 4 Warm Harmonic Saturations (Soft, Tape, Hearth, Flux)", TEAL),
        ("• 17 Themes (Quantum, Solaris, Cyberpunk, Nordic)", WHITE),
    ]
    right = [
        ("• Parametric Master Biquad EQ (Q14)", AMBER),
        ("• Real-Time Visualizer", TEAL),
        ("• +42 Original Presets", WHITE),
    ]
    for col, (cx0, items) in enumerate(((x0, left), (816, right))):
        for i, (s, c) in enumerate(items):
            d.text((cx0, 208 + i * 34), s, font=item, fill=c)

    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "assets", "branding", "quantum_banner.png")
    img.save(out, optimize=True)
    print("gen_banner: %dx%d -> %s (%d bytes)" % (W, H, out, os.path.getsize(out)))


if __name__ == "__main__":
    main()
