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
check("p->x < x0 || p->x > rgt || p->y < top || p->y > bot" in draw,
      "the draw is clipped to the exact rect the caller hands in (no 2 px overshoot)")
check("ex = p->x + p->size > rgt + 1 ? rgt + 1 : p->x + p->size" in draw and
      "ey = p->y + p->size > bot + 1 ? bot + 1 : p->y + p->size" in draw,
      "a 2 px spark is truncated at the bay's edge, not drawn past it")
check("ox >= x0 && ox <= rgt && oy >= top && oy <= bot" in draw,
      "the trail is only drawn when both of its ends are inside the bay")
check("VZ_NP_MAX" in being and "malloc" not in being and "calloc" not in being,
      "the pool is a fixed array: no allocation on the UI or audio path")

# ---- the EDIT screen: no sparks on its ribbon -----------------------------
# The first EDIT screen's lower bay is the WRAITH ribbon (FLU WHI VOX CHR BEL SCR) and the cells
# under it: a control with names, glyphs and a playhead. It must draw no sparks at all.
check(body(being, "graph_being") and not calls_in(body(being, "graph_being")),
      "EDIT (graph_being) draws no sparks over its ribbon")

# ---- where the sparks DO belong, each inside its own screen's free area ----
home = calls_in(body(being, "graph_ecosys"))
check(home == [(9, 67, 86, 50)], "HOME draws the pool inside its bay (9,67,86,50): %s" % (home,))
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

# ---- the HOME solar system: every orbit and planet inside its bay ----------
# The bay is the clip (sys_box); the geometry must not lean on the clip for its size, or a
# planet would be cut at the edge rather than drawn whole. The check is arithmetic, on the
# tables the draw uses: orbit radius + planet radius + a 2 px margin fits in the half bay.
def nums(name, text):
    m = re.search(r"\b" + name + r"\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}", text)
    return [int(v) for v in re.findall(r"-?\d+", m.group(1))] if m else []

def call_ints(name, text):
    m = re.search(r"\b" + name + r"\s*\(\s*(-?\d+[^)]*)\)", text)   # (a call, not the prototype)
    return [int(v) for v in re.findall(r"-?\d+", m.group(1))] if m else []

orx, orys, orr = nums("ORX", being), nums("ORYS", being), nums("ORR", being)
box = call_ints("sys_box", being)
sysc = call_ints("gw_draw_system", being)
# (3.2) the number of planets comes from the source (NPL: HOME drew three, it draws four), so a fifth
# orbit would be checked too instead of quietly leaving the tables a size behind the draw. The check is
# the same arithmetic on every orbit: no table is exempt.
m = re.search(r"#define\s+NPL\s+(\d+)", being)
npl = int(m.group(1)) if m else 3
check(len(orx) == npl and len(orys) == npl and len(orr) == npl and len(box) == 4 and len(sysc) >= 2,
      "HOME: the solar system's %d orbit tables, its clip and its call are all found" % npl)
if len(orx) == npl and len(orr) == npl and len(orys) == npl and len(box) == 4 and len(sysc) >= 2:
    bx, by, bw, bh = box
    cx, cy = sysc[0], sysc[1]
    hw, hh = min(cx - bx, bx + bw - cx), min(cy - by, by + bh - cy)
    worst = 0
    for i in range(npl):
        ex = orx[i] + orr[i] + 2
        ey = orx[i] * orys[i] // 100 + orr[i] + 2
        worst = max(worst, ex - hw, ey - hh)
    check(bx >= 0 and by >= 0 and bx + bw <= 240 and by + bh <= 240,
          "HOME: the solar system's clip is inside the 240x240 screen")
    check(worst <= 0, "HOME: every orbit and planet fits its bay with 2 px to spare (worst %d)" % worst)
    check(cx - bx >= 8 and bx + bw - cx >= 8 and cy - by >= 8 and by + bh - cy >= 8,
          "HOME: the system is centred in its bay (no orbit pushed to an edge)")


# ---- (3.5) HOME's bay answers a touch, one mechanism per world -------------------
inp = read("ui_input.c")
tap = body(inp, "ui_tap")
check(bool(tap) and "gw_planet_at" in tap and "gw_planet_poke" in tap,
      "ui_tap hit-tests the bay and pokes the body it finds")
# (FASE 2) the guard moved: HOME's bay and its deep field are two worlds, and a menu, a
# confirmation, a layer or a name entry owns the screen outright. Assert the shape, not the
# exact text of an expression that legitimately grew.
check(re.search(r"if\s*\(\s*!ui\.home", tap) is not None,
      "ui_tap does nothing off HOME's own screen (it returns unless HOME is up)")
check("gw_viz_hit" in tap and "gw_viz_poke" in tap,
      "ui_tap: the deep field has its own hit test and its own poke")
poke = body(being, "gw_planet_poke")
check("pl_poke_t[i] = ui.frame" in poke and "pl_poke_on[i] = 1u" in poke,
      "a touch stores the frame it happened on: a re-touch restarts, nothing accumulates")
check("vz_np_add" in poke, "the frozen world's fan comes from the bounded pool")
check("malloc" not in being and "calloc" not in being, "the reaction path allocates nothing")
sysdraw = body(being, "gw_draw_system") + body(being, "gw_body_react") + body(being, "gw_planet_poke")
pk = set(re.findall(r"PK_(SPIN|SWELL|RIPPLE|SLING|FROST)", sysdraw))
check(len(pk) == 5, "HOME's five worlds answer five mechanisms: %s" % sorted(pk))
check("static const uint8_t PL_POKE[NPL] = {PK_SPIN, PK_SWELL, PK_RIPPLE, PK_SLING, PK_FROST}" in being,
      "each body is bound to its own reaction (PL_POKE)")
check("ui.frame - pl_poke_t[i]" in body(being, "pl_age"),
      "a reaction is a function of the frame it was touched, not an accumulator")
check("gw_ring_clip" in being and "sys_line" in body(being, "gw_ring_clip"),
      "the touch's rings are clipped by the bay, not painted over the panel")
deep = body(being, "gw_viz_deep")
vizbody = body(graph, "graph_home_viz")
check("gw_viz_deep(ui_viz_q)" in vizbody, "the visualizer draws the deep field")
check(vizbody.find("gw_viz_deep") < vizbody.find("gw_note_particles"),
      "the deep field is under the music's own drawing, not over it")
check("cv_text" not in deep and "cv_rrect" not in deep,
      "the deep field draws no text and no cell (nothing for the layout lint to collide with)")
check("if (q >= 2u)" in deep and "return" in deep.split("if (q >= 2u)")[1][:80],
      "the deep field is off at LOW quality")
check("if (n > GW_EMBER_MAX)" in body(being, "gw_embers"),
      "the dust field is capped whatever a caller asks for")
check("bg_darken" in body(being, "bg_darken") or "ux_gray" in body(being, "bg_darken"),
      "the deeper page stays on the gray axis (GREY / MONO keep every pixel neutral)")

print("\nparticles: %d failed checks" % len(fails))
sys.exit(1 if fails else 0)
