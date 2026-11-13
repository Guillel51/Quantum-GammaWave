#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Our own icon glyphs, drawn as SVG and merged into the Fukiai icon font.

  gen_icon_font.py [OUT.ttf] [--base web/fukiai.ttf] [--src assets/icons-svg] [--first E000]

Each assets/icons-svg/qg_<name>.svg is a 24 x 24 grid drawn with strokes only (no fills, no curves), which is
what an icon wants: a 2 px line is a 2 px line at every size the rasteriser asks for. This script converts a
stroked path into a filled outline -- a quad per segment plus a 12-gon disc at every vertex, so caps and joins
are round without a stroker library -- and hangs the result on the base font at a private-use codepoint, one
glyph per file, named qg_<name>. The base font is never modified: the output is a copy.

  UPM is 2048 and the design grid 24, so one grid unit is 2048/24 = 85.33 em units; the ink is laid out from
  y = 0 (the grid's foot) upwards, like every other Fukiai icon (advance = em, so gfx.c centres by ink).

The result is what tools/gen_aa_icons.py rasterises: `python3 tools/gen_icon_font.py` then `build.generate()`,
and gen_aa_icons maps an ICON_X_<NAME> to its qg_<name> glyph (EXTRA, tools/gen_aa_icons.py).
"""
import argparse
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GRID = 24.0
DISC = 12          # the polygon a cap / join is approximated with


def nums(s):
    return [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", s or "")]


def subpaths(d):
    """a path `d` (M L H V Z, absolute) as a list of point lists. Every M starts a new subpath; Z closes it."""
    tokens = re.findall(r"[MLHVZmlhvz]|-?\d+(?:\.\d+)?", d)
    out, cur, closed, pos, i = [], [], False, (0.0, 0.0), 0
    while i < len(tokens):
        t = tokens[i]
        if t == "M":
            if len(cur) > 1:
                out.append((cur, closed))
            pos = (float(tokens[i + 1]), float(tokens[i + 2]))
            cur, closed, i = [pos], False, i + 3
        elif t == "L":
            pos = (float(tokens[i + 1]), float(tokens[i + 2]))
            cur.append(pos)
            i += 3
        elif t == "H":
            pos = (float(tokens[i + 1]), pos[1])
            cur.append(pos)
            i += 2
        elif t == "V":
            pos = (pos[0], float(tokens[i + 1]))
            cur.append(pos)
            i += 2
        elif t == "Z":
            closed, i = True, i + 1
        else:
            raise SystemExit(f"gen_icon_font: unsupported path command {t!r} in {d!r}")
    if len(cur) > 1:
        out.append((cur, closed))
    return out


def shape_points(el):
    """(list of point lists, closed) for one SVG element, in the 24 grid."""
    tag = el.tag.split("}")[-1]
    a = el.attrib
    if tag == "path":
        return [(p, c) for p, c in subpaths(a.get("d", ""))]
    if tag == "line":
        return [([(float(a["x1"]), float(a["y1"])), (float(a["x2"]), float(a["y2"]))], False)]
    if tag == "polyline" or tag == "polygon":
        v = nums(a.get("points"))
        pts = list(zip(v[0::2], v[1::2]))
        return [(pts, tag == "polygon")]
    if tag == "rect":
        x, y, w, h = (float(a[k]) for k in ("x", "y", "width", "height"))
        return [([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], True)]
    if tag == "circle":
        cx, cy, r = (float(a[k]) for k in ("cx", "cy", "r"))
        n = 16
        return [([(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n))
                  for k in range(n)], True)]
    return []


def disc(cx, cy, r, n=DISC):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def area(pts):
    return sum(pts[i][0] * pts[i - 1][1] - pts[i - 1][0] * pts[i][1] for i in range(len(pts))) / 2.0


def stroke_outline(pts, closed, w):
    """the stroke of a polyline as contours (quads per segment, a disc per vertex), all wound the same way."""
    half = w / 2.0
    segs = list(zip(pts, pts[1:])) + ([(pts[-1], pts[0])] if closed and len(pts) > 2 else [])
    out = []
    for (x0, y0), (x1, y1) in segs:
        dx, dy = x1 - x0, y1 - y0
        n = math.hypot(dx, dy)
        if n < 1e-9:
            continue
        ux, uy = -dy / n * half, dx / n * half
        out.append([(x0 + ux, y0 + uy), (x1 + ux, y1 + uy), (x1 - ux, y1 - uy), (x0 - ux, y0 - uy)])
    for x, y in pts:
        out.append(disc(x, y, half))
    return [c if area(c) > 0 else c[::-1] for c in out]


def svg_contours(path):
    root = ET.parse(path).getroot()
    contours = []

    def walk(el, width):
        w = float(el.attrib.get("stroke-width", width))
        for shp, closed in shape_points(el):
            contours.extend(stroke_outline(shp, closed, w))
        for ch in el:
            walk(ch, w)

    for el in root:
        walk(el, 2.0)
    return contours


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?", default=str(ROOT / "web" / "fukiai_qg.ttf"))
    ap.add_argument("--base", default=str(ROOT / "web" / "fukiai.ttf"))
    ap.add_argument("--src", default=str(ROOT / "assets" / "icons-svg"))
    ap.add_argument("--first", default="E000", help="first private-use codepoint, hex (one per icon, in name order)")
    a = ap.parse_args()

    from fontTools.pens.ttGlyphPen import TTGlyphPen
    from fontTools.ttLib import TTFont

    files = sorted(Path(a.src).glob("qg_*.svg"))
    if not files:
        raise SystemExit(f"gen_icon_font: no qg_*.svg in {a.src}")
    f = TTFont(a.base, recalcBBoxes=False)
    upm = f["head"].unitsPerEm
    s = upm / GRID
    order = f.getGlyphOrder()
    cp = int(a.first, 16)
    added = []
    for p in files:
        name = "qg_" + p.stem[3:]
        cont = svg_contours(p)
        if not cont:
            raise SystemExit(f"gen_icon_font: {p.name} draws nothing")
        pen = TTGlyphPen(None)
        for c in cont:
            pen.moveTo((round(c[0][0] * s), round((GRID - c[0][1]) * s)))
            for x, y in c[1:]:
                pen.lineTo((round(x * s), round((GRID - y) * s)))
            pen.closePath()
        g = pen.glyph()
        g.recalcBounds(f["glyf"])
        if g.xMax - g.xMin < upm * 0.25 or g.yMax - g.yMin < upm * 0.25:
            raise SystemExit(f"gen_icon_font: {p.name} is too small to read: {g.xMax - g.xMin} x {g.yMax - g.yMin}")
        f["glyf"].glyphs[name] = g
        f["hmtx"][name] = (upm, g.xMin)
        order.append(name)
        for t in f["cmap"].tables:
            if t.format in (4, 12):        # (a format-0 subtable only holds Latin-1: a PUA codepoint cannot go in it)
                t.cmap[cp] = name
        added.append((name, cp, g, len(cont), sum(len(c) for c in cont)))
        cp += 1
    f.setGlyphOrder(order)
    f["maxp"].numGlyphs = len(order)
    f.save(a.out)
    for name, c, g, nc, np_ in added:
        print(f"  U+{c:04X} {name:12s} ink {g.xMin:5d}..{g.xMax:5d} x {g.yMin:5d}..{g.yMax:5d}  "
              f"{nc} contours, {np_} points")
    print(f"icon font: {len(added)} glyphs from {Path(a.src).name} -> {a.out} (base {Path(a.base).name}, "
          f"{len(order)} glyphs)")


if __name__ == "__main__":
    sys.exit(main())
