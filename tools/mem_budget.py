#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The RAM windows, checked against their floors after a target build.

The FM-1 has five RAM windows and the firmware has to fit in all of them: the LOWBUF page (main-loop
staging), RAM (.data+.bss, the 96 KiB budget the linker enforces), POOL (the big zero-initialised FX
lines and canvases), RAMTEXT (code that runs while the flash is busy) and NOINIT. The linker only
refuses a window that is *full*; a change that eats the last few hundred bytes leaves a build that
links and then fails on the unit, at boot or at the first voice. This reads `build/felucca.map` and
checks the free space in each window against a floor, so a feature that spends the headroom has to
say so in `tests/mem_budget.json` instead of discovering it on hardware.

    python3 tools/mem_budget.py                 (after ./build.sh; skips with a note otherwise)
    python3 tools/mem_budget.py --show          prints the table even when it passes

Class: AC791N link map, written by the real toolchain. This measures the firmware's own memory, not a
simulation of it -- but it is a static map, not a stack measurement: the stacks and the heap-free
pool top are printed by build.sh and are not checked here.
"""
import json
import os
import re
import sys

MAP = "build/felucca.map"
FLOORS = "tests/mem_budget.json"

# The windows, from firmware/app.ld (name: length in bytes). Kept here as well as there because a
# linker script is not something this can import; `--show` prints both so a drift is visible.
REGIONS = {
    "LOWBUF": 0x8000,
    "RAM": 96 * 1024,
    "POOL": 0x50000,
    "RAMTEXT": 0x6000,
    "NOINIT": 0x3D50,
}
# which output section each window holds, and how its name appears in the map
SECTIONS = {
    "LOWBUF": [".lowbuf"],
    "RAM": [".data", ".bss"],
    "POOL": [".pool"],
    # (2.5.0) .ramdata: the cold main-loop buffers the RAM window lent to this one (firmware/app.ld)
    "RAMTEXT": [".ram_text", ".ramdata"],
    "NOINIT": [".noinit"],
}


def sizes(path=MAP):
    """Output section -> bytes, from the linker map's own section lines (name, address, size)."""
    out = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"^(\.\w+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\b", line.rstrip())
            if m:
                out[m.group(1)] = int(m.group(3), 16)
    return out


def main():
    show = "--show" in sys.argv
    if not os.path.exists(MAP):
        print("mem budget: no %s (no target build yet) -- skipped" % MAP)
        return 0
    got = sizes()
    try:
        floors = json.load(open(FLOORS, encoding="utf-8"))["free_floor"]
    except (OSError, KeyError, ValueError) as e:
        print("mem budget: cannot read %s (%s)" % (FLOORS, e))
        return 1
    fails = 0
    print("window    length   used    free   floor   (AC791N build, %s)" % MAP)
    for win, length in REGIONS.items():
        used = sum(got.get(s, 0) for s in SECTIONS[win])
        free = length - used
        floor = floors.get(win, 0)
        ok = free >= floor
        if show or not ok:
            print("  %-7s %7d %7d %7d %7d  %s" % (win, length, used, free, floor, "ok" if ok else "FAIL"))
        if not ok:
            fails += 1
    if fails:
        print("mem budget: %d window(s) below their floor -- a feature spent the headroom; raise the "
              "floor in %s only with the measurement that says it is safe" % (fails, FLOORS))
        return 1
    print("mem budget: every window keeps its floor")
    return 0


if __name__ == "__main__":
    sys.exit(main())
