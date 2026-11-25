#!/usr/bin/env python3
"""The XIP audit: what every read-only constant costs and whether anything still points at it.

Why this exists
--------------
The image budget is the only one this firmware cannot grow: 583 612 B of app slot, and `--gc-sections` already
drops what nothing calls. So the question "what XIP data is unreachable?" has an uncomfortable answer that this
tool makes measurable instead of guessed: with `-fdata-sections` on, the linker keeps exactly the read-only
sections something references -- and the real cost of a constant is not whether it is referenced, but how many
bytes it takes to *be* referenced (a hand-written table, a switch table, a font).

What it reports, all from the linked ELF and its map (class: TARGET AC791N, the toolchain's own output):
  * every `.rodata.*` input section: its size, its symbol if it has one, and how many 32-bit words of the
    image point into it (references found by address, which is how pi32v2 encodes them);
  * DEAD: a section nothing in the image points into (a candidate for removal, and rare: gc did its job);
  * the padding the output section pays to align each input section, which is bytes spent on nothing;
  * per category totals (fonts, presets, curves, switch tables, everything else).

Usage: python3 tools/xip_audit.py [--md docs/XIP_AUDIT.md]
Exit status is 0 always: this is an audit, not a gate. `tools/build.py` keeps the budget.
"""
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(ROOT, "build", "felucca.elf")
MAP = os.path.join(ROOT, "build", "felucca.map")

# A section name says a lot about what the bytes are for; the report groups them so the total is legible.
CATS = [
    ("fonts", ("AF_S", "AF_M", "AF_L", "AF_S_G", "ui_fonts", "GLYPH", "glyph")),
    ("presets", ("preset", "PRESET", "ANALOG_PRESETS", "FM4_PRESET", "GENRE")),
    ("curves", ("WT_", "FM6_", "SINE", "sine", "ENV_", "AI12", "AI16", "KC_", "UAC_FIR", "TP", "GP")),
    ("switch tables", ("switch.table",)),
]


def cat_of(name):
    for c, keys in CATS:
        for k in keys:
            if k in name:
                return c
    return "other"


def parse_map(path):
    """[(section, address, size, object)] for every input section the map lists with a size."""
    txt = open(path, encoding="utf-8", errors="replace").read()
    out = []
    pat = re.compile(r"^\s*(\.\S+)\s*\n?\s*0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S+)", re.M)
    for m in pat.finditer(txt):
        out.append((m.group(1), int(m.group(2), 16), int(m.group(3), 16), m.group(4)))
    return out


def parse_symbols(path):
    """[(name, address, size)] for the data symbols in the image (nm -S)."""
    syms = []
    p = subprocess.run(["/usr/bin/nm", "-S", path], capture_output=True, text=True)
    for line in p.stdout.splitlines():
        f = line.split()
        if len(f) == 4 and f[3] not in (".text", ".data", ".bss"):
            try:
                syms.append((f[3], int(f[0], 16), int(f[1], 16)))
            except ValueError:
                pass
    return syms


def main():
    if not os.path.exists(ELF) or not os.path.exists(MAP):
        print("xip_audit: build/felucca.elf and build/felucca.map are needed (./build.sh)", file=sys.stderr)
        return 1
    img = open(ELF, "rb").read()
    syms = parse_symbols(ELF)
    secs = [s for s in parse_map(MAP) if s[0].startswith(".rodata") and s[2]]
    total = sum(s[2] for s in secs)

    # every 4-byte word of the image, as the addresses pi32v2 encodes: a reference is a word inside a section
    words = struct.unpack_from("<%dI" % (len(img) // 4), img)
    refs = {}
    for name, addr, size, obj in secs:
        lo, hi = addr, addr + size
        n = 0
        for w in words:
            if lo <= w < hi:
                n += 1
        refs[(name, addr)] = n

    # the address of each symbol, so a section gets a name a person recognises
    def sym_at(addr):
        best = "."
        for nm, sa, sz in syms:
            if sa <= addr < sa + sz and sz:
                best = nm
        return best

    rows = sorted(secs, key=lambda s: -s[2])
    by_cat = {}
    dead = []
    for name, addr, size, obj in rows:
        c = cat_of(name + " " + sym_at(addr))
        by_cat[c] = by_cat.get(c, 0) + size
        if refs[(name, addr)] == 0:
            dead.append((name, addr, size, obj))

    # padding: the output section's size against the sum of its input sections
    ro = [l for l in open(MAP, encoding="utf-8", errors="replace").read().splitlines() if re.search(r"^\s*\.rodata\s+0x", l)]
    out_size = 0
    if ro:
        f = ro[0].split()
        try:
            out_size = int(f[2], 16)
        except (IndexError, ValueError):
            out_size = 0
    pad = max(0, out_size - total) if out_size else 0

    print("xip_audit: %d read-only sections, %d B in them, %d B of padding" % (len(rows), total, pad))
    print("%-44s %-9s %8s %6s  %s" % ("section", "symbol", "bytes", "refs", "category"))
    for name, addr, size, obj in rows[:28]:
        print("%-44s %-9s %8d %6d  %s" % (name[:44], sym_at(addr)[:9], size, refs[(name, addr)], cat_of(name + " " + sym_at(addr))))
    print()
    for c in sorted(by_cat, key=lambda k: -by_cat[k]):
        print("  %-14s %6d B  %4.1f %%" % (c, by_cat[c], 100.0 * by_cat[c] / total if total else 0.0))
    print("  %-14s %6d B (padding / unnamed)" % ("overhead", pad))
    print()
    if dead:
        print("DEAD sections (nothing points into them; gc left them for a reason worth knowing):")
        for name, addr, size, obj in dead:
            print("  %-40s %6d B  %s" % (name[:40], size, obj))
        print("  %d B in %d sections" % (sum(d[2] for d in dead), len(dead)))
    else:
        print("DEAD sections: none. Every read-only byte is pointed at from the image.")

    md = sys.argv[sys.argv.index("--md") + 1] if "--md" in sys.argv else None
    if md:
        with open(os.path.join(ROOT, md), "w", encoding="utf-8") as f:
            f.write("# The XIP audit (AC791N build)\n\n")
            f.write("Generated by `tools/xip_audit.py` from `build/felucca.elf` and `build/felucca.map`:\n")
            f.write("class **TARGET**, the toolchain's own output, not an estimate.\n\n")
            f.write("| | bytes |\n|---|---|\n")
            f.write("| read-only sections | %d |\n| of which padding | %d |\n" % (total, pad))
            for c in sorted(by_cat, key=lambda k: -by_cat[k]):
                f.write("| %s | %d |\n" % (c, by_cat[c]))
            f.write("\n| section | symbol | bytes | references | category |\n|---|---|---|---|---|\n")
            for name, addr, size, obj in rows:
                f.write("| `%s` | `%s` | %d | %d | %s |\n" % (name, sym_at(addr), size, refs[(name, addr)],
                                                             cat_of(name + " " + sym_at(addr))))
            f.write("\n**Dead sections: %d (%d B).**\n" % (len(dead), sum(d[2] for d in dead)))
        print("xip_audit: wrote %s" % md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
