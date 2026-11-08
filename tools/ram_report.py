#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""What the target build actually spends, region by region.

Reads `firmware/app.ld` for the memory map and `build/felucca.elf` for the sections and the symbols, and
prints the free space left in each region plus the biggest consumers, attributed to the source file that
declares them. `build.py` gates the four budgets it knows about (RAM code, RAM .data+.bss, the pool's
spare and the call reach); this says *where* the bytes went, which is what a decision needs.

Every number here is a **static target build** measurement: section sizes come from the linker, symbol
sizes from `nm` on the AC791N ELF. Nothing here is a host estimate or a device reading.

    python3 tools/ram_report.py                     # the table, the regions, the top symbols
    python3 tools/ram_report.py --top 40            # more of them
    python3 tools/ram_report.py --json build/ram.json
    python3 tools/ram_report.py --region RAM        # one region, in symbol order

Without `build/felucca.elf` it says so and exits 2 (a report needs a build; it does not invent one).
"""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LD = ROOT / "firmware" / "app.ld"
ELF = ROOT / "build" / "felucca.elf"
SRC = ROOT / "firmware" / "src"
HAL = ROOT / "firmware" / "hal"

# The ELF's sections as app.ld places them. A section's region decides which budget it spends; `.text`
# is the XIP image (code first, the constants after `_ecode`), and the three RAM-code pieces share one
# 24 KiB window, so they are listed together.
SECTION_REGION = {
    ".text": "XIP",
    ".ram_text": "RAMCODE",
    ".c1_text": "RAMCODE",
    ".hot_text": "RAMCODE",
    ".data": "RAM",
    ".bss": "RAM",
    ".pool": "POOL",
    ".lowbuf": "LOWBUF",
    ".noinit": "NOINIT",
    ".c1_stacks": "POOL",
}
SECTION_OF_REGION = {
    "RAM": (".data", ".bss"),
    "POOL": (".pool", ".c1_stacks"),
    "LOWBUF": (".lowbuf",),
    "NOINIT": (".noinit",),
    "RAMCODE": (".ram_text", ".c1_text", ".hot_text"),
    "XIP": (".text",),
}


def toolchain():
    """The nm and objdump of the build's own toolchain (the ELF is pi32v2, not x86)."""
    root = os.environ.get("JIELI_TOOLCHAIN")
    cands = []
    if root:
        cands.append(Path(root) / "pi32v2" / "bin")
        cands.append(Path(root) / "bin")
    cands += [ROOT / ".tools" / "toolchain" / "pi32v2" / "bin"]
    for d in cands:
        if (d / "nm").exists() and (d / "objdump").exists():
            return d
    return None


def regions():
    """The MEMORY block of app.ld: name -> (origin, length)."""
    text = LD.read_text()
    block = re.search(r"MEMORY\s*\{(.*?)\n\}", text, re.S)
    if not block:
        raise SystemExit(f"{LD}: no MEMORY block")
    def num(s):
        s = s.strip()
        mul = {"K": 1024, "M": 1024 * 1024}.get(s[-1:].upper(), 1)
        return (int(s[:-1], 0) if mul != 1 else int(s, 0)) * mul

    out = {}
    for name, org, ln in re.findall(r"(\w+)\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(\S+)\s*,\s*LENGTH\s*=\s*(\S+)",
                                    block.group(1)):
        out[name] = (num(org), num(ln))
    if "RAMCODE" not in out:                      # app.ld calls the window RAMTEXT
        out["RAMCODE"] = out.get("RAMTEXT", (0, 0))
    return out


