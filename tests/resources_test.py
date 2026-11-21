#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""(ETAPA 1) The resource map has a ceiling, and the one optimization this stage made is pinned down.

Two things are checked here, both from `build/felucca.elf` (a build artefact, so the test skips cleanly
wherever the target has not been built):

  1. `tools/resource_map.py --check` passes: .data+.bss inside 96 KiB, the pool's headroom at or above
     8 KiB, .lowbuf inside its window. That is the regression detector for memory, and it runs on the
     numbers the linker wrote, not on an estimate.
  2. The FM6 exp2 curve is stored the way ETAPA 1 left it: 2046 B of second differences in .text and the
     1025 decoded points in .bss. A change that puts the 4100-byte table back in flash costs the app slot
     2 KiB and would silently undo the widest margin this firmware has had -- so it fails here instead.

  python3 tests/resources_test.py            (skips without build/felucca.elf)
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(ROOT, "build", "felucca.elf")
fails = []


def check(what, ok, detail=""):
    print(("%s  %s" % ("ok  " if ok else "FAIL", what)) + (("  " + detail) if detail else ""))
    if not ok:
        fails.append(what)


def main():
    if not os.path.exists(ELF):
        print("resources: no build/felucca.elf here, skipped (a host-only checkout)")
        return 0

    out = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "resource_map.py"),
                          "--check", "--json", os.path.join(ROOT, "build", "resources.json")],
                         capture_output=True, text=True)
    check("the resource map passes its ceilings (96 KiB RAM, 8 KiB pool spare, .lowbuf window)",
          out.returncode == 0, out.stdout.strip().splitlines()[-1] if out.stdout.strip() else out.stderr.strip())

    rp = os.path.join(ROOT, "build", "resources.json")
    rep = json.load(open(rp, encoding="utf-8")) if os.path.exists(rp) else {}
    ram = rep.get("ram", {})
    pool = rep.get("pool", {})
    flash = rep.get("flash", {})
    check("the map reports the RAM inside its budget (%s of %s B)" % (ram.get("data_bss"), ram.get("budget")),
          isinstance(ram.get("data_bss"), int) and ram["data_bss"] <= 96 * 1024)
    check("the map reports the pool headroom (%s B, floor %s)" % (pool.get("spare"), pool.get("floor")),
          isinstance(pool.get("spare"), int) and pool["spare"] >= 8 * 1024)
    check("the map reads the app slot from web/fm1pkg.js (%s B)" % flash.get("app_slot"),
          flash.get("app_slot") == 0x8E7BC)

    # the exp2 storage, by symbol: 2046 B of int16 differences in .text, 4100 B of points in .bss
    readelf = None
    for cand in ("readelf", os.path.join(os.environ.get("JIELI_TOOLCHAIN", ""), "bin", "pi32v2-linux-readelf")):
        try:
            subprocess.run([cand, "--version"], capture_output=True, check=True)
            readelf = cand
            break
        except Exception:
            continue
    syms = subprocess.run([readelf or "readelf", "-sW", ELF], capture_output=True, text=True).stdout
    secs = subprocess.run([readelf or "readelf", "-SW", ELF], capture_output=True, text=True).stdout
    by_num = {}
    for ln in secs.splitlines():
        m = ln.split()
        if len(m) >= 7 and m[0] == "[":
            pass
    # readelf -SW prints "[ 6] .bss  NOBITS ..."; the index carries the brackets only when < 10
    for ln in secs.splitlines():
        parts = ln.replace("[", " ").replace("]", " ").split()
        if len(parts) >= 3 and parts[0].isdigit() and parts[1].startswith("."):
            by_num[int(parts[0])] = parts[1]

    def sym(name):
        for ln in syms.splitlines():
            p = ln.split()
            if len(p) >= 8 and p[-1] == name:
                try:
                    return int(p[2]), by_num.get(int(p[6]), p[6])
                except ValueError:
                    return None, None
        return None, None

    size, sec = sym("FM6_EXP2_D2")
    check("the exp2 differences are 2046 B of .text (1023 int16 + the two scalars)",
          size == 2046 and sec == ".text", "%s B in %s" % (size, sec))
    size, sec = sym("fm6_exp2_tab")
    check("the decoded curve is 4100 B in .bss (1025 points, freed from the app slot)",
          size == 4100 and sec == ".bss", "%s B in %s" % (size, sec))
    size, sec = sym("FM6_EXP2")
    check("the old 4100-byte table is gone from flash", size is None,
          "FM6_EXP2 is back in %s (%s B)" % (sec, size) if size else "")
    size, sec = sym("SINE_D2")
    check("the sine is 1023 B of int8 differences in .text (was 2050 B)",
          size == 1023 and sec == ".text", "%s B in %s" % (size, sec))
    size, sec = sym("SINE")
    check("the decoded sine is 2050 B in .bss, where every caller reads it",
          size == 2050 and sec == ".bss", "%s B in %s" % (size, sec))

    windows = rep.get("regions", {})
    for name, floor in (("XIP", 2048), ("RAM", 4096), ("POOL", 8192), ("LOWBUF", 1024), ("NOINIT", 64)):
        w = windows.get(name, {})
        check("window %s is above its floor (%s B free, floor %s)" % (name, w.get("free"), floor),
              isinstance(w.get("free"), int) and w["free"] >= floor)

    print(fails and ("RESOURCES TEST FAILED (%d)" % len(fails)) or "resources: %d checks, 0 FAILED" % 13)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
