#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""(ETAPA 1) THE FM-1'S RESOURCES, READ FROM THE BUILD ITSELF, WITH A CEILING EACH.

Every number here comes out of `build/felucca.elf` -- the sections the linker made, the symbols it placed --
plus one constant read from the tree instead of copied (`STOCK_APP_SIZE` in web/fm1pkg.js, which is the app
area a stock FM-1 updater accepts). Nothing is estimated and nothing is remembered from a report:

  RAM      .data + .bss against the 96 KiB budget of AGENTS.md §8, and the free tail after `_bss_end`.
  pool     the same for .pool: what it holds (`_pool_start` to the last object in it) and what is left
           (`_pool_end`), which is the headroom the audio buffers come out of.
  lowbuf   .lowbuf against the 32 KiB it was declared with.
  flash    the XIP sections and the app-slot budget, so the flash side (the resource this firmware runs
           out of first: the slot had 728 B free when the exp2 table was 4100 B of it) has a number too.
  XIP      the largest objects that live in flash, which is where a new table is paid for.

`--check` exits non-zero when a ceiling is crossed, and `tests/resources_test.py` runs it, so a change that
eats the margin fails a test instead of being discovered by a build that no longer links. The ceilings are
the documented ones from AGENTS.md §8 (96 KiB of .data+.bss, 8 KiB of pool spare); they are not tuned here.

  tools/resource_map.py [--elf build/felucca.elf] [--json build/resources.json] [--check]
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BSS_BUDGET = 96 * 1024          # AGENTS.md §8: the linker budget for .data + .bss
POOL_SPARE_MIN = 8 * 1024       # AGENTS.md §8: headroom in POOL_END - pool_top stays >= 8 KiB
LOWBUF_MAX = 32 * 1024          # the .lowbuf the linker declares


def tool(name):
    for p in (name, os.path.join(os.environ.get("JIELI_TOOLCHAIN", ""), "bin", "pi32v2-linux-" + name)):
        try:
            subprocess.run([p, "--version"], capture_output=True, check=True)
            return p
        except Exception:
            continue
    return name


def app_slot():
    """the app area a stock unit's updater accepts: one number, read from the file that pins it"""
    src = open(os.path.join(ROOT, "web", "fm1pkg.js"), encoding="utf-8").read()
    m = re.search(r"STOCK_APP_SIZE\s*=\s*0x([0-9A-Fa-f]+)", src)
    return int(m.group(1), 16) if m else None


def elf_sections(path):
    out = subprocess.run([tool("readelf"), "-SW", path], capture_output=True, text=True)
    if out.returncode:
        raise SystemExit("resource_map: readelf failed: " + out.stderr.strip())
    rows, by_num = [], {}
    for ln in out.stdout.splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)", ln)
        if not m:
            continue
        num, name, typ, addr, off, size = m.groups()
        row = {"num": int(num), "name": name, "type": typ, "addr": int(addr, 16), "size": int(size, 16)}
        rows.append(row)
        by_num[row["num"]] = row
    return rows, by_num


