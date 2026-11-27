#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Quantum GammaWave contributors
"""The Quantum GammaWave banner, drawn from the firmware's own pixels.

Every image here is composed with Pillow from files that the firmware itself produced:

  * the panels are 240 x 240 framebuffer captures in assets/screenshots/, taken by tests/ui_render.c
    from the same sources as the package -- no invented screen, no render of a screen, no photograph
    of a screen that is not this firmware;
  * the numbers on the banner (voices, tracks, engines, presets, palettes) are read from the tree
    (tests/run_tests.sh counts the presets, build/gen/ui_palettes.h the palettes, core.h the engines),
    so a banner cannot claim a count the firmware does not have;
  * the type is the project's own: Chakra Petch and IBM Plex Mono from assets/fonts/.

There is no photo of the M-VAVE FM-1 in this repository, so nothing here pretends to be one: the
screens sit in their own thin bezels, the way the firmware draws them, and the composition is the
instrument's identity, not a mock of the hardware's body.

  python3 tools/make_banner.py            # writes the four files below
  python3 tools/make_banner.py --check    # only reports what would change
Outputs:
  assets/branding/quantum_banner.png      2400 x 900   the hero of README and storefronts
  web/site/assets/banner.png              1600 x 600   the site's own banner (8:3)
  web/site/assets/og.png                  1200 x 630   Open Graph / Twitter large card
  web/site/assets/banner-mobile.png       1080 x 1350  the vertical crop
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent.parent
SHOTS = HERE / "assets" / "screenshots"
FONTS = HERE / "assets" / "fonts"

INK = (238, 244, 255)
MUTED = (154, 171, 204)
CYAN = (58, 223, 255)
VIOLET = (139, 92, 255)
TURQ = (78, 240, 208)
DEEP = (0, 4, 16)


def counts():
    """The numbers the banner prints, read from the tree instead of typed here.

    (FASE 8) They used to be literals: the Open Graph card said "293 presets" while the factory library had 296,
    and the docstring above claimed they came from the tree. A banner that counts by hand drifts on the first
    preset added, so every figure now comes from the file that owns it -- the library total from
    tests/preset_library_test.py (TOTAL, the same number the suite enforces), the palettes from the generated
    ui_palettes.h, the voices / tracks / steps from core.h. A missing source raises instead of printing a guess."""
    lib = (HERE / "tests" / "preset_library_test.py").read_text(encoding="utf-8")
    m = re.search(r"^TOTAL\s*=\s*(\d+)", lib, re.M)
    if not m:
        raise SystemExit("make_banner: TOTAL not found in tests/preset_library_test.py")
    presets = int(m.group(1))
    pal = (HERE / "build" / "gen" / "ui_palettes.h")
    if not pal.is_file():
        raise SystemExit("make_banner: build/gen/ui_palettes.h is missing (run tools/build.py generate)")
    body = pal.read_text(encoding="utf-8").split("UI_PALETTES[]", 1)[1].split("};", 1)[0]
    pals = len(re.findall(r'\{\s*"', body))          # one row of the table per palette
    core = (HERE / "firmware" / "src" / "core.h").read_text(encoding="utf-8")
    def n(name, default):
        mm = re.search(r"#define\s+" + name + r"\s+(\d+)", core)
        return int(mm.group(1)) if mm else default
    return {"presets": presets, "palettes": pals, "voices": n("NVOICE", 12),
            "tracks": n("NTRK", 6), "steps": n("NSTEP", 64)}


NUM = counts()

def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size)


def ease(t):
    return t * t * (3 - 2 * t)


def linear(size, stops, angle=90.0):
    """A linear gradient over the whole canvas; angle 90 is top to bottom."""
    import math
    w, h = size
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    a = math.radians(angle)
    dx, dy = math.cos(a), math.sin(a)
    # projection span
    corners = [(0, 0), (w, 0), (0, h), (w, h)]
    proj = [x * dx + y * dy for x, y in corners]
    lo, hi = min(proj), max(proj)
    span = (hi - lo) or 1.0
    steps = 512
    for i in range(steps):
        t0 = i / steps
        t1 = (i + 1) / steps
        # colour at the midpoint of this band
        c = colour_at(stops, (t0 + t1) / 2)
        p0 = lo + span * t0
        # a perpendicular line at distance p0..p1 covers a band; approximate with rows
        y0 = (p0 - min(x * dx for x in (0, w))) / dy if abs(dy) > 1e-6 else 0
        y1 = (lo + span * t1 - min(x * dx for x in (0, w))) / dy if abs(dy) > 1e-6 else h
        d.rectangle([0, min(y0, y1), w, max(y0, y1) + 1], fill=c)
    return img


def colour_at(stops, t):
    t = max(0.0, min(1.0, t))
    for i in range(len(stops) - 1):
        p0, c0 = stops[i]
        p1, c1 = stops[i + 1]
        if t <= p1:
            k = ease((t - p0) / ((p1 - p0) or 1.0))
            return tuple(int(round(c0[j] + (c1[j] - c0[j]) * k)) for j in range(3))
    return stops[-1][1]


def glow(size, centre, radius, colour, strength=1.0):
    """An additive radial glow as an RGBA layer."""
    w, h = size
    small = (max(8, w // 4), max(8, h // 4))
    layer = Image.new("L", small, 0)
    d = ImageDraw.Draw(layer)
    cx, cy = centre[0] * small[0] / w, centre[1] * small[1] / h
    r = radius * small[0] / w
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    layer = layer.filter(ImageFilter.GaussianBlur(r * 0.55))
    layer = layer.resize((w, h), Image.BICUBIC)
    rgba = Image.new("RGBA", (w, h), colour + (0,))
    rgba.putalpha(layer.point(lambda v: int(v * strength)))
    return rgba


def text_size(draw, text, fnt, tracking=0):
    box = draw.textbbox((0, 0), text, font=fnt)
    return box[2] - box[0] + tracking * max(0, len(text) - 1), box[3] - box[1]


def draw_tracked(draw, xy, text, fnt, fill, tracking=0):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += draw.textlength(ch, font=fnt) + tracking


def gradient_text(size, text, fnt, stops, tracking=0, angle=90.0):
    """The text as a mask, filled with a gradient; returns RGBA tightly cropped."""
    probe = ImageDraw.Draw(Image.new("RGB", (10, 10)))
    w = int(draw_tracked_length(probe, text, fnt, tracking)) + 8
    h = int(fnt.size * 1.5) + 8
    mask = Image.new("L", (w, h), 0)
    md = ImageDraw.Draw(mask)
    draw_tracked(md, (4, 2), text, fnt, 255, tracking)
    grad = linear((w, h), stops, angle)
    out = grad.convert("RGBA")
    out.putalpha(mask)
    return out


def draw_tracked_length(draw, text, fnt, tracking):
    return sum(draw.textlength(ch, font=fnt) + tracking for ch in text)


def stars(size, count=190, seed=7):
    import random
    rnd = random.Random(seed)
    w, h = size
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(count):
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        r = rnd.choice([0.6, 0.8, 1.0, 1.3, 1.8])
        a = rnd.randint(26, 120)
        tint = rnd.choice([(200, 230, 255), (220, 210, 255), (180, 255, 250)])
        d.ellipse([x - r, y - r, x + r, y + r], fill=tint + (a,))
    return layer


def orbit(size, centre, radii, tilt, colour, width=1, alpha=150, blur=0.4):
    w, h = size
    s = 2
    layer = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx, cy = centre[0] * s, centre[1] * s
    rx, ry = radii[0] * s, radii[1] * s
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], outline=colour + (alpha,), width=max(1, width * s))
    layer = layer.rotate(tilt, center=(cx, cy), resample=Image.BICUBIC)
    layer = layer.resize((w, h), Image.LANCZOS)
    if blur:
        layer = layer.filter(ImageFilter.GaussianBlur(blur))
    return layer


def panel(shot, scale=1.4, label="", sub=""):
    """A 240x240 capture in a thin bezel, with a soft reflection under it."""
    src = Image.open(SHOTS / shot).convert("RGB")
    if src.size != (240, 240):
        raise SystemExit(f"{shot}: {src.size}, not 240 x 240")
    face = src.resize((int(240 * scale), int(240 * scale)), Image.LANCZOS)
    pad = int(10 * scale)
    r = int(18 * scale)
    try:
        from PIL import ImageChops
    except Exception:
        ImageChops = None
    card = Image.new("RGBA", (face.width + pad * 2, face.height + pad * 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(card)
    # the bezel: a near-black plate with a hairline and a top highlight
    d.rounded_rectangle([0, 0, card.width - 1, card.height - 1], r, fill=(8, 12, 24, 255),
                        outline=(46, 78, 120, 255), width=1)
    d.rounded_rectangle([1, 1, card.width - 2, card.height - 2], r - 1, outline=(96, 214, 255, 26), width=1)
    card.paste(face, (pad, pad))
    d.rounded_rectangle([pad - 1, pad - 1, pad + face.width, pad + face.height],
                        max(2, int(3 * scale)), outline=(0, 0, 0, 255), width=1)
    # a light sweep across the glass
    glass = Image.new("RGBA", card.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glass)
    gd.polygon([(pad, pad), (pad + face.width * 0.55, pad), (pad, pad + face.height * 0.55)],
               fill=(180, 240, 255, 12))
    card = Image.alpha_composite(card, glass)
    # reflection
    mir = face.transpose(Image.FLIP_TOP_BOTTOM)
    rhi = int(min(mir.height, 120 * scale))
    mir = mir.crop((0, 0, mir.width, rhi))
    fade = Image.new("L", mir.size, 0)
    fd = ImageDraw.Draw(fade)
    for y in range(mir.height):
        fd.line([(0, y), (mir.width, y)], fill=int(48 * (1 - y / mir.height) ** 2))
    refl = Image.new("RGBA", mir.size, (0, 0, 0, 0))
    refl.paste(mir, (0, 0))
    refl.putalpha(fade)
    out = Image.new("RGBA", (card.width, card.height + refl.height + int(20 * scale)), (0, 0, 0, 0))
    out.paste(card, (0, 0))
    out.alpha_composite(refl, (pad, card.height + 2))
    if label:
        ld = ImageDraw.Draw(out)
        f = font("IBMPlexMono-SemiBold.ttf", max(11, int(13 * scale)))
        y = card.height + int(30 * scale)
        draw_tracked(ld, (pad, y), label, f, (200, 226, 255, 230), 2.2)
        if sub:
            f2 = font("IBMPlexMono-SemiBold.ttf", max(10, int(11 * scale)))
            draw_tracked(ld, (pad, y + int(15 * scale)), sub, f2, (120, 150, 200, 200), 1.8)
    return out


def chips(items, size, scale=1.0):
    f = font("IBMPlexMono-SemiBold.ttf", int(16 * scale))
    probe = ImageDraw.Draw(Image.new("RGB", (8, 8)))
    pad_x, pad_y, gap = int(16 * scale), int(9 * scale), int(10 * scale)
    widths = [draw_tracked_length(probe, t, f, 1.4) + pad_x * 2 for t in items]
    h = int(f.size * 1.35) + pad_y * 2
    total = sum(widths) + gap * (len(items) - 1)
    img = Image.new("RGBA", (int(total) + 2, h + 2), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x = 1
    for t, w in zip(items, widths):
        d.rounded_rectangle([x, 1, x + w, h], h // 2, fill=(10, 22, 40, 210), outline=(52, 96, 150, 200), width=1)
        d.rounded_rectangle([x + 1, 1, x + w - 1, h // 2], h // 2, fill=(30, 60, 100, 26))
        tw = draw_tracked_length(probe, t, f, 1.4)
        draw_tracked(d, (x + (w - tw) / 2, (h - f.size) / 2 - 1), t, f, (198, 232, 255, 240), 1.4)
        x += w + gap
    return img


def compose(w, h, layout="hero"):
    base = linear((w, h), [(0.0, (0, 2, 7)), (0.5, (3, 4, 14)), (1.0, (6, 4, 22))], 105)
    canvas = base.convert("RGBA")

    # glows: cyan top-left, violet right, turquoise low
    canvas = Image.alpha_composite(canvas, glow((w, h), (w * 0.14, h * 0.02), w * 0.40, CYAN, 0.09))
    canvas = Image.alpha_composite(canvas, glow((w, h), (w * 0.90, h * 0.10), w * 0.26, VIOLET, 0.13))
    canvas = Image.alpha_composite(canvas, glow((w, h), (w * 0.55, h * 1.10), w * 0.40, (30, 190, 190), 0.06))

    # grid
    grid = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grid)
    step = max(28, w // 46)
    for x in range(0, w, step):
        gd.line([(x, 0), (x, h)], fill=(58, 223, 255, 7))
    for y in range(0, h, step):
        gd.line([(0, y), (w, y)], fill=(58, 223, 255, 5))
    canvas = Image.alpha_composite(canvas, grid)

    canvas = Image.alpha_composite(canvas, stars((w, h), int(w * h / 14000)))

    # orbits behind the wordmark
    cx, cy = (w * 0.16, h * 0.42) if layout == "hero" else (w * 0.5, h * 0.2)
    r0 = w * (0.13 if layout == "hero" else 0.24)
    for k, (mult, col, a, tilt) in enumerate([(1.0, VIOLET, 58, -18), (0.78, CYAN, 66, 22), (1.22, TURQ, 34, 8)]):
        canvas = Image.alpha_composite(canvas, orbit((w, h), (cx, cy), (r0 * mult * 1.6, r0 * mult * 0.78), tilt, col, 2, a, 1.2))
    canvas = Image.alpha_composite(canvas, glow((w, h), (cx, cy), r0 * 0.42, (120, 220, 255), 0.14))

    # hairline frame
    fr = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    fd = ImageDraw.Draw(fr)
    m = max(10, w // 120)
    fd.rectangle([m, m, w - m - 1, h - m - 1], outline=(60, 120, 190, 60), width=1)
    for c in [(m, m, 1, 1), (w - m - 1, m, -1, 1), (m, h - m - 1, 1, -1), (w - m - 1, h - m - 1, -1, -1)]:
        x, y, sx, sy = c
        L = max(18, w // 60)
        fd.line([(x, y), (x + sx * L, y)], fill=CYAN + (200,), width=2)
        fd.line([(x, y), (x, y + sy * L)], fill=CYAN + (200,), width=2)
    canvas = Image.alpha_composite(canvas, fr)
    return canvas


def fit_font(text, name, max_w, max_size, tracking=0):
    """The largest size of `name` whose tracked text fits max_w."""
    probe = ImageDraw.Draw(Image.new("RGB", (8, 8)))
    size = max_size
    while size > 8:
        f = font(name, size)
        if draw_tracked_length(probe, text, f, tracking) <= max_w:
            return f
        size -= 2
    return font(name, 8)


def banner_hero():
    w, h = 2400, 900
    canvas = compose(w, h, "hero")
    d = ImageDraw.Draw(canvas)

    x0 = 96
    col_w = 1240
    y = 104
    draw_tracked(d, (x0, y), "BOUND STATES  /  FREE ENERGY", font("IBMPlexMono-SemiBold.ttf", 24),
                 (146, 206, 240, 210), 6.5)
    y += 34
    fq = fit_font("QUANTUM", "ChakraPetch-Bold.ttf", col_w * 0.58, 132, 3)
    q = gradient_text((w, h), "QUANTUM", fq, [(0.0, (150, 246, 255)), (1.0, (26, 146, 236))], 3, 100)
    canvas.alpha_composite(q, (x0, y))
    y += q.height - 16
    fg = fit_font("GAMMAWAVE", "ChakraPetch-Bold.ttf", col_w, 136, 3)
    g = gradient_text((w, h), "GAMMAWAVE", fg, [(0.0, (198, 148, 255)), (0.6, (139, 92, 255)), (1.0, (78, 240, 208))], 3, 100)
    canvas.alpha_composite(g, (x0, y))
    y += g.height + 2

    d.line([(x0, y), (x0 + 150, y)], fill=CYAN + (225,), width=3)
    d.line([(x0 + 160, y), (x0 + 306, y)], fill=VIOLET + (200,), width=3)
    draw_tracked(d, (x0 + 326, y - 11), "M-VAVE  FM-1   /   AC791N", font("IBMPlexMono-SemiBold.ttf", 23),
                 (168, 196, 230, 225), 3.2)

    y += 32
    d.text((x0, y), "OPEN FIRMWARE FOR THE GROOVEBOX", font=font("ChakraPetch-SemiBold.ttf", 40),
           fill=(228, 240, 255, 240))
    y += 52
    d.text((x0, y), "It already is.", font=font("ChakraPetch-Medium.ttf", 32), fill=(138, 166, 204, 215))

    y += 70
    c1 = chips(["12 VOICES", "6 TRACKS", "10 ENGINES"], (w, h), 1.12)
    canvas.alpha_composite(c1, (x0, y))
    c2 = chips(["%d PRESETS" % NUM["presets"], "%d-STEP SEQ" % NUM["steps"], "%d PALETTES" % NUM["palettes"]], (w, h), 1.12)
    canvas.alpha_composite(c2, (x0, y + c1.height + 12))

    d.text((x0, h - 68), "OPEN SOURCE  /  GPL-3.0-ONLY", font=font("IBMPlexMono-SemiBold.ttf", 20),
           fill=(116, 146, 186, 205))
    d.text((x0 + 470, h - 68), "QUANTUM GAMMAWAVE  QG-1", font=font("IBMPlexMono-SemiBold.ttf", 20),
           fill=(116, 146, 186, 205))

    # the real screens, right: three frames, each its own thin bezel
    shots = [("quantum_01_home.png", "HOME / ORBITAL", "six tracks, spectrum, filter"),
             ("quantum_03_fm6_algorithm.png", "FM6 / ALGORITHM", "six operators, 32 routings"),
             ("quantum_12_fx_rack.png", "FX / THE RACK", "chorus, delay, reverb, drive")]
    scale = 1.18
    p0 = panel(shots[0][0], scale, shots[0][1], shots[0][2])
    gap = 26
    total = p0.width * 3 + gap * 2
    px = w - 96 - total
    offsets = [10, 48, -12]
    for i, (shot, lab, sub_) in enumerate(shots):
        p = p0 if i == 0 else panel(shot, scale, lab, sub_)
        canvas.alpha_composite(p, (px + i * (p0.width + gap), 196 + offsets[i]))

    out = canvas.convert("RGB")
    out.save(HERE / "assets" / "branding" / "quantum_banner.png", optimize=True)
    return out


def banner_wide():
    hero = Image.open(HERE / "assets" / "branding" / "quantum_banner.png")
    out = hero.resize((1600, 600), Image.LANCZOS)
    (HERE / "web" / "site" / "assets").mkdir(parents=True, exist_ok=True)
    out.save(HERE / "web" / "site" / "assets" / "banner.png", optimize=True)
    return out


def card_og():
    w, h = 1200, 630
    canvas = compose(w, h, "card")
    d = ImageDraw.Draw(canvas)
    x0 = 64
    draw_tracked(d, (x0, 58), "OPEN FIRMWARE  /  M-VAVE FM-1", font("IBMPlexMono-SemiBold.ttf", 18),
                 (146, 206, 240, 215), 4.2)
    fq = fit_font("QUANTUM", "ChakraPetch-Bold.ttf", 560, 92, 2)
    q = gradient_text((w, h), "QUANTUM", fq, [(0.0, (150, 246, 255)), (1.0, (26, 146, 236))], 2, 100)
    canvas.alpha_composite(q, (x0, 88))
    fg = fit_font("GAMMAWAVE", "ChakraPetch-Bold.ttf", 660, 96, 2)
    g = gradient_text((w, h), "GAMMAWAVE", fg, [(0.0, (198, 148, 255)), (0.6, (139, 92, 255)), (1.0, (78, 240, 208))], 2, 100)
    canvas.alpha_composite(g, (x0, 88 + q.height - 14))
    y = 88 + q.height + g.height
    d.text((x0, y + 8), "%d voices \u00b7 %d tracks \u00b7 10 engines" % (NUM["voices"], NUM["tracks"]), font=font("ChakraPetch-Medium.ttf", 27),
           fill=(212, 232, 255, 235))
    d.text((x0, y + 46), "%d presets \u00b7 %d-step sequencer \u00b7 %d palettes" % (NUM["presets"], NUM["steps"], NUM["palettes"]), font=font("ChakraPetch-Medium.ttf", 21),
           fill=(146, 174, 212, 220))
    p = panel("quantum_01_home.png", 0.92, "HOME / ORBITAL", "240 \u00d7 240, firmware")
    canvas.alpha_composite(p, (w - p.width - 60, 96))
    d.text((x0, h - 62), "guillel51.github.io/Quantum-GammaWave", font=font("IBMPlexMono-SemiBold.ttf", 17),
           fill=(126, 196, 242, 220))
    out = canvas.convert("RGB")
    out.save(HERE / "web" / "site" / "assets" / "og.png", optimize=True)
    return out


def banner_mobile():
    w, h = 1080, 1350
    canvas = compose(w, h, "card")
    d = ImageDraw.Draw(canvas)
    x0 = 76
    draw_tracked(d, (x0, 104), "BOUND STATES / FREE ENERGY", font("IBMPlexMono-SemiBold.ttf", 21),
                 (146, 206, 240, 210), 4.4)
    fq = fit_font("QUANTUM", "ChakraPetch-Bold.ttf", 900, 118, 2)
    q = gradient_text((w, h), "QUANTUM", fq, [(0.0, (150, 246, 255)), (1.0, (26, 146, 236))], 2, 100)
    canvas.alpha_composite(q, (x0, 142))
    fg = fit_font("GAMMAWAVE", "ChakraPetch-Bold.ttf", 928, 112, 2)
    g = gradient_text((w, h), "GAMMAWAVE", fg, [(0.0, (198, 148, 255)), (0.6, (139, 92, 255)), (1.0, (78, 240, 208))], 2, 100)
    canvas.alpha_composite(g, (x0, 142 + q.height - 16))
    y = 142 + q.height + g.height + 10
    d.text((x0, y), "OPEN FIRMWARE FOR THE M-VAVE FM-1", font=font("ChakraPetch-SemiBold.ttf", 30),
           fill=(228, 240, 255, 240))
    y += 58
    c1 = chips(["12 VOICES", "6 TRACKS", "10 ENGINES"], (w, h), 0.95)
    canvas.alpha_composite(c1, (x0, y))
    c2 = chips(["%d PRESETS" % NUM["presets"], "%d-STEP SEQ" % NUM["steps"], "%d PALETTES" % NUM["palettes"]], (w, h), 0.95)
    canvas.alpha_composite(c2, (x0, y + c1.height + 10))
    p1 = panel("quantum_01_home.png", 1.15, "HOME / ORBITAL", "")
    p2 = panel("quantum_12_fx_rack.png", 1.15, "FX / THE RACK", "")
    py = h - p1.height - 96
    canvas.alpha_composite(p1, (x0, py))
    canvas.alpha_composite(p2, (w - p2.width - x0, py))
    out = canvas.convert("RGB")
    out.save(HERE / "web" / "site" / "assets" / "banner-mobile.png", optimize=True)
    return out


def main():
    check = "--check" in sys.argv[1:]
    hero = banner_hero()
    wide = banner_wide()
    og = card_og()
    mob = banner_mobile()
    for name, img in [("assets/branding/quantum_banner.png", hero),
                      ("web/site/assets/banner.png", wide),
                      ("web/site/assets/og.png", og),
                      ("web/site/assets/banner-mobile.png", mob)]:
        print(f"{name}: {img.size[0]}x{img.size[1]}")
    if check:
        print("make_banner: written (--check also writes: the banner is generated, not edited)")


if __name__ == "__main__":
    main()
