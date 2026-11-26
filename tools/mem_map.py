#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The RAM map of the built firmware, read from the ELF instead of from a document.

    python3 tools/mem_map.py [--top N] [build/felucca.elf]

firmware/app.ld puts the RAM in five windows -- .data+.bss inside the SDK's 96 KiB budget, LOWBUF
(32 KiB), POOL (320 KiB, with core 1's stacks inside it), NOINIT (the projects, surviving a reset) and
the two guarded stacks -- and a document that quotes them goes stale the first time a buffer changes.
This reads the real symbols: each window's start, its used bytes, what is left, the largest consumers
and the padding between them (a gap is alignment the linker could not use).

It never guesses: a window is reported from the linker's own boundary symbols (_low_start/_low_end,
_pool_start/_pool_end, _ni_start/_ni_end, _bss_start/_bss_end). Exit status is 1 when a window's
symbols are missing, which means the ELF is not one of this firmware's.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# (name, window length, lo symbol, hi symbol, extra reserved bytes inside the window)
WINDOWS = [
    # (name, window length, first symbol, last symbol, bytes reserved inside the window, window origin)
    # .data starts at RAM_LO (0x01C08000) before .bss: the SDK's 96 KiB budget covers both, and measuring
    # from _bss_start would leave its 896 B out of the count that build.sh prints.
    (".data+.bss (SDK budget)", 98304, "_bss_start", "_bss_end", 0, 0x01C08000),
    ("LOWBUF", 0x8000, "_low_start", "_low_end", 0, None),
    ("POOL", 0x50000, "_pool_start", "_pool_end", 7168, None),   # + core 1's stacks, placed by the script
    ("NOINIT", 0x3D50, "_ni_start", "_ni_end", 0, None),
]


def nm(elf):
    out = subprocess.run(["nm", "-S", "--size-sort", str(elf)], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        p = line.split()
        # `nm -S` prints "addr size type name"; a symbol with no size (a linker boundary) comes as
        # "addr type name", which is exactly what _low_start / _pool_end are: they must not be dropped.
        if len(p) >= 4:
            try:
                addr, size, name = int(p[0], 16), int(p[1], 16), p[3]
            except ValueError:
                continue
        elif len(p) == 3:
            try:
                addr, size, name = int(p[0], 16), 0, p[2]
            except ValueError:
                continue
        else:
            continue
        syms.append((addr, size, name))
    return syms


def main():
    argv = sys.argv[1:]
    top, args, i = 8, [], 0
    while i < len(argv):
        if argv[i] == "--top" and i + 1 < len(argv):
            top, i = int(argv[i + 1]), i + 2
        else:
            args.append(argv[i])
            i += 1
    elf = Path(args[0]) if args else ROOT / "build" / "felucca.elf"
    if not elf.exists():
        # a host-only checkout (NO_PACKAGE=1) has no ELF: this is a report of the target build, so say so
        # and pass. tests/memory_map_test.py is the gate that fails on a malformed map.
        print(f"mem_map: skipped ({elf} is missing: run ./build.sh or tools/build.py first)")
        return 0
    syms = nm(elf)
    at = {n: a for a, _s, n in syms}
    # `nm -S` leaves out a symbol whose size is 0, and the linker's own boundaries are exactly that:
    # _low_start, _pool_end, _ni_start ... So the addresses come from a plain `nm` for anything missing.
    plain = subprocess.run(["nm", str(elf)], capture_output=True, text=True).stdout
    for line in plain.splitlines():
        p = line.split()
        if len(p) >= 3:
            try:
                at.setdefault(p[2], int(p[0], 16))
            except ValueError:
                pass
    bad = 0
    for name, length, lo_s, hi_s, extra, origin in WINDOWS:
        if lo_s not in at or hi_s not in at:
            print(f"{name:24s} MISSING {lo_s}/{hi_s}")
            bad = 1
            continue
        lo, hi = at[lo_s], at[hi_s]
        lo = origin if origin is not None else lo
        used = hi - lo
        free = length - used - extra
        pct = 100.0 * used / length
        print(f"{name:24s} 0x{lo:08X}  used {used:7d} / {length:7d} B ({pct:5.1f} %)  "
              f"free {free:7d} B" + (f"  (+{extra} B reserved inside the window)" if extra else ""))
        inside = [(s, a, n) for a, s, n in syms if lo <= a < hi]
        inside.sort(reverse=True)
        for s, a, n in inside[:top]:
            print(f"    {s:8d} B  {n}")
        gaps = 0
        ordered = sorted((a, s) for a, s, _n in inside)
        for (a, s), (b, _s2) in zip(ordered, ordered[1:]):
            gaps += max(0, b - (a + s))
        print(f"    .. {len(inside)} symbol(s) in the window, {gaps} B of alignment gap")
    for name, lo, hi in (("user stack", 0x01C76900, 0x01C7A000), ("exception stack", 0x01C7A100, 0x01C7C000)):
        print(f"{name:24s} 0x{lo:08X}..0x{hi:08X}  {hi - lo:6d} B (guarded, app.ld)")
    return bad


if __name__ == "__main__":
    raise SystemExit(main())
