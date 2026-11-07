#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Bounds guard for the note-reactive particle pool (src/ui_being.c).

Why this exists: sparks born on HOME used to be painted by whichever screen next drew the
pool, which is how they appeared over the EDIT ribbon's bands -- CHR among them -- and over the
six cells under them. The fix is structural: the pool is owned (a screen draws and advances only
its own sparks) and every draw is bounded to the bay that screen hands in. A future edit that
re-introduces a draw over a control area, or drops the owner filter, fails here.

    python3 tests/particle_bounds_test.py
"""
import os
import re
import sys

SRC = os.path.join(os.path.dirname(__file__), "..", "firmware", "src")


def read(name):
    with open(os.path.join(SRC, name), encoding="utf-8") as f:
        return f.read()


def uncomment(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def body(text, name):
    """The code of a C function, comments stripped: from its signature to the brace in column 0."""
    text = uncomment(text)
    m = re.search(r"\n(?:static )?[A-Za-z_][A-Za-z0-9_ *]*\b" + name + r"\s*\([^;{]*\)\s*\{", text)
    if not m:
        return ""
    i, depth = m.end(), 1
    while i < len(text) and depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[m.end():i]


fails = []


def check(ok, what):
    print(("particles: %-74s %s" % (what, "ok" if ok else "FAIL")))
    if not ok:
        fails.append(what)


def calls_in(code):
    out = []
    for m in re.finditer(r"gw_note_particles\s*\(([^)]*)\)", code):
        args = [a.strip() for a in m.group(1).split(",")]
        if len(args) == 4 and all(re.fullmatch(r"-?\d+", a) for a in args):
            out.append(tuple(int(a) for a in args))
    return out


being = read("ui_being.c")
graph = read("ui_graph.c")

# ---- the pool itself -------------------------------------------------------
check("VZ_OWN_HOME" in being and "VZ_OWN_VIZ" in being,
      "the pool names its owners (VZ_OWN_HOME / VZ_OWN_VIZ)")
check("p->own = vz_np_owner" in body(being, "vz_np_add"),
      "a spark is tagged with the screen that spawned it")
draw = body(being, "gw_note_particles")
check("p->own != own" in draw, "the draw loop skips another screen's sparks")
check("p->life = 0" in draw, "a spark leaving its own bay is dropped, not carried elsewhere")
check("p->x < x0 - 2 || p->x > x0 + w + 2 || p->y < top - 2 || p->y > bot + 2" in draw,
      "the draw is clipped to the rect the caller hands in")
check("VZ_NP_MAX" in being and "malloc" not in being and "calloc" not in being,
      "the pool is a fixed array: no allocation on the UI or audio path")

# ---- the EDIT screen: no sparks on its ribbon -----------------------------
# The first EDIT screen's lower bay is the WRAITH ribbon (FLU WHI VOX CHR BEL SCR) and the cells
# under it: a control with names, glyphs and a playhead. It must draw no sparks at all.
check(body(being, "graph_being") and not calls_in(body(being, "graph_being")),
      "EDIT (graph_being) draws no sparks over its ribbon")

# ---- where the sparks DO belong, each inside its own screen's free area ----
home = calls_in(body(being, "graph_ecosys"))
check(home == [(8, 66, 88, 52)], "HOME draws the pool in its own bay (8,66,88,52): %s" % (home,))
viz = calls_in(body(graph, "graph_home_viz"))
check(viz == [(12, 64, 144, 67)], "the visualizer draws it in its scope field (12,64,144,67): %s" % (viz,))

for scr, bay in (("HOME", home[0] if home else None), ("VISUALIZER", viz[0] if viz else None)):
    if not bay:
        continue
    x, y, w, h = bay
    check(x >= 0 and y >= 0 and x + w <= 240 and y + h <= 240,
          "%s: its bay is inside the 240x240 screen" % scr)
# HOME: the four mini-knobs and the six track cells start at x 100 (the right bay), and the ribbon
# does not exist on HOME at all. The bay must stay clear of the knobs.
check(home and home[0][0] + home[0][2] <= 100, "HOME: its bay stays left of the mini-knobs (x < 100)")
# VISUALIZER: the header band ends at y 57, the phase plot starts at x 164, the spectrum floor at
# y 180. The scope field is between them.
check(viz and viz[0][1] >= 64 and viz[0][0] + viz[0][2] <= 164 and viz[0][1] + viz[0][3] <= 180,
      "VISUALIZER: its bay is the scope field, clear of header, phase plot and spectrum")

print("\nparticles: %d failed checks" % len(fails))
sys.exit(1 if fails else 0)
