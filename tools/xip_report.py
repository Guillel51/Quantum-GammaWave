#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Where the flash and the RAM go, per source file, from a finished build.

The AC791N link leaves one image (build/felucca.elf): LTO turns every firmware source into a single object,
so the linker map says "build/felucca.o" for nearly all of it and tells nothing about which screen or which
engine spent the bytes. The symbol table still carries every function and every named table, and each one's
name can be traced back to the file that defines it, so this reads the ELF and answers the two questions a
phase actually asks: which files hold the XIP, and which hold the RAM.

  tools/xip_report.py [--elf build/felucca.elf] [--top 20] [--ram]

Class of the measurement: this is the linked image for the target (pi32v2), not a host and not a unit. Every
row is bytes in the file the linker produced; the section each symbol lives in decides whether it is XIP
(flash, executed in place) or RAM (copied at boot or zeroed). Unnamed literals (a single string, a jump
table) belong to no symbol and show up as the unattributed remainder, which must stay small for the split
to mean anything.
"""
import argparse
import os
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "firmware" / "src" if (ROOT / "firmware" / "src").is_dir() else ROOT / "src"
NM = os.path.expanduser("~/.jieli/toolchain/pi32v2/bin/nm")
DEF = re.compile(r"^[A-Za-z_][\w \*\(\),\[\]]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{", re.M)


def owners():
    """function or table name -> the .c that defines it (first definition wins)"""
    out = {}
    for f in sorted(SRC.glob("*.c")):
        text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", f.read_text(errors="ignore"), flags=re.S)
        for m in DEF.finditer(text):
            out.setdefault(m.group(1), f.name)
        for m in re.finditer(r"^\s*(?:static\s+)?(?:const\s+)?[\w ]+\s+\**([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*(?:=|;)", text, re.M):
            out.setdefault(m.group(1), f.name)
    return out


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--elf", default=str(ROOT / "build" / "felucca.elf"))
    ap.add_argument("--top", type=int, default=15)
    ap.add_argument("--ram", action="store_true", help="the RAM table too (.data + .bss)")
    a = ap.parse_args(argv)
    if not Path(a.elf).is_file():
        return f"xip_report: no {a.elf} (build the target first)"
    nm = subprocess.run([NM, "-S", "--radix=d", a.elf], capture_output=True, text=True)
    if nm.returncode:
        return f"xip_report: nm failed ({nm.stderr.strip()[:120]})"
    own = owners()
    xip, ram, unnamed_xip = Counter(), Counter(), 0
    fns = []
    for line in nm.stdout.splitlines():
        p = line.split()
        if len(p) != 4:
            continue
        _, size, kind, name = p
        size = int(size)
        if kind in ("t", "T", "r", "R"):                 # .text, .rodata (folded into .text by app.ld)
            if name.startswith(".L") or name in ("$d", "$a"):
                unnamed_xip += size
                continue
            base = re.sub(r"\.(?:constprop|isra|part|cold)\.\d+$", "", name)   # LTO's renamed copies
            xip[own.get(name) or own.get(base) or "(SDK / headers / literals)"] += size
            fns.append((size, kind, name))
        elif kind in ("d", "D", "b", "B") and a.ram:
            base = re.sub(r"\.(?:constprop|isra|part|cold)\.\d+$", "", name)
            ram[own.get(name) or own.get(base) or "(SDK / headers / literals)"] += size
    total = sum(xip.values())
    print(f"XIP: {total} B attributed" + (f", {unnamed_xip} B in unnamed literals" if unnamed_xip else ""))
    print(f"{'bytes':>8}  share  file")
    for f, s in xip.most_common(a.top):
        print(f"{s:8d}  {100.0 * s / total:5.1f}%  {f}")
    if a.ram:
        rt = sum(ram.values())
        print(f"\nRAM (.data + .bss): {rt} B attributed")
        for f, s in ram.most_common(a.top):
            print(f"{s:8d}  {100.0 * s / rt:5.1f}%  {f}")
    print("\nbiggest single objects:")
    for s, kind, name in sorted(fns, reverse=True)[:12]:
        print(f"{s:8d}  {'XIP' if kind in 'tT' else 'const'}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