def sections(tc):
    """objdump -h: name -> (size, address)."""
    out = subprocess.run([str(tc / "objdump"), "-h", str(ELF)], capture_output=True, text=True).stdout
    secs = {}
    for line in out.splitlines():
        m = re.match(r"\s*\d+\s+(\.\S+)\s+([0-9A-Fa-f]{8})\s+([0-9A-Fa-f]{16})", line)
        if m:
            secs[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return secs


def symbols(tc):
    """objdump -t: (size, name, section) for the data symbols of the image, section named by the linker."""
    out = subprocess.run([str(tc / "objdump"), "-t", str(ELF)], capture_output=True, text=True).stdout
    # `addr flags [type] .section size name`: a function carries the one-letter type (`g F .text`), a data
    # symbol does not (`l .bss`). `.L*` are the compiler's merged-global blobs: their members are listed
    # inside them, so counting both would count those bytes twice.
    row = re.compile(r"^([0-9a-fA-F]+)\s+\S+\s+(?:[A-Za-z]\s+)?(\.[^\s]+)\s+([0-9a-fA-F]+)\s+(\S+)$")
    best = {}                                     # (section, address) -> (size, name): aliases count once
    for line in out.splitlines():
        m = row.match(line.strip())
        if not m:
            continue
        addr, sect, size, name = int(m.group(1), 16), m.group(2), int(m.group(3), 16), m.group(4)
        if not (addr and size) or name.startswith(".L"):
            continue
        k = (sect, addr)
        if k not in best or size > best[k][0]:
            best[k] = (size, name)
    return [(size, name, s) for (s, _), (size, name) in best.items()]


def owners(names):
    """Symbol -> the firmware source file that declares it (a name may appear in more than one file)."""
    idents = {}                                   # file -> its identifiers as a set (one pass, then O(1))
    for f in list(SRC.glob("*.[ch]")) + list(HAL.glob("*.h")) + list((ROOT / "build" / "gen").glob("*.h")):
        idents[f] = set(re.findall(r"[A-Za-z_]\w*", f.read_text(errors="ignore")))
    where = {}
    order = sorted(idents, key=lambda f: (f.suffix != ".c", ".tools" in str(f)))
    for n in names:
        where[n] = next((f.name for f in order if n in idents[f]), "?")
    return where


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--top", type=int, default=20, help="symbols shown per region (default 20)")
    ap.add_argument("--region", help="report only this region's symbols (RAM, POOL, LOWBUF, NOINIT, RAMCODE, XIP)")
    ap.add_argument("--json", help="also write the report here")
    args = ap.parse_args()

    if not ELF.exists():
        print(f"ram_report: no {ELF.relative_to(ROOT)} — run ./build.sh (or python3 tools/build.py) first",
              file=sys.stderr)
        return 2
    tc = toolchain()
    if not tc:
        print("ram_report: no pi32v2 toolchain (JIELI_TOOLCHAIN, or tools/get_toolchain.sh)", file=sys.stderr)
        return 2

    reg = regions()
    sec = sections(tc)
    syms = symbols(tc)

    # Region totals from the sections, then the free space each region has left.
    per_region = {}
    for s, (size, addr) in sec.items():
        r = SECTION_REGION.get(s)
        if r:
            per_region[r] = per_region.get(r, 0) + size
    rows = []
    for r, used in sorted(per_region.items(), key=lambda kv: -kv[1]):
        org, ln = reg.get(r, (0, 0))
        rows.append({"region": r, "origin": org, "size": ln, "used": used, "free": max(0, ln - used),
                     "pct": round(100.0 * used / ln, 1) if ln else 0.0})

    print(f"# {ELF.relative_to(ROOT)}: static target build (AC791N), sections from the linker\n")
    print(f"{'region':8s} {'origin':>10s} {'size':>8s} {'used':>8s} {'free':>7s} {'used%':>6s}")
    for r in rows:
        print(f"{r['region']:8s} 0x{r['origin']:08X} {r['size']:8d} {r['used']:8d} {r['free']:7d} {r['pct']:5.1f}%")

    # Symbols by region: the section the linker put them in decides it, so a symbol can never be
    # counted in a budget it does not spend.
    by_region = {}
    for size, name, s in syms:
        r = SECTION_REGION.get(s)
        if r:
            by_region.setdefault(r, []).append({"sym": name, "size": size, "section": s})

    where = owners([x["sym"] for v in by_region.values() for x in v])
    for v in by_region.values():
        for x in v:
            x["file"] = where.get(x["sym"], "?")

    shown = [args.region.upper()] if args.region else [r["region"] for r in rows]
    for r in shown:
        items = sorted(by_region.get(r, []), key=lambda x: -x["size"])[:args.top]
        if not items:
            continue
        tot = sum(x["size"] for x in by_region.get(r, []))
        head = "code and constants" if r == "XIP" else "symbols"
        print(f"\n## {r}: {len(by_region.get(r, []))} {head}, {tot} B in named symbols")
        for x in items:
            print(f"  {x['size']:7d} B  {x['sym']:20s} {x['section']:11s} {x['file']}")
        # The unnamed remainder is alignment and the gaps between symbols, not free memory.
        if tot < per_region.get(r, 0):
            print(f"  {per_region[r] - tot:7d} B  (alignment, section padding, unnamed)")

    if args.json:
        Path(args.json).write_text(json.dumps({"elf": str(ELF), "regions": rows,
                                               "symbols": by_region}, indent=1))
        print(f"\nram_report: wrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
