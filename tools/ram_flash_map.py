#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Where the bytes are: a per-symbol RAM / flash map of the firmware, from a host compile.

The target image needs `JIELI_TOOLCHAIN` and `AC79_SDK`; without them there is no map file and no
`.data+.bss` number. This reads the same sources compiled for the host ABI and reports, per section,
every object over a threshold, so a buffer can be found (and a duplicate spotted) without the target.
Array sizes of fixed-width types (`int16_t[4096]`, `uint8_t[512]`) are the same number of bytes on
both; pointers and `long` are not, so the small bookkeeping objects are host-shaped and the big
buffers are exact. **Class: host x86-64 build of the real sources. NOT an AC791N measurement**, and
the section split below is the host's: `app.ld` maps `.pool`, `.lowbuf` and `.noinit` into their own
regions, which is why they are listed apart from `.bss` / `.data`.

  python3 tools/ram_flash_map.py [--min BYTES] [--dups] [--object FW.O]

Compiles `web/emu/felucca_web.c` (the emulator's translation unit: every source the device builds)
unless `--object` names one. `--dups` hashes the rodata objects to find two names holding the same
bytes, which is flash paid twice.
"""
import argparse
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware/src"
GEN = ROOT / "build/gen"
TU = ROOT / "web/emu/felucca_web.c"

# app.ld: these are their own regions on the device, not the 96 KiB .data+.bss
APART = (".pool", ".lowbuf", ".noinit", ".ram_text", ".c1_stacks", ".web", ".host")


def build_object():
    out = Path(tempfile.mkdtemp(prefix="rammap_")) / "fw.o"
    cmd = ["cc", "-O2", "-w", f"-I{GEN}", f"-I{SRC}", "-c", str(TU), "-o", str(out)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode:
        sys.exit("ram_flash_map: the host compile failed\n" + r.stderr[-3000:])
    return out


def sections(obj):
    """section index -> (name, type) and name -> (file offset, address) from readelf -SW"""
    txt = subprocess.run(["readelf", "-SW", str(obj)], capture_output=True, text=True).stdout
    out, info = {}, {}
    for line in txt.splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)", line)
        if not m:
            continue
        out[int(m.group(1))] = (m.group(2), m.group(3))
        info[m.group(2)] = (int(m.group(5), 16), int(m.group(4), 16))
    return out, info


def symbols(obj, sec):
    """(name, size, section name, value) for every object with a size, from readelf -sW"""
    txt = subprocess.run(["readelf", "-sW", str(obj)], capture_output=True, text=True).stdout
    out = []
    for line in txt.splitlines():
        f = line.split()
        if len(f) < 8 or not f[0].endswith(":"):
            continue
        try:
            value = int(f[1], 16)
            size = int(f[2])
            ndx = f[6]
        except (ValueError, IndexError):
            continue
        typ = f[3]
        if typ not in ("OBJECT", "FUNC") or size <= 0:
            continue
        name = f[7] if len(f) > 7 else ""
        if ndx == "ABS" or ndx == "UND":
            continue
        try:
            sname = sec[int(ndx)][0]
        except (ValueError, KeyError):
            sname = "?"
        out.append((name, size, sname, value))
    return out


def rodata_bytes(obj, sname, size, value, secinfo):
    """the bytes of a rodata object: its section's file offset plus the symbol's own value"""
    info = secinfo.get(sname)
    if not info:
        return None
    off, addr = info
    pos = off + (value - addr)
    raw = Path(str(obj)).read_bytes()[pos:pos + size]
    return hashlib.sha256(raw).hexdigest() if len(raw) == size else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=int, default=256, help="only objects at least this many bytes")
    ap.add_argument("--dups", action="store_true", help="hash rodata and list identical objects")
    ap.add_argument("--check", action="store_true",
                    help="the suite's gate: fail when two rodata objects hold the same bytes (flash paid twice)")
    ap.add_argument("--object", default=None)
    a = ap.parse_args()

    obj = Path(a.object) if a.object else build_object()
    sec, secinfo = sections(obj)
    syms = symbols(obj, sec)
    if not syms:
        sys.exit("ram_flash_map: no symbols (is readelf there?)")

    groups = {}
    for name, size, sname, _ in syms:
        key = "RAM (.bss/.data)" if sname in (".bss", ".data") else sname
        groups.setdefault(key, []).append((size, name, sname))

    print("QG-1 per-symbol map -- HOST x86-64 build of the real sources, not an AC791N measurement")
    print("  (fixed-width arrays are exact; pointers and long are host-shaped)")
    order = sorted(groups.items(), key=lambda kv: -sum(s for s, _, _ in kv[1]))
    for key, items in order:
        total = sum(s for s, _, _ in items)
        if total < 256:
            continue
        print(f"\n{key}: {total} B in {len(items)} object(s)")
        for size, name, sname in sorted(items, reverse=True):
            if size < a.min:
                continue
            print(f"  {size:7d} B  {name}")

    if a.dups:
        seen = {}
        for name, size, sname, value in syms:
            if not sname.startswith(".rodata") or size < 64:
                continue
            h = rodata_bytes(obj, sname, size, value, secinfo)
            if h:
                seen.setdefault((h, size), []).append(name)
        dup = {k: v for k, v in seen.items() if len(v) > 1}
        print(f"\nrodata duplicates: {len(dup)} group(s)")
        for (h, size), names in sorted(dup.items(), key=lambda kv: -kv[0][1]):
            print(f"  {size:7d} B x {len(names)}: {', '.join(names)}")

    print("\nThe target's own numbers need JIELI_TOOLCHAIN + AC79_SDK (tools/build.py reads the map).")
    if a.check and dup:
        sys.exit("ram_flash_map: %d rodata object(s) duplicated: the same flash paid more than once" % len(dup))


if __name__ == "__main__":
    main()
