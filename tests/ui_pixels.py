#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""(FASE 5) The screens as a baseline, not as a lint.

tests/ui_render already draws every screen in every palette from the real drawing code and fails on the layout
lint and the alignment of the ink. What it never did was compare the PIXELS: a change to a glyph table, a
palette, a shade or a 1 px offset could pass the whole suite and only be seen by eye. This hashes every frame
the sweep writes and compares the list with the tracked one, so a pixel that moves fails the run.

  ui_pixels.py [PPM_DIR] [--update]

PPM_DIR defaults to build/ui_new/ppm; the list lives in tests/ui_pixels.txt (name + the first 16 hex of the
SHA-256, one per line, sorted). --update rewrites it -- do that only when the pixels were MEANT to change, and
say why in the commit.
"""
import hashlib
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIST = HERE / "ui_pixels.txt"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()[:16]


def read_list():
    if not LIST.exists():
        return {}
    out = {}
    for line in LIST.read_text().splitlines():
        if line.strip():
            name, h = line.split()
            out[name] = h
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update = "--update" in sys.argv
    ppm = Path(args[0] if args else "build/ui_new/ppm")
    files = sorted(ppm.glob("*.ppm"))
    if not files:
        print("ui pixels: no frames in %s -- run tests/ui_render first" % ppm)
        return 2
    now = {f.name: digest(f) for f in files}
    if update:
        LIST.write_text("".join("%s %s\n" % (n, now[n]) for n in sorted(now)))
        print("ui pixels: %d frames recorded in %s" % (len(now), LIST.name))
        return 0
    old = read_list()
    if not old:
        print("ui pixels: %s is missing -- run this with --update once" % LIST.name)
        return 2
    changed = sorted(n for n in now if n in old and old[n] != now[n])
    gone = sorted(n for n in old if n not in now)
    new = sorted(n for n in now if n not in old)
    if not (changed or gone or new):
        print("ui pixels: %d frames identical to the baseline" % len(now))
        return 0
    for n in changed[:20]:
        print("ui pixels: %s changed" % n)
    for n in gone[:10]:
        print("ui pixels: %s is gone" % n)
    for n in new[:10]:
        print("ui pixels: %s is new" % n)
    print("ui pixels: %d changed, %d gone, %d new -- if the pixels were meant to change, "
          "run 'ui_pixels.py --update' and say why in the commit" % (len(changed), len(gone), len(new)))
    return 1


if __name__ == "__main__":
    sys.exit(main())
