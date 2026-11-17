#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""assets/branding/quantum_banner.png: the project's identity banner, drawn from code.

    python3 tools/gen_banner.py

The brief: a quantum field rather than a space illustration. What is on it is the instrument's own
vocabulary -- the QUANTUM palette decoded out of `ui_palettes.h`, the two 240 x 240 screens the firmware
actually draws, the typefaces in `assets/fonts` -- plus one motif that belongs to the name: a bound state.

How the quantum motif is made, so one piece can change without redrawing the rest:

  * the field   a near-black violet gradient with two faint sources, and the probability density of a
                bound state around the emblem: concentric phase rings whose brightness falls as 1/r,
                drawn before the type and at low alpha so nothing is read through them;
  * the emblem  a nucleus and three elliptical orbits at 60 degrees, each with one electron and a halo,
                over a dotted cloud whose dot density follows the same 1/r falloff;
  * the lattice a 32 px grid of single pixels, the screen the firmware draws on, never over the name;
  * the quanta  discrete points, not a starfield: fixed seed, so the image is reproducible;
  * the screens the HOME page and the FM6 algorithm page, framed small, as references -- not mockups:
                they are the PNGs in `assets/screenshots`.

The banner carries no version and no build hash: it is the same image for every release.

The layout is a grid, so nothing can drift into anything else: the emblem owns x 72..330, the name column
x 372..1060, the screens x 1120..1528, and the foot y 500..552 is a rule and the number ladder below it.
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1600, 560
SS = 3                      # supersampling for every shape: rings, orbits, frames, chips
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "assets", "fonts")
SHOTS = os.path.join(ROOT, "assets", "screenshots")

# the QUANTUM palette (UI_QUANTUM_INDEX, RGB565 decoded the way gfx.c does) and the tones derived from it
INK = (3, 3, 10)
DEEP = (12, 8, 32)          # 0x0022: the QUANTUM background
SURF = (22, 16, 52)         # 0x10a5: the panel
VIOLET = (139, 92, 255)     # theme
CYAN = (62, 255, 255)       # accent 0x3eff
GOLD = (255, 212, 92)       # 0xfd44 lifted for type
EMBER = (255, 122, 30)
CREAM = (240, 246, 255)
MUTED = (150, 160, 196)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgrad(w, h, stops):
    """(h, w, 3) float: a vertical gradient through (position, rgb) stops."""
    y = np.linspace(0.0, 1.0, h, dtype=np.float32)
    out = np.zeros((h, 3), dtype=np.float32)
    for k in range(len(stops) - 1):
        p0, c0 = stops[k]
        p1, c1 = stops[k + 1]
        m = (y >= p0) & (y <= p1)
        f = (y[m] - p0) / max(p1 - p0, 1e-6)
        out[m] = np.array(c0, np.float32) * (1 - f)[:, None] + np.array(c1, np.float32) * f[:, None]
    out[y < stops[0][0]] = np.array(stops[0][1], np.float32)
    out[y > stops[-1][0]] = np.array(stops[-1][1], np.float32)
    return np.repeat(out[:, None, :], w, axis=1)


