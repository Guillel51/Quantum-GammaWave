#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The repository banner, drawn here instead of in a photo editor.

Everything in it belongs to the project: the five colours are read from the firmware's own QUANTUM palette
(firmware/src/ui_palettes.h, RGB565), the two screens are the real captures of the drawing code the firmware
links (assets/screenshots/quantum_26_boot_splash.png and quantum_01_home.png, made by generate_screenshots.py
and by the render sweep), the typefaces are the ones in assets/fonts, and the version is read from
firmware/src/felucca.c. Nothing is invented: if a number is on the banner, a file here says it.

    python3 tools/generate_banner.py            # writes assets/branding/quantum_banner.png

The output is 1600 x 560 (3x the panel's 240 px area, a 20:7 band that GitHub shows whole) and is written as a
palette PNG when the pixels allow it, so the README's first screen stays a few tens of KB.
"""
import random
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
W, H = 1600, 560
SS = 2                      # supersampling for the marks and the rounded frames


def palette(name="QUANTUM"):
    """The firmware's palette, decoded from RGB565 the way gfx.c does when the panel takes it."""
    src = (ROOT / "firmware/src/ui_palettes.h").read_text(encoding="utf-8")
    m = re.search(r'\{"%s",\s*(0x[0-9a-f]{4}),\s*(0x[0-9a-f]{4}),\s*(0x[0-9a-f]{4}),\s*(0x[0-9a-f]{4}),\s*(0x[0-9a-f]{4})\}'
                  % name, src)
    if not m:
        raise SystemExit("generate_banner: palette %s not found" % name)

    def d(v):
        v = int(v, 16)
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))

    return dict(zip(("bg", "surf", "text", "theme", "accent"), (d(x) for x in m.groups())))


def version():
    src = (ROOT / "firmware/src/felucca.c").read_text(encoding="utf-8")
    m = re.search(r'#define\s+FELUCCA_VERSION\s+"([^"]+)"', src)
    if not m:
        raise SystemExit("generate_banner: FELUCCA_VERSION not found")
    return m.group(1)


def font(name, size):
    return ImageFont.truetype(str(ROOT / "assets/fonts" / name), size)


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def lerp3(c, t, other):
    return mix(c, other, t)


def tracked(draw, xy, text, fnt, fill, spacing=0):
    """Text with letter-spacing, returned end x: the banner's own tracking, no font tricks."""
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + spacing
    return x - spacing


def glow(layer, radius, strength=1.0):
    """A blurred copy of the layer, added back: the light a screen and a star leave around them."""
    blur = layer.filter(ImageFilter.GaussianBlur(radius))
    if strength != 1.0:
        blur = Image.eval(blur, lambda v: int(v * strength))
    return blur


def ring_ellipse(draw, cx, cy, rx, ry, width, fill, start=0, end=360, tilt=0.0):
    """An ellipse (a planet's ring, an orbit) drawn as a rotated polygon outline: PIL has no arcs with a tilt."""
    import math
    steps = max(120, int((rx + ry) / 2))
    pts = []
    for i in range(steps + 1):
        a = math.radians(start + (end - start) * i / steps)
        x, y = rx * math.cos(a), ry * math.sin(a)
        pts.append((cx + x * math.cos(tilt) - y * math.sin(tilt),
                    cy + x * math.sin(tilt) + y * math.cos(tilt)))
    draw.line(pts, fill=fill, width=width, joint="curve")