def elf_symbols(path, by_num):
    out = subprocess.run([tool("readelf"), "-sW", path], capture_output=True, text=True)
    if out.returncode:
        raise SystemExit("resource_map: readelf -s failed: " + out.stderr.strip())
    syms = []
    for ln in out.stdout.splitlines():
        m = re.match(r"\s*\d+:\s+([0-9A-Fa-f]+)\s+(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)$", ln)
        if not m:
            continue
        value, size, typ, bind, vis, ndx, name = m.groups()
        sec = by_num[int(ndx)]["name"] if ndx.isdigit() and int(ndx) in by_num else ndx
        syms.append({"name": name, "addr": int(value, 16), "size": int(size),
                     "type": typ, "bind": bind, "section": sec})
    return syms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", default=os.path.join(ROOT, "build", "felucca.elf"))
    ap.add_argument("--json", default=None)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if not os.path.exists(a.elf):
        print("resource_map: no %s (build first)" % os.path.relpath(a.elf, ROOT))
        return 2

    sections, by_num = elf_sections(a.elf)
    syms = elf_symbols(a.elf, by_num)
    sec = {s["name"]: s for s in sections}

    def size(name):
        return sec.get(name, {}).get("size", 0)

    def sym_size(name):
        for s in syms:
            if s["name"] == name:
                return s["size"]
        return None

    slot = app_slot()
    data_bss = size(".data") + size(".bss")
    xip = size(".text")

    # where the pool really ends: the last object that lives in it, from the symbols the linker wrote
    def sym_addr(name, dflt=None):
        for s in syms:
            if s["name"] == name:
                return s["addr"]
        return dflt

    # .pool is NOLOAD: the section ends where the last object ends, and the headroom is what is left of the
    # POOL window once the section and the core-1 stacks (which share it) are in -- exactly what the build
    # script prints, computed the same way, with the window read from the linker script instead of copied
    pool_start = sym_addr("_pool_start")
    pool_end = sym_addr("_pool_end")
    pool_used = (pool_end - pool_start) if (pool_end is not None and pool_start is not None) else None
    pool_win = None
    try:
        ld = open(os.path.join(ROOT, "firmware", "app.ld"), encoding="utf-8").read()
        m = re.search(r"POOL\s*\(rw\)\s*:\s*ORIGIN\s*=\s*0x([0-9A-Fa-f]+)\s*,\s*LENGTH\s*=\s*0x([0-9A-Fa-f]+)", ld)
        if m:
            pool_win = int(m.group(2), 16)
    except OSError:
        pass
    spare = (pool_win - pool_used - size(".c1_stacks")) if (pool_win and pool_used is not None) else None

    top = sorted((s for s in syms if s["size"] and s["section"] in (".bss", ".pool", ".text", ".data")),
                 key=lambda s: -s["size"])[:12]
    top_xip = sorted((s for s in syms if s["size"] and s["section"] == ".text"), key=lambda s: -s["size"])[:10]

    report = {
        "elf": os.path.relpath(a.elf, ROOT),
        "class": "AC791N build (linker output) -- not a reading from an FM-1",
        "ram": {"data_bss": data_bss, "budget": BSS_BUDGET, "free_in_budget": BSS_BUDGET - data_bss,
                "data": size(".data"), "bss": size(".bss")},
        "pool": {"used": pool_used, "window": pool_win, "stacks": size(".c1_stacks"),
                 "spare": spare, "floor": POOL_SPARE_MIN},
        "lowbuf": {"used": size(".lowbuf"), "max": LOWBUF_MAX},
        "flash": {"xip_text": xip, "app_slot": slot, "free": (slot - xip) if slot else None},
        "c1_stacks": size(".c1_stacks"),
        "top_objects": [{"name": s["name"], "bytes": s["size"], "section": s["section"]} for s in top],
        "top_xip_objects": [{"name": s["name"], "bytes": s["size"]} for s in top_xip],
    }

    print("resource map: %s" % report["class"])
    print("  RAM   .data %6d B  .bss %6d B  = %6d of %6d B  (%6d free, %4.1f %%)"
          % (data_bss - size(".bss"), size(".bss"), data_bss, BSS_BUDGET, BSS_BUDGET - data_bss,
             100.0 * data_bss / BSS_BUDGET))
    if spare is not None:
        print("  pool  used %7d B  spare %6d B  (floor %d)" % (report["pool"]["used"], spare, POOL_SPARE_MIN))
    print("  low   .lowbuf %6d of %6d B  (%5.1f %%)" % (size(".lowbuf"), LOWBUF_MAX,
                                                       100.0 * size(".lowbuf") / LOWBUF_MAX))
    print("  XIP   .text %7d B  of an app slot of %s B  (%s free)"
          % (xip, slot, (slot - xip) if slot else "?"))
    print("  stacks .c1_stacks %d B" % size(".c1_stacks"))
    print("  largest objects in RAM:")
    for s in top[:8]:
        print("    %7d  %-9s %s" % (s["size"], s["section"], s["name"]))
    print("  largest objects in flash (.text: code and constants):")
    for s in top_xip[:8]:
        print("    %7d  %s" % (s["size"], s["name"]))

    fails = []
    if data_bss > BSS_BUDGET:
        fails.append(".data+.bss %d > %d" % (data_bss, BSS_BUDGET))
    if spare is not None and spare < POOL_SPARE_MIN:
        fails.append("pool spare %d < %d" % (spare, POOL_SPARE_MIN))
    if size(".lowbuf") > LOWBUF_MAX:
        fails.append(".lowbuf %d > %d" % (size(".lowbuf"), LOWBUF_MAX))

    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1, sort_keys=True)
        print("  json: %s" % os.path.relpath(a.json, ROOT))
    if a.check:
        if fails:
            for f in fails:
                print("resource_map: FAIL " + f)
            return 1
        print("resource_map: ok -- RAM, pool and .lowbuf inside their budgets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
