#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Per-engine XIP and .data+.bss, measured on the AC791N build (no manual work, no guessing).

The firmware links as one unity object, so the linker map has no per-engine objects: what it has is
one section (and one symbol) per function. This reads those symbols -- the table `objdump -t` prints,
which `./build.sh` already saves as `build/felucca.syms` -- attributes each one to the engine whose
source defines it (the file-scope functions and arrays of `firmware/src/eng_*.c`), and prints what
each engine costs in flash and in RAM.

    tools/engine_size.py                      # build/felucca.syms (or the ELF, with the toolchain)
    tools/engine_size.py --map build/felucca.map    # also cross-check the functions against the map
    tools/engine_size.py --json               # for CI
    tools/engine_size.py --strict             # exit 1 if the documented budgets are exceeded

What is XIP and what is not, in this linker script: `.text` holds code **and** constants (the map's
`_ecode`/`_etext` split is reported too), `.ram_text`/`.c1_text`/`.hot_text` are code copied into RAM
(counted apart: they are RAM, not flash), `.data` is initialised RAM and `.bss` is RAM.

Every number comes from the artefacts of one build. The class is "AC791N build (pi32v2 clang + ld)",
which is not a measurement on an FM-1: nothing here says anything about a physical unit.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "firmware" / "src"
OUT = ROOT / "build"
SYMS = OUT / "felucca.syms"
ELF = OUT / "felucca.elf"
MAP = OUT / "felucca.map"

APP_XIP = 0x02000120            # app.ld: where the image starts (tools/build.py APP_XIP)
RAM_BUDGET = 96 * 1024          # AGENTS.md: .data+.bss
XIP = (".text",)
RAM_CODE = (".ram_text", ".c1_text", ".hot_text")
DATA = (".data",)
BSS = (".bss",)


def kind(section):
    if section in XIP:
        return "xip"
    if section in RAM_CODE:
        return "ram"
    if section in DATA:
        return "data"
    if section in BSS:
        return "bss"
    return "other"                       # .pool, .noinit, debug: not this report's business


class Sym:
    __slots__ = ("addr", "section", "size", "name")

    def __init__(self, addr, section, size, name):
        self.addr, self.section, self.size, self.name = addr, section, size, name


def parse_syms(text):
    """objdump -t: '<addr> <scope> [F|O] <section> <size> <name>' (the flags column may be empty)."""
    syms = []
    for line in text.splitlines():
        p = line.split()
        if len(p) < 4 or not re.fullmatch(r"[0-9a-fA-F]{8,16}", p[0]):
            continue
        try:
            size = int(p[-2], 16)
        except ValueError:
            continue
        name = p[-1]
        section = next((x for x in p[1:-2] if x.startswith(".")), "")
        if name.startswith("."):
            continue                     # a section symbol, not an object
        syms.append(Sym(int(p[0], 16), section, size, name))
    return syms


def objdump():
    root = Path(os.environ.get("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain")))
    tool = root / "common" / "bin" / "objdump"
    return str(tool) if tool.exists() else None


def symbols(path=None):
    """The symbol table from build/felucca.syms, else straight from the ELF with the toolchain."""
    path = Path(path) if path else SYMS
    if path.exists():
        return parse_syms(path.read_text(errors="replace")), str(path)
    if not ELF.exists():
        return None, None                      # nothing built here: the caller reports the skip
    tool = objdump()
    if not tool:
        raise SystemExit("engine_size: build/felucca.syms missing and no JIELI_TOOLCHAIN objdump "
                         "(run ./build.sh, or tools/get_toolchain.sh)")
    return parse_syms(subprocess.run([tool, "-t", str(ELF)], capture_output=True, text=True,
                                     check=True).stdout), str(ELF)


