#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Check the built ELF against the documented flash/SRAM map.

This is a static, reproducible test: it reads the symbols and sections of
build/felucca.elf, not a physical board.  It fails on any overlap or on a
region that leaves its documented window.  Run it after tools/build.py.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ELF = ROOT / "build" / "felucca.elf"
APP = ROOT / "build" / "felucca.bin"
PKG = ROOT / "build" / "felucca.fwsc"
LD = ROOT / "firmware" / "app.ld"
MAKE = ROOT / "tools" / "fm1pkg_make.py"

FLASH_SIZE = 0x100000
APP_AREA_LO = 0x4000
APP_AREA_HI = 0x93000
RAM_LO = 0x01C08000
RAM_HI = 0x01C20000
POOL_HI = 0x01C70000
RAMTEXT_HI = 0x01C76800
STACK_TOP = 0x01C7C000
RESERVED_LO = 0x01C7FD50
LOWBUF_LO = 0x01C00000
LOWBUF_HI = 0x01C07000


def objdump():
    root = Path(os.environ.get("JIELI_TOOLCHAIN", "/tmp/jieli/toolchain"))
    tool = root / "common" / "bin" / "objdump"
    if not tool.exists():
        return None
    return str(tool)


def symbols(tool):
    out = subprocess.check_output([tool, "-t", str(ELF)], text=True)
    syms = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and re.fullmatch(r"[0-9a-fA-F]{8}", p[0]):
            try:
                syms[p[-1]] = int(p[0], 16)
            except ValueError:
                pass
    return syms


def sections(tool):
    out = subprocess.check_output([tool, "-h", str(ELF)], text=True)
    secs = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and re.fullmatch(r"[0-9a-fA-F]+", p[2]) and p[1].startswith("."):
            secs[p[1]] = (int(p[3], 16), int(p[2], 16))
    return secs


def py_const(name):
    text = MAKE.read_text()
    m = re.search(rf"^{name}\s*=\s*(0x[0-9a-fA-F]+|\d+)", text, re.M)
    if not m:
        raise SystemExit(f"{MAKE}: no {name}")
    return int(m.group(1), 0)


def main():
    tool = objdump()
    if tool is None or not ELF.exists():
        print("memory map: skipped (no build/felucca.elf or JieLi objdump)")
        return 0
    sym = symbols(tool)
    sec = sections(tool)
    need = ["_data_start", "_data_end", "_bss_start", "_bss_end", "_low_start", "_low_end",
            "_pool_start", "_pool_end", "_c1_stack_lo", "_c1_stack_hi", "_rt_start", "_hot_end",
            "_guard0", "_ustack_lo", "_ustack_top", "_guard1", "_sstack_lo", "_sstack_top"]
    missing = [n for n in need if n not in sym]
    if missing:
        print("memory map: FAIL, missing symbols: " + ", ".join(missing))
        return 1

    bad = []
    def ck(name, ok, detail=""):
        print(f"{name:<62} {'ok' if ok else 'FAIL'} {detail}".rstrip())
        if not ok:
            bad.append(name)

    # Flash: 1 MiB part, app area and package end below the SDK regions.
    app_slot = py_const("APP_SLOT")
    flash_img = py_const("FLASH_SIZE")
    ck("flash is the 1 MiB part", FLASH_SIZE == 0x100000)
    ck("app slot inside the app area", APP_AREA_LO + 0x120 + app_slot <= APP_AREA_HI,
       f"{APP_AREA_LO + 0x120 + app_slot:#x} <= {APP_AREA_HI:#x}")
    ck("package image ends before the VM", flash_img <= 0x93000, f"{flash_img:#x}")
    ck("VM descriptor ends at 0xE9000", flash_img + 0x56000 == 0xE9000)
    app_size = APP.stat().st_size if APP.exists() else None
    pkg_size = PKG.stat().st_size if PKG.exists() else None
    ck("app.bin fits the app slot", app_size is None or app_size <= app_slot,
       f"{app_size} <= {app_slot}")
    ck("package is exactly the 0x93000 image plus wrapper", pkg_size is None or pkg_size >= flash_img,
       f"{pkg_size} B")

    # SRAM: .data/.bss, pool, core-1 stacks, RAM code, guards and stacks.
    ck("LOWBUF starts at its origin", sym["_low_start"] == LOWBUF_LO)
    ck("LOWBUF inside its window", sym["_low_end"] <= LOWBUF_HI,
       f"end {sym['_low_end']:#x} <= {LOWBUF_HI:#x}")
    ck("RAM starts at its origin", sym["_data_start"] == RAM_LO)
    ck(".data before .bss", sym["_data_end"] <= sym["_bss_start"])
    ck(".bss inside RAM", sym["_bss_end"] <= RAM_HI, f"end {sym['_bss_end']:#x} <= {RAM_HI:#x}")
    ck(".data+.bss <= 96 KiB", sym["_bss_end"] - sym["_data_start"] <= 96 * 1024,
       f"{sym['_bss_end'] - sym['_data_start']} B")
    ck("pool starts after RAM", sym["_pool_start"] >= RAM_HI)
    ck("pool before core-1 stacks", sym["_pool_end"] <= sym["_c1_stack_lo"],
       f"{sym['_pool_end']:#x} <= {sym['_c1_stack_lo']:#x}")
    ck("core-1 stacks are 7168 B", sym["_c1_stack_hi"] - sym["_c1_stack_lo"] == 7168)
    ck("core-1 stacks below the pool top", sym["_c1_stack_hi"] <= POOL_HI,
       f"end {sym['_c1_stack_hi']:#x} <= {POOL_HI:#x}")
    ck("RAMTEXT starts above the pool", sym["_rt_start"] >= POOL_HI)
    ck("hot code ends before guard0", sym["_hot_end"] <= sym["_guard0"],
       f"{sym['_hot_end']:#x} <= {sym['_guard0']:#x}")
    ck("RAMTEXT window is not overrun", sym["_hot_end"] <= RAMTEXT_HI)
    ck("guard0 / user stack / guard1", sym["_guard0"] == 0x01C76800 and
       sym["_ustack_lo"] == 0x01C76900 and sym["_ustack_top"] == 0x01C7A000 and
       sym["_guard1"] == 0x01C7A000)
    ck("system stack inside the top window", sym["_sstack_lo"] == 0x01C7A100 and
       sym["_sstack_top"] == STACK_TOP)
    noinit = sec.get(".noinit")
    ck(".noinit inside NOINIT", bool(noinit) and noinit[0] + noinit[1] <= RESERVED_LO,
       f"{noinit[0]:#x}+{noinit[1]:#x}" if noinit else "missing")
    ck("NOINIT does not reach the reserved area", noinit[0] + noinit[1] <= RESERVED_LO)

    # The exact headrooms, printed so a future change is visible in CI logs.
    print(f"memory map: RAM spare {RAM_HI - sym['_bss_end']} B; pool spare {POOL_HI - sym['_c1_stack_hi']} B; "
          f"RAMTEXT spare {sym['_guard0'] - sym['_hot_end']} B; NOINIT spare {RESERVED_LO - (noinit[0] + noinit[1])} B")
    if bad:
        print("memory map: FAIL (" + ", ".join(bad) + ")")
        return 1
    print("memory map: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