def glow(w, h, cx, cy, r, colour, strength):
    """an additive radial falloff, (h, w, 3) float."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r
    return (np.clip(1.0 - d, 0, 1) ** 2.2 * strength)[..., None] * np.array(colour, np.float32)


def tracked(draw, xy, text, fnt, fill, tracking=0.0, anchor_left=True):
    """text with letter-spacing; returns the width drawn."""
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + tracking
    return x - xy[0] - (tracking if text else 0)


def tracked_width(draw, text, fnt, tracking=0.0):
    if not text:
        return 0
    return sum(draw.textlength(c, font=fnt) for c in text) + tracking * (len(text) - 1)


def gradient_text(img, xy, lines, fnt, stops, tracking=0.0, shadow=None):
    """Michroma-grade wordmark: the glyph mask filled with a gradient, over a soft shadow."""
    probe = ImageDraw.Draw(img)
    widest = max(tracked_width(probe, ln, fnt, tracking) for ln in lines)
    asc, desc = fnt.getmetrics()
    line_h = asc + desc
    lead = int(line_h * 0.10)
    total_h = int(line_h * len(lines) + lead * (len(lines) - 1))
    mask = Image.new("L", (int(math.ceil(widest)) + 8, total_h + 8), 0)
    md = ImageDraw.Draw(mask)
    for i, ln in enumerate(lines):
        tracked(md, (0, i * (line_h + lead)), ln, fnt, 255, tracking)
    fill = vgrad(mask.width, mask.height, stops)
    fill_img = Image.fromarray(np.clip(fill, 0, 255).astype(np.uint8), "RGB")
    if shadow:
        sh = Image.new("RGBA", mask.size, (0, 0, 0, 0))
        sh.putalpha(mask.filter(ImageFilter.GaussianBlur(shadow[1])).point(lambda v: int(v * shadow[0])))
        img.paste(shadow[2], (int(xy[0]) + shadow[3], int(xy[1]) + shadow[4]), sh.split()[3])
    img.paste(fill_img, (int(xy[0]), int(xy[1])), mask)
    return widest, total_h


def chip(d, xy, label, fnt, accent, pad_x=17, height=38):
    w = tracked_width(d, label, fnt, 1.2)
    x, y = xy
    box = [x, y, x + w + pad_x * 2, y + height]
    d.rounded_rectangle(box, radius=height / 2, fill=accent + (26,), outline=accent + (120,), width=2)
    tracked(d, (x + pad_x, y + (height - fnt.size) / 2 - 3), label, fnt, (232, 240, 255, 255), 1.2)
    return box[2] - box[0]


def main():
    # 1. the field: violet at the crown, black at the foot, with the two sources of the motif
    field = vgrad(W, H, [(0.0, DEEP), (0.55, (8, 6, 22)), (1.0, INK)])
    field += glow(W, H, 196, 300, 520, VIOLET, 0.42)
    field += glow(W, H, 1330, 250, 640, (20, 46, 108), 0.55)
    field += glow(W, H, 196, 300, 150, CYAN, 0.30)
    img = Image.fromarray(np.clip(field, 0, 255).astype(np.uint8), "RGB").convert("RGB")

    # 2. the probability density: phase rings of a bound state, 1/r, drawn before any type
    rings = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    rd = ImageDraw.Draw(rings)
    for k in range(0, 44):
        r = 44 + k * 34
        a = int(max(0.0, 30.0 - k * 0.55) * (0.55 + 0.45 * math.sin(k * 1.7) ** 2))
        if a <= 0:
            continue
        col = lerp(CYAN, VIOLET, min(1.0, k / 30.0))
        rd.ellipse([(196 - r) * SS, (300 - r) * SS, (196 + r) * SS, (300 + r) * SS],
                   outline=col + (a,), width=2 * SS)
    # the same rings, cut where the screens sit, so the references keep their own ground
    img = Image.alpha_composite(img.convert("RGBA"),
                                rings.filter(ImageFilter.GaussianBlur(0.6)).resize((W, H), Image.LANCZOS)
                                ).convert("RGB")

    # 3. the lattice: a plus every 64 px, the screen the firmware draws on, never over the name
    lay = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    for gy in range(32, H, 64):
        for gx in range(32, W, 64):
            a = int(max(8, 26 - 18 * (gx / W)))
            d.line([gx * SS - 2 * SS, gy * SS, gx * SS + 2 * SS, gy * SS], fill=CYAN + (a,), width=SS)
            d.line([gx * SS, gy * SS - 2 * SS, gx * SS, gy * SS + 2 * SS], fill=CYAN + (a,), width=SS)
    img = Image.alpha_composite(img.convert("RGBA"), lay.resize((W, H), Image.LANCZOS)).convert("RGB")

    # 4. the quanta: discrete points, fixed seed, denser where the field is bright
    rng = random.Random(20261117)
    lay = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    for _ in range(340):
        x = rng.uniform(0, W)
        y = rng.uniform(0, H)
        near = math.exp(-(((x - 196) ** 2 + (y - 300) ** 2) ** 0.5) / 260.0)
        a = int(rng.uniform(18, 70) + 90 * near)
        r = rng.choice((1, 1, 1, 1, 2, 2)) * SS
        col = rng.choice((CYAN, VIOLET, CREAM, GOLD))
        d.ellipse([x * SS - r, y * SS - r, x * SS + r, y * SS + r], fill=col + (min(190, a),))
    img = Image.alpha_composite(img.convert("RGBA"), lay.resize((W, H), Image.LANCZOS)).convert("RGB")

    # 5. the emblem: a nucleus, three orbits, an electron on each, a dotted cloud
    lay = Image.new("RGBA", (W * SS, H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(lay)
    cx, cy = 196, 300
    for i in range(150):
        t = i / 149.0
        r = 40 + 300 * t ** 0.62
        ang = i * 2.399963            # golden angle: an even cloud, not a spiral
        px = cx + r * math.cos(ang)
        py = cy + r * math.sin(ang) * 0.94
        a = int(120 * (1.0 - t) ** 1.6) + 12
        col = lerp(CYAN, VIOLET, t)
        rr = (1.2 + 1.5 * (1 - t)) * SS
        d.ellipse([px * SS - rr, py * SS - rr, px * SS + rr, py * SS + rr], fill=col + (min(a, 150),))
    orbits = ((176, 52, 0.0, CYAN), (150, 78, 59.0, VIOLET), (118, 96, 118.0, GOLD))
    for rx, ry, deg, col in orbits:
        pts = []
        for k in range(181):
            a = math.radians(deg) + 2 * math.pi * k / 180
            pts.append(((cx + rx * math.cos(a)) * SS, (cy + ry * math.sin(a)) * SS))
        d.line(pts, fill=col + (60,), width=7 * SS, joint="curve")
        d.line(pts, fill=col + (235,), width=2 * SS, joint="curve")
    # one electron per orbit, at the phase the drawing can show
    for rx, ry, deg, col in orbits:
        a = math.radians(deg) + (2.30 if deg == 0.0 else 5.05 if deg < 60 else 3.75)
        ex, ey = cx + rx * math.cos(a), cy + ry * math.sin(a)
        for g, al in ((22, 22), (13, 45), (7, 90)):
            d.ellipse([(ex - g) * SS, (ey - g) * SS, (ex + g) * SS, (ey + g) * SS], fill=col + (al,))
        d.ellipse([(ex - 4) * SS, (ey - 4) * SS, (ex + 4) * SS, (ey + 4) * SS], fill=CREAM + (255,))
    for g, al in ((46, 20), (28, 34), (15, 54)):
        d.ellipse([(cx - g) * SS, (cy - g) * SS, (cx + g) * SS, (cy + g) * SS], fill=CYAN + (al,))
    d.ellipse([(cx - 9) * SS, (cy - 9) * SS, (cx + 9) * SS, (cy + 9) * SS], fill=CREAM + (255,))
    d.ellipse([(cx - 20) * SS, (cy - 20) * SS, (cx + 20) * SS, (cy + 20) * SS],
              outline=CREAM + (150,), width=2 * SS)
    img = Image.alpha_composite(img.convert("RGBA"), lay.resize((W, H), Image.LANCZOS)).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")

    # 6. the name: Michroma, fitted to its column, two gradients, no version
    title = font("Michroma-Regular.ttf", 96)
    lines = ["QUANTUM", "GAMMAWAVE"]
    widest = max(tracked_width(d, ln, title) for ln in lines)
    if widest > 672:
        title = font("Michroma-Regular.ttf", int(96 * 672 / widest))
    x0, y0 = 372, 96
    tracked(d, (x0, 64), "BOUND  STATES  /  FREE  ENERGY", font("IBMPlexMono-SemiBold.ttf", 17),
            (120, 214, 230, 235), 3.4)
    w1, h1 = gradient_text(img, (x0 - 2, y0), [lines[0]], title,
                           [(0.0, (200, 255, 255)), (0.55, CYAN), (1.0, (58, 140, 255))],
                           shadow=(0.85, 12, (0, 0, 0), 4, 8))
    asc, desc = title.getmetrics()
    line_h = asc + desc
    gradient_text(img, (x0 - 2, y0 + int(line_h * 1.10)), [lines[1]], title,
                  [(0.0, (255, 244, 214)), (0.42, GOLD), (0.80, EMBER), (1.0, (214, 74, 26))],
                  shadow=(0.85, 12, (0, 0, 0), 4, 8))
    d = ImageDraw.Draw(img, "RGBA")

    # 7. the rule under the name, and what the instrument is
    base = y0 + int(line_h * 2.10) + 26
    d.rectangle([x0, base, x0 + 96, base + 4], fill=CYAN + (255,))
    d.rectangle([x0 + 100, base + 1, x0 + 250, base + 3], fill=VIOLET + (200,))
    d.text((x0 + 268, base - 16), "M-VAVE  FM-1  /  AC791N", font=font("IBMPlexMono-SemiBold.ttf", 17),
           fill=(150, 166, 200, 235))
    body = font("ChakraPetch-SemiBold.ttf", 31)
    d.text((x0 - 1, base + 22), "OPEN FIRMWARE FOR A GROOVEBOX", font=body, fill=CREAM + (255,))
    d.text((x0 - 1, base + 60), "TWELVE VOICES   SIX TRACKS   TEN ENGINES", font=body, fill=(214, 222, 242, 255))

    # 8. the chips: the instrument's own numbers, nothing invented
    small = font("IBMPlexMono-SemiBold.ttf", 16)
    row1 = (("12 VOICES", CYAN), ("6 TRACKS", CYAN), ("10 ENGINES", CYAN))
    row2 = (("285 PRESETS", VIOLET), ("64-STEP SEQ", GOLD), ("4-SLOT FX", GOLD), ("19 PALETTES", GOLD))
    y = base + 112
    x = x0 - 1
    for label, col in row1:
        x += chip(d, (x, y), label, small, col) + 12
    y += 50
    x = x0 - 1
    for label, col in row2:
        x += chip(d, (x, y), label, small, col) + 12

    # 9. the foot: a rule, the emission ladder under it, and the licence at the left
    d.rectangle([72, 512, 1528, 513], fill=(60, 70, 110, 160))
    rng2 = random.Random(7)
    lx = 560
    while lx < 1524:
        hgt = rng2.choice((3, 3, 5, 5, 7, 9, 13, 18))
        col = [CYAN, VIOLET, GOLD][rng2.randrange(3)]
        a = 150 if hgt < 10 else 225
        d.rectangle([lx, 520, lx + 2, 520 + hgt], fill=col + (a,))
        lx += rng2.choice((5, 6, 7, 9))
    d.text((72, 528), "OPEN  SOURCE  /  GPL-3.0-ONLY", font=font("IBMPlexMono-SemiBold.ttf", 14),
           fill=(146, 158, 192, 235))

    # 10. the screens: the firmware's own 240 x 240 pages, small, framed, captioned
    for (name, caption, px) in (("quantum_01_home.png", "HOME / ORBITAL", 1128),
                                ("quantum_03_fm6_algorithm.png", "FM6 / ALGORITHM", 1340)):
        shot = Image.open(os.path.join(SHOTS, name)).convert("RGB").resize((182, 182), Image.LANCZOS)
        frame = Image.new("RGBA", (206, 206), (0, 0, 0, 0))
        fd = ImageDraw.Draw(frame)
        fd.rounded_rectangle([0, 0, 205, 205], radius=18, fill=(10, 9, 24, 255),
                             outline=CYAN + (120,), width=2)
        fd.rounded_rectangle([11, 11, 193, 193], radius=10, outline=(40, 44, 74, 255), width=1)
        frame.paste(shot, (12, 12))
        halo = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        hd = ImageDraw.Draw(halo)
        hd.rounded_rectangle([px - 8, 152, px + 206 + 8, 366], radius=24, fill=CYAN + (26,))
        halo = halo.filter(ImageFilter.GaussianBlur(16))
        img = Image.alpha_composite(img.convert("RGBA"), halo).convert("RGB")
        img.paste(frame, (px, 160), frame)
        d = ImageDraw.Draw(img, "RGBA")
        tracked(d, (px + 13, 374), caption, font("IBMPlexMono-SemiBold.ttf", 12), (150, 168, 205, 220), 1.6)

    # 11. the energy levels: the one diagram that names the motif, under the two screens
    lx0, lx1 = 1132, 1524
    levels = (500, 482, 458, 430, 398)
    for i, ly in enumerate(levels):
        d.rectangle([lx0, ly, lx1, ly + 2], fill=lerp(VIOLET, CYAN, i / (len(levels) - 1)) + (190,))
        d.rectangle([lx0, ly, lx0 + 4, ly + 2], fill=CREAM + (235,))
    ax = lx0 + 74
    d.line([ax, levels[1], ax, levels[2]], fill=GOLD + (230,), width=2)
    d.polygon([(ax, levels[2] - 5), (ax - 5, levels[2] + 5), (ax + 5, levels[2] + 5)], fill=GOLD + (230,))
    d.text((ax + 14, levels[2] + 4), "hf", font=font("IBMPlexMono-SemiBold.ttf", 14), fill=GOLD + (235,))
    d.text((ax + 54, levels[2] - 22), "ABSORPTION", font=font("IBMPlexMono-SemiBold.ttf", 11),
           fill=(150, 166, 200, 200))

    # 12. a hairline frame: the whole banner is one screen
    d.rectangle([0, 0, W - 1, H - 1], outline=(52, 62, 104, 120), width=1)

    out = os.path.join(ROOT, "assets", "branding", "quantum_banner.png")
    img.save(out, optimize=True)
    print("gen_banner: %dx%d -> %s (%d bytes)" % (W, H, out, os.path.getsize(out)))


if __name__ == "__main__":
    main()