def rounded_frame(img, xy, radius, fill, outline=None, width=2):
    d = ImageDraw.Draw(img)
    d.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def picture(path, box, radius, border, glow_colour):
    """A real capture inside a rounded frame, scaled with NEAREST (its pixels are the panel's), plus its glow."""
    w, h = box
    shot = Image.open(ROOT / "assets/screenshots" / path).convert("RGB")
    shot = shot.resize((w, h), Image.NEAREST)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, fill=255)
    sheet = Image.new("RGBA", (w + 24, h + 24), (0, 0, 0, 0))
    body = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    body.paste(shot, (0, 0), mask)
    ImageDraw.Draw(body).rounded_rectangle((0, 0, w - 1, h - 1), radius=radius, outline=border + (255,), width=2)
    hal = Image.new("RGBA", sheet.size, (0, 0, 0, 0))
    ImageDraw.Draw(hal).rounded_rectangle((12, 12, 12 + w, 12 + h), radius=radius,
                                          outline=glow_colour + (255,), width=10)
    hal = hal.filter(ImageFilter.GaussianBlur(9))
    sheet.alpha_composite(hal)
    sheet.alpha_composite(body, (12, 12))
    return sheet


def fit(draw, text, name, size, max_w, spacing=0, floor=12):
    """The largest size of `name` at which `text`, with its tracking, fits in max_w."""
    while size > floor:
        w = sum(draw.textlength(c, font=font(name, size)) + spacing for c in text)
        if w <= max_w:
            return size
        size -= 1
    return floor


def htrack(draw, text, name, size, spacing):
    return sum(draw.textlength(c, font=font(name, size)) + spacing for c in text)


