#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Does every character the firmware can draw have a glyph in the face that draws it?

v1.5.14 took nine glyphs out of the generated faces (the Latin-1 extras no string ever used) and the last
printable code ('~', 126) out of the dense range, which recovered 2 480 B of the app slot. That is only
sound while nothing draws one of them: `gfx.c glyph()` falls back to '?' for a code the face does not have,
so a missing glyph is not a crash, it is a wrong pixel that no test looked at before.

This reads the faces the build really emitted (build/gen/ui_fonts.h: first, last and the extras of S, M and
L) and every string literal in firmware/src (and the generated tables it draws from), and fails if a
character has no glyph. It is the static counterpart of the L-only check in tests/ui_test.c.

Class: host, static analysis of the sources and the generated headers. It does not render.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "build" / "gen" / "ui_fonts.h"

if not HDR.exists():
    print("font charset: no build/gen/ui_fonts.h (generate the headers first); skipped")
    sys.exit(0)

hdr = HDR.read_text(encoding="utf-8", errors="ignore")


def face(name):
    """(first, last, extras) of AF_<name>, read from the aafont_t initialiser and its AF_<name>_EX array"""
    m = re.search(rf"static const aafont_t AF_{name} = \{{([^}}]+)\}};", hdr)
    if not m:
        raise SystemExit(f"font charset: AF_{name} not in the generated header")
    fields = [f.strip() for f in m.group(1).split(",")]
    first, last = int(fields[2]), int(fields[3])
    ex = []
    me = re.search(rf"AF_{name}_EX\[\d*\] = \{{([^}}]*)\}};", hdr)
    if me:
        ex = [int(t.strip()) for t in me.group(1).split(",") if t.strip()]
    return first, last, set(ex)


# S and M draw every label and value, so every literal has to be covered by them. L is sparse on purpose (it draws
# the title, the ABOUT wordmark and MENU > LARGE's values, tools/gen_aa_font.py L_CHARS) and which string reaches it
# is a runtime question: tests/ui_test.c already walks every L string and fails on a missing glyph, so it is that
# one's job, not this scan's.
FACES = {n: face(n) for n in ("S", "M")}
ALL = ("S", "M", "L")
ELLIPSIS = 0x2026


def drawable(name, ch):
    first, last, ex = FACES[name]
    c = ord(ch)
    if name == "L" and "a" <= ch <= "z":            # L has capitals only (gfx.c glyph(): folded up)
        c -= 32
    return first <= c <= last or c in ex or c == ELLIPSIS


def literals():
    files = sorted((ROOT / "firmware" / "src").rglob("*.c")) + sorted((ROOT / "firmware" / "src").rglob("*.h"))
    files += sorted((ROOT / "build" / "gen").glob("*.h"))
    lit = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
    for f in files:
        text = f.read_text(encoding="utf-8", errors="ignore")
        for m in lit.finditer(text):
            body = m.group(1)
            if "\\" in body:
                continue                            # an escape sequence: not a plain character
            line = text.count("\n", 0, m.start()) + 1
            for ch in body:
                yield f, line, ch


bad = 0
seen = set()
for f, line, ch in literals():
    for name in FACES:
        if drawable(name, ch):
            continue
        key = (name, ch)
        if key in seen:
            continue
        seen.add(key)
        bad += 1
        print(f"font charset: FAIL  U+{ord(ch):04X} {ch!r} has no glyph in AF_{name} "
              f"(first used at {f.relative_to(ROOT)}:{line})")

if bad:
    print(f"font charset: {bad} character(s) would draw as '?' -- add them to tools/gen_aa_font.py EXTRAS")
    sys.exit(1)
print(f"font charset: S and M cover every literal the firmware draws "
      f"(S {FACES['S'][0]}..{FACES['S'][1]}+{len(FACES['S'][2])}, M {FACES['M'][0]}..{FACES['M'][1]}+{len(FACES['M'][2])}; "
      f"L is checked at run time by tests/ui_test.c)")