def strip_c(text):
    """Comments and string/char literals out, so a name inside one is not taken for a definition."""
    out, i, n, mode = [], 0, len(text), None
    while i < n:
        c = text[i]
        if mode is None:
            if text.startswith("/*", i):
                mode, i = "/*", i + 2
                continue
            if text.startswith("//", i):
                mode, i = "//", i + 2
                continue
            if text.startswith("#if 0", i):          # (not used here, but a `#if 0` block is dead too)
                mode, i = "#if0", i + 4
                continue
            if c in "\"'":
                mode, i = c, i + 1
                continue
            out.append(c)
        elif mode == "/*":
            if text.startswith("*/", i):
                mode, i = None, i + 2
                continue
        elif mode == "//":
            if c == "\n":
                mode = None
                out.append(c)
        elif mode == "#if0":
            if text.startswith("#endif", i):
                mode, i = None, i + 6
                continue
        else:
            if c == "\\":
                i += 1
            elif c == mode:
                mode = None
        i += 1
    return "".join(out)


FUNC = re.compile(r"(?m)^(?:static\s+)?(?:const\s+|inline\s+)*[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{})]*\)\s*\{")
DATA_DEF = re.compile(r"(?m)^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*(?:=[^=]|;)")


def definitions(text):
    """The names a file defines at file scope: functions (whatever attributes they carry) and objects."""
    text = re.sub(r"__attribute__\s*\(\([^)]*\)\)", "", text)     # static __attribute__((noinline)) void f(
    return set(FUNC.findall(text)) | set(DATA_DEF.findall(text))


def engine_sources():
    """[(display name, file, {symbols the file defines at file scope})] from firmware/src/eng_*.c."""
    out = []
    for path in sorted(SRC.glob("eng_*.c")):
        raw = path.read_text()
        name = re.search(r'\.name\s*=\s*"([^"]+)"', raw)          # the engine's own name (ADDTV, WHEEL, ...)
        if not name:
            continue
        text = strip_c(raw)                                          # the definitions, without comments or strings
        names = definitions(text)
        out.append((name.group(1), path.name, names))
    return out


def attribute(syms, engines):
    """{engine display name: {kind: bytes}} and the symbols nobody claimed, per kind."""
    owner, seen = {}, {}
    for name, _file, names in engines:
        for s in names:
            owner.setdefault(s, set()).add(name)
    owner = {s: next(iter(v)) for s, v in owner.items() if len(v) == 1}    # ambiguous: unclaimed
    per = {name: {"xip": 0, "data": 0, "bss": 0, "ram": 0, "symbols": 0, "names": []}
           for name, _f, _n in engines}
    rest = {"xip": 0, "data": 0, "bss": 0, "ram": 0, "symbols": 0}
    for s in syms:
        k = kind(s.section)
        if k == "other":
            continue
        e = owner.get(s.name)
        row = per[e] if e else rest
        row[k] += s.size
        row["symbols"] += 1
        if e:
            row["names"].append((s.name, k, s.size))
    return per, rest


def map_sizes(path):
    """{symbol: bytes} from the linker map: the size line after '.text.<symbol>' (functions only)."""
    out, want = {}, None
    for line in Path(path).read_text(errors="replace").splitlines():
        m = re.match(r"^\s+\.(?:text|ram_text|c1_text|hot_text)\.(\w+)\s*$", line)
        if m:
            want = m.group(1)
            continue
        if want:
            mm = re.match(r"^\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+\S+", line)
            if mm:
                out[want] = int(mm.group(1), 16)
            want = None
    return out


def totals(syms):
    """The build's own landmarks, for the budget lines: _start, _ecode/_etext, _bss_end."""
    at = {s.name: s.addr for s in syms}
    t = {k: sum(s.size for s in syms if kind(s.section) == k) for k in ("xip", "data", "bss", "ram")}
    if "_start" in at and "_etext" in at:
        t["xip_from_start"] = at["_etext"] - at["_start"]
    if "_ecode" in at:
        t["xip_code"] = at["_ecode"] - at["_start"]
        t["xip_const"] = at["_etext"] - at["_ecode"]
    if "_bss_end" in at:
        t["ram_region"] = at["_bss_end"] - 0x01C08000
    return t


