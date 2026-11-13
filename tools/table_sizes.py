#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""What the constant tables cost, per table, in the linked image (AC791N ELF).

  table_sizes.py [build/felucca.elf] [--nm PATH]

The generated headers (ui_fonts.h, ui_icons.h, ui_keycaps.h, ui_shapes.h, felucca_tables.h) are the bulk of
the flash that is not code, and an estimate from the header file's size is wrong by whatever the compiler
pads. This reads the ELF instead: every symbol's real size, grouped by the face or table it belongs to, so a
change in a generator is measured in the image and not in a text file.

Groups (a symbol belongs to the first one whose pattern matches):
  font S / M / L     AF_S_*, AF_M_*, AF_L_*   (glyph data, glyph table, kerning, Huffman table)
  icons              AI*_*                    (cells, offsets, ink boxes, index, the code)
  keycaps            kb_*, KC_*
  shapes             shp_*, SHP_*, ui_shapes*
  palettes           UI_PALETTES, pal_*

Output: one line per symbol (size, address, name), then the group totals and the sum of all of them.
Exit 0 always: it measures, it does not judge.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GROUPS = [("font S", r"^AF_S_"), ("font M", r"^AF_M_"), ("font L", r"^AF_L_"), ("icons", r"^AI[H0-9_]+"),
          ("keycaps", r"^(kb_|KC_)"), ("shapes", r"^(shp_|SHP_|ui_shapes)"), ("palettes", r"^(UI_PALETTES|pal_)")]


def find_nm():
    """the JieLi toolchain's nm, or the system one (any nm can size a symbol table)"""
    home = Path.home() / ".jieli"
    hits = sorted(home.glob("*/pi32v2/bin/*nm")) if home.is_dir() else []
    return str(hits[-1]) if hits else "nm"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("elf", nargs="?", default=str(ROOT / "build" / "felucca.elf"))
    ap.add_argument("--nm", default=None)
    a = ap.parse_args()
    elf = Path(a.elf)
    if not elf.exists():
        raise SystemExit(f"table_sizes: no {elf} (build it first: ./build.sh)")
    out = subprocess.run([a.nm or find_nm(), "--size-sort", "-S", str(elf)], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        m = re.match(r"^([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\S\s+(\S+)$", line.strip())
        if m:
            syms.append((int(m.group(2), 16), m.group(1), m.group(3)))
    if not syms:
        raise SystemExit(f"table_sizes: {a.nm or 'nm'} read no sizes from {elf}")
    totals = {g: 0 for g, _ in GROUPS}
    rows = {g: [] for g, _ in GROUPS}
    for size, addr, name in syms:
        for g, pat in GROUPS:
            if re.match(pat, name):
                totals[g] += size
                rows[g].append((size, addr, name))
                break
    for g, _ in GROUPS:
        if not rows[g]:
            continue
        print(f"\n{g} ({totals[g]} B)")
        for size, addr, name in sorted(rows[g], reverse=True):
            print(f"  {size:6d} B  0x{addr}  {name}")
    print()
    for g, _ in GROUPS:
        print(f"{g:10s} {totals[g]:7d} B")
    print(f"{'total':10s} {sum(totals.values()):7d} B   ({elf.name})")


if __name__ == "__main__":
    sys.exit(main())