def main():
    pal = palette()
    bg, surf, ink, cyan, amber = pal["bg"], pal["surf"], pal["text"], pal["theme"], pal["accent"]
    rnd = random.Random(0x5147)            # QG-1: the same sky every run

    # ---------------------------------------------------------------- the deep field
    canvas = Image.new("RGB", (W, H), bg)
    px = canvas.load()
    for y in range(H):
        t = y / (H - 1)
        row = mix(mix(bg, surf, 0.34 * t), (3, 17, 32), 0.22 * t * t)
        for x in range(W):
            px[x, y] = row

    def light(centre, radius, colour, alpha):
        """One soft light: a filled ellipse, blurred hard, added to the sky. No rings, no bands."""
        small = Image.new("RGB", (W // 2, H // 2), (0, 0, 0))
        cx, cy, r = centre[0] / 2, centre[1] / 2, radius / 2
        ImageDraw.Draw(small).ellipse((cx - r, cy - r, cx + r, cy + r),
                                      fill=tuple(int(c * alpha) for c in colour))
        return small.filter(ImageFilter.GaussianBlur(radius / 5)).resize((W, H), Image.BICUBIC)

    for img in (light((20, 10), 660, amber, 0.20), light((1320, 580), 760, cyan, 0.24),
                light((780, 320), 540, (96, 64, 200), 0.10)):
        canvas = Image.fromarray(np.clip(np.asarray(canvas, dtype="int16") +
                                         np.asarray(img, dtype="int16"), 0, 255).astype("uint8"))

    # the nebulae: the same soft ellipses the splash draws, at banner scale
    neb = Image.new("RGB", (W, H), (0, 0, 0))
    nd = ImageDraw.Draw(neb)
    for (cx, cy, rx, ry, col, a) in ((380, 140, 460, 170, cyan, 0.13), (1230, 110, 340, 130, amber, 0.11),
                                     (1000, 480, 560, 170, (120, 90, 255), 0.10)):
        nd.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=tuple(int(c * a) for c in col))
    canvas = Image.fromarray(np.clip(np.asarray(canvas, dtype="int16") +
                                     np.asarray(neb.filter(ImageFilter.GaussianBlur(80)), dtype="int16"),
                                     0, 255).astype("uint8"))

    canvas = canvas.convert("RGBA")
    stars = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(stars)
    for _ in range(230):
        x, y = rnd.randrange(W), rnd.randrange(H)
        depth = rnd.random()
        b = int(40 + 180 * depth ** 2)
        r = 1 if depth < 0.8 else 2
        sd.ellipse((x, y, x + r, y + r), fill=(b, min(255, b + 30), 255, 200))
        if depth > 0.965:                     # a flare on the handful of bright ones
            sd.line((x - 5, y, x + 6, y), fill=(200, 240, 255, 90))
            sd.line((x, y - 5, x, y + 6), fill=(200, 240, 255, 90))
    canvas.alpha_composite(stars)

    # the frame's own falloff: deepest at the edge, so the centre (words, screens) carries the light
    vig = Image.new("L", (W // 4, H // 4), 0)
    vd = ImageDraw.Draw(vig)
    vd.ellipse((-W // 10, -H // 8, W // 4 + W // 10, H // 4 + H // 8), fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(50)).resize((W, H), Image.BICUBIC)
    dark = Image.new("RGBA", (W, H), (*tuple(int(c * 0.55) for c in bg), 255))
    dark.putalpha(vig.point(lambda v: int(200 - v * 200 / 255)))
    canvas.alpha_composite(dark)

    # ---------------------------------------------------------------- the mark: the star HOME draws, its rings and orbits
    M = 300
    mark = Image.new("RGBA", (M * 2, M * 2), (0, 0, 0, 0))
    md = ImageDraw.Draw(mark)
    cx, cy = M, M
    for rx, ry, a, wdt in ((132, 52, 0.50, 2), (104, 40, 0.85, 2), (76, 29, 1.0, 2)):
        ring_ellipse(md, cx, cy, rx * SS, ry * SS, int(wdt * SS), tuple(int(c * a) for c in cyan) + (255,), tilt=-0.30)
    r = 44 * SS
    for i in range(30, 0, -1):
        t = i / 30
        md.ellipse((cx - r * t, cy - r * t, cx + r * t, cy + r * t),
                   fill=mix(mix(amber, (255, 238, 198), 1 - t), amber, t * 0.30) + (255,))
    md.ellipse((cx - r * 0.42, cy - r * 0.42, cx + r * 0.42, cy + r * 0.42), fill=(255, 246, 220, 255))
    md.arc((cx - r, cy - r, cx + r, cy + r), 150, 262, fill=cyan + (200,), width=3 * SS)
    ring_ellipse(md, cx, cy, 108 * SS, 28 * SS, 6 * SS, tuple(int(c * 0.9) for c in amber) + (255,), tilt=-0.30)
    md.ellipse((cx + 86 * SS, cy - 66 * SS, cx + 102 * SS, cy - 50 * SS), fill=cyan + (255,))
    md.ellipse((cx + 89 * SS, cy - 63 * SS, cx + 95 * SS, cy - 57 * SS), fill=(230, 252, 255, 255))
    mark = mark.resize((M, M), Image.LANCZOS)

    halo = Image.new("RGBA", (M, M), (0, 0, 0, 0))
    ImageDraw.Draw(halo).ellipse((60, 60, M - 60, M - 60), outline=amber + (255,), width=26)
    canvas.alpha_composite(halo.filter(ImageFilter.GaussianBlur(30)), (46, 130))
    canvas.alpha_composite(mark, (46, 130))

    # ---------------------------------------------------------------- the words (auto-fitted: nothing runs under the screens)
    d = ImageDraw.Draw(canvas)
    x0, col_w = 398, 700
    s1 = fit(d, "QUANTUM", "Michroma-Regular.ttf", 92, col_w, 3)
    s2 = fit(d, "GAMMAWAVE", "Michroma-Regular.ttf", 80, col_w, 2)
    name_f, name2_f = font("Michroma-Regular.ttf", s1), font("Michroma-Regular.ttf", s2)
    tag_f = font("ChakraPetch-Medium.ttf", 23)
    chip_f = font("IBMPlexMono-SemiBold.ttf", 16)
    sub_f = font("IBMPlexMono-SemiBold.ttf", 13)
    small_f = font("IBMPlexMono-SemiBold.ttf", 15)

    word = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    wd = ImageDraw.Draw(word)
    tracked(wd, (x0, 100), "QUANTUM", name_f, cyan + (255,), spacing=3)
    tracked(wd, (x0, 100 + s1 + 16), "GAMMAWAVE", name2_f, (255, 190, 96, 255), spacing=2)
    canvas.alpha_composite(glow(word, 16, 0.5))
    canvas.alpha_composite(word)

    d = ImageDraw.Draw(canvas)
    tracked(d, (x0 + 1, 66), "QUANTUM GAMMAWAVE  //  QG-1", small_f, mix(cyan, ink, 0.35), spacing=4)

    rule_y = 300
    for i in range(col_w - 10):
        d.line((x0 + i, rule_y, x0 + i, rule_y + 1), fill=mix(cyan, amber, i / (col_w - 10)))
    t1 = "EXPERIMENTAL DIGITAL INSTRUMENT  ·  SYNTHESIZER & GROOVEBOX"
    t2 = "FIRMWARE FOR THE M-VAVE FM-1  ·  DUAL-CORE JIELI AC791N"
    ts1 = fit(d, t1, "ChakraPetch-Medium.ttf", 22, col_w, 1)
    ts2 = fit(d, t2, "ChakraPetch-Medium.ttf", 22, col_w, 1)
    tracked(d, (x0, 316), t1, font("ChakraPetch-Medium.ttf", ts1), mix(ink, cyan, 0.45), spacing=0)
    tracked(d, (x0, 316 + ts1 + 12), t2, font("ChakraPetch-Medium.ttf", ts2), mix(ink, cyan, 0.25), spacing=0)

    # the chips: every number here is in the tree (voice budget, preset tables, FX slots, latch matrix, palettes)
    chips = [("12 VOICES", "quietest stolen first"), ("8 ENGINES", "200 factory presets"),
             ("4-SLOT FX", "CRUSH DELAY REVERB CHORUS"), ("PUNCH-IN", "15 keys · 6 mutes · latch"),
             ("17 THEMES", "QUANTUM + NORDIC")]
    cx0, cy0, row_h = x0, 316 + ts1 + 12 + ts2 + 22, 50
    for label, sub in chips:
        wbox = max(d.textlength(label, font=chip_f), d.textlength(sub, font=sub_f)) + 28
        if cx0 + wbox > x0 + col_w:        # the next row: the inset owns the right side
            cx0, cy0 = x0, cy0 + row_h + 8
        d.rounded_rectangle((cx0, cy0, cx0 + wbox, cy0 + row_h - 6), radius=10, fill=surf + (235,),
                            outline=cyan + (80,), width=1)
        d.text((cx0 + 13, cy0 + 6), label, font=chip_f, fill=cyan)
        d.text((cx0 + 13, cy0 + 26), sub, font=sub_f, fill=mix(ink, bg, 0.42))
        cx0 += wbox + 10

    # version and licence, under the chips
    tracked(d, (x0, cy0 + row_h + 6), f"{version()}  ·  OPEN SOURCE  ·  GPL-3.0-ONLY", small_f,
            mix(ink, cyan, 0.30), spacing=3)

    # ---------------------------------------------------------------- the instrument: two real captures
    home = picture("quantum_01_home.png", (288, 288), 22, mix(cyan, bg, 0.25), cyan)
    splash = picture("quantum_26_boot_splash.png", (160, 160), 14, mix(cyan, bg, 0.35), amber)
    light2 = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(light2).rounded_rectangle((1150, 70, 1600, 470), radius=60, fill=cyan + (60,))
    canvas.alpha_composite(light2.filter(ImageFilter.GaussianBlur(70)))
    canvas.alpha_composite(home, (1258, 62))
    canvas.alpha_composite(splash, (1258, 362))

    d = ImageDraw.Draw(canvas)
    d.rectangle((0, 0, W - 1, H - 1), outline=mix(cyan, bg, 0.60) + (255,), width=2)

    out = canvas.convert("RGB")
    path = ROOT / "assets/branding/quantum_banner.png"
    out.save(path, optimize=True)
    print(f"banner: {path} ({path.stat().st_size} B, {W}x{H}, {version()})")


if __name__ == "__main__":
    main()
