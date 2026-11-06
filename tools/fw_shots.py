#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The firmware's own screens, as the site shows them.

Every picture here is a render of the firmware, not a mockup and not a screenshot of a browser: the
frames come from tests/ui_render.c (the same sources the instrument runs) and are composed onto the
site's canvas -- the 240 x 240 panel scaled x3 on the site's background. There is no emulator frame,
no keyboard and no browser chrome in them, because what the portal shows is the screen.

    python3 tools/fw_shots.py [--render]

--render re-runs the host renderer first (it needs build/gen: run the table generator once). Without
it the PPMs already in build/uiA/ppm are used, which is what CI does after the UI test has run.
"""
import os
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PPM = os.path.join(ROOT, "build", "uiA", "ppm")
OUT = os.path.join(ROOT, "web", "site", "assets")
SCALE = 3
CANVAS = (1040, 760)
BG = (2, 6, 15)
AT = (160, 20)

# what the portal shows, and the frame it is: (file, render, caption for the log)
SHOTS = [
    ("fw-home.png", "GAMMA_home.ppm", "HOME: the system, the spectrum and the six tracks"),
    ("fw-visualizer.png", "GAMMA_home_viz.ppm", "the full-screen visualizer (HOME twice)"),
    ("fw-mixer.png", "GAMMA_mixer.ppm", "MIXER: six tracks"),
    ("fw-fx.png", "GAMMA_fx.ppm", "FX: the per-track rack"),
]
LIGHT = (12, 22, 40)


def render():
    exe = os.path.join(ROOT, "build", "host", "ui_render")
    if not os.path.exists(os.path.join(ROOT, "build", "gen", "ui_fonts.h")):
        subprocess.run([sys.executable, "-c",
                        "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"],
                       cwd=ROOT, check=True)
    os.makedirs(os.path.join(ROOT, "build", "host"), exist_ok=True)
    os.makedirs(PPM, exist_ok=True)
    os.makedirs(os.path.join(ROOT, "build", "uiA_slot"), exist_ok=True)
    subprocess.run(["cc", "-w", "-Ibuild/gen", "-Ifirmware/src", "-Itests", "-o", "build/host/ui_render",
                    "tests/ui_render.c", "-lm"], cwd=ROOT, check=True)
    subprocess.run([exe, "build/uiA", "build/uiA_slot"], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)


def compose(ppm, out):
    """The panel, x3, centred on the site's canvas: the same size for every shot, so the gallery's cells
    line up and nothing is stretched."""
    im = Image.open(os.path.join(PPM, ppm)).convert("RGB")
    if im.size != (240, 240):
        raise SystemExit(f"{ppm} is {im.size}, not the panel's 240 x 240")
    big = im.resize((240 * SCALE, 240 * SCALE), Image.NEAREST)
    canvas = Image.new("RGB", CANVAS, BG)
    # a one-pixel frame around the screen, so the dark panel edge reads as an edge and not as the page
    x, y = AT
    canvas.paste(LIGHT, (x - 1, y - 1, x + big.width + 1, y + big.height + 1))
    canvas.paste(big, AT)
    canvas.save(out, optimize=True)
    return os.path.getsize(out)


def main():
    if "--render" in sys.argv:
        render()
    os.makedirs(OUT, exist_ok=True)
    written = []
    for name, ppm, what in SHOTS:
        if not os.path.exists(os.path.join(PPM, ppm)):
            print(f"skip {name}: {ppm} is not in {os.path.relpath(PPM, ROOT)} (run --render)")
            continue
        size = compose(ppm, os.path.join(OUT, name))
        written.append(name)
        print(f"{name}: {size} B, 240 x 240 x{SCALE} on {CANVAS[0]} x {CANVAS[1]} -- {what}")
    if not written:
        raise SystemExit("no frame was written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