def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    flags = {a for a in argv[1:] if a.startswith("--")}
    path = Path(args[0]) if args else None
    syms, origin = symbols(path)
    if syms is None:
        print("engine size: skipped (no build/felucca.syms or build/felucca.elf: run ./build.sh)")
        return 0
    engines = engine_sources()
    per, rest = attribute(syms, engines)
    t = totals(syms)

    if "--map" in flags:
        mp = MAP if len(args) < 2 else Path(args[1])
        if not mp.exists():
            raise SystemExit("engine_size: %s missing (the link writes it with -Map)" % mp)
        sizes = map_sizes(mp)
        checked = bad = 0
        for name, _f, _n in engines:
            for sym, k, size in per[name]["names"]:
                if k == "xip" and sym in sizes:
                    checked += 1
                    bad += sizes[sym] != size
        if not checked:
            raise SystemExit("engine_size: the map has no per-function sections to cross-check")
        if bad:
            raise SystemExit("engine_size: %d of %d engine functions differ between the map and the "
                             "symbol table" % (bad, checked))
        print("map: %d engine functions agree with the map (%s)" % (checked, mp))

    rows = sorted(per.items(), key=lambda kv: -(kv[1]["xip"] + kv[1]["bss"]))
    if "--json" in flags:
        print(json.dumps({"origin": origin, "totals": t,
                          "engines": {n: {k: v for k, v in r.items() if k != "names"} for n, r in rows},
                          "unattributed": rest}, indent=2))
        return 0

    print("QG-1 engine size, from %s" % origin)
    print("class: AC791N build (pi32v2 clang + ld) -- not an FM-1 measurement")
    print("%-8s %8s %8s %8s %8s   %s" % ("engine", "XIP", ".data", ".bss", "symbols", "biggest"))
    for name, r in rows:
        big = max(r["names"], key=lambda x: x[2]) if r["names"] else ("-", "", 0)
        print("%-8s %8d %8d %8d %8d   %s %d B" % (name, r["xip"], r["data"], r["bss"], r["symbols"],
                                                  big[0], big[2]))
    print("%-8s %8d %8d %8d %8d" % ("(rest)", rest["xip"], rest["data"], rest["bss"], rest["symbols"]))
    print("%-8s %8d %8d %8d" % ("total", t["xip"], t["data"], t["bss"]))
    if "xip_code" in t:
        print("XIP: code %d B + constants %d B = %d B from %#x" %
              (t["xip_code"], t["xip_const"], t["xip_from_start"], APP_XIP))
    print("RAM: .data+.bss %d B of %d B (%d B free)%s" %
          (t.get("ram_region", t["data"] + t["bss"]), RAM_BUDGET,
           RAM_BUDGET - t.get("ram_region", t["data"] + t["bss"]),
           "" if "ram_region" not in t else " [region, from _bss_end]"))
    hot = [n for n, r in rows if r["ram"]]
    if hot:
        print("RAM code (not XIP): " + ", ".join("%s %d B" % (n, per[n]["ram"]) for n in hot))

    over = []
    ram = t.get("ram_region", t["data"] + t["bss"])
    if ram > RAM_BUDGET:
        over.append("RAM .data+.bss %d B over the %d B budget" % (ram, RAM_BUDGET))
    img = OUT / "felucca.bin"
    if img.exists() and "--no-image" not in flags:
        import fm1pkg_make                                  # APP_SLOT: the image's region (tools/)
        if img.stat().st_size > fm1pkg_make.APP_SLOT:
            over.append("image %d B over the app slot %d B" % (img.stat().st_size, fm1pkg_make.APP_SLOT))
        else:
            print("image: %d B of %d B (%d B free in the app slot)" %
                  (img.stat().st_size, fm1pkg_make.APP_SLOT, fm1pkg_make.APP_SLOT - img.stat().st_size))
    for line in over:
        print("over budget: " + line)
    if over and "--strict" in flags:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
