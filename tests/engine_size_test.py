#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/engine_size.py, checked without the AC791N toolchain.

The measurement itself runs on a real build (`./build.sh` writes build/felucca.syms and
build/felucca.map and prints the table). This test exercises the part that can be wrong on any host:
which symbols belong to which engine, what counts as XIP and what as RAM, the map cross-check, the
budget exit code and the JSON shape.

The fixture is built from the real `firmware/src/eng_*.c`, so a renamed or moved engine symbol breaks
this test on the host, before anyone waits for a toolchain.
"""
import io
import contextlib
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import engine_size as E                                    # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print("%-78s %s" % (what, "ok" if ok else "FAIL"))
    bad += not ok


def sym_line(addr, section, size, name):
    return "%08x l     F %-10s %08x %s\n" % (addr, section, size, name)


def run(*argv):
    out, err = io.StringIO(), io.StringIO()
    code = 0
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = E.main(["engine_size.py", *map(str, argv)])
        except SystemExit as e:                            # --map failures and skips exit this way
            code = e.code or 0
            if isinstance(code, str):                      # a message: the process would print it and exit 1
                out.write(code + "\n")
                code = 1
    return code, out.getvalue() + err.getvalue()


def main():
    engines = E.engine_sources()
    names = [n for n, _f, _s in engines]
    check("every engine source is found, with its own name", len(engines) >= 10 and "ADDTV" in names)
    add = dict((n, s) for n, _f, s in engines)
    # (FASE 1/8) ad_v is the slot arena now: ad_slot() and ad_vc_t, not a static voice array
    check("ADDTV declares the symbols the engine is made of",
          {"add_render", "add_block", "add_note_on", "AD_PROF", "AD_PRESETS", "ENG_ADDITIVE",
           "ad_t", "ad_slot"} <= add["ADDTV"])
    check("no symbol is claimed by two engines",
          sum(len(s) for s in add.values()) == len(set().union(*add.values())))
    check("an engine's set carries no header static (trk, song, TSEL)",
          not ({"trk", "song", "TSEL"} & add["ADDTV"]))

    # one symbol of every engine, plus one nobody claims: sizes are ours, so the sums are predictable
    lines, want, pairs = [], {}, []
    i = 0
    for name, _f, syms in engines:
        for sym in sorted(syms)[:3]:
            size = 0x10 + 2 * i
            lines.append(sym_line(0x02000000 + 0x100 * i, ".text", size, sym).rstrip("\n"))
            want[name] = want.get(name, 0) + size
            pairs.append((sym, 0x02000000 + 0x100 * i, size))
            i += 1
        for sym in sorted(syms)[:1]:                       # one in .bss, to exercise the RAM column
            size = 0x40
            lines.append(sym_line(0x01C08000 + 0x100 * i, ".bss", size, sym).rstrip("\n"))
            want[name + "/bss"] = want.get(name + "/bss", 0) + size
            i += 1
    lines.append(sym_line(0x0200F000, ".text", 0x2000, "ui_widget_nobody_claims").rstrip("\n"))
    lines.append(sym_line(0x01C0F000, ".data", 0x123, "ui_state_too").rstrip("\n"))
    lines.append("%08x l       .text\t\t 00000000 .text" % 0x02000000)      # a section symbol: skipped
    text = "\n".join(lines) + "\n"
    parsed = E.parse_syms(text)
    check("the symbol table parser reads address, section, size and name",
          len(parsed) == len(lines) - 1 and parsed[0].name in add["ADDTV"] | add["WHEEL"] | add["FM6"])

    per, rest = E.attribute(parsed, engines)
    ok = all(per[n]["xip"] == want[n] and per[n]["bss"] == want[n + "/bss"] for n in names)
    check("each engine gets exactly its own bytes, XIP and .bss", ok)
    check("a symbol no engine declares stays unattributed",
          rest["xip"] == 0x2000 and rest["data"] == 0x123 and rest["bss"] == 0)
    check("code copied into RAM (.ram_text) is not counted as XIP",
          E.kind(".ram_text") == "ram" and E.kind(".text") == "xip" and E.kind(".bss") == "bss")

    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        syms = d / "fixture.syms"
        syms.write_text(text)
        code, out = run(syms)
        check("the report exits 0 on a build inside the budget", code == 0)
        check("the report names every engine and the class of the measurement",
              all(n in out for n in names) and "AC791N build" in out)
        code, out = run(syms, "--json")
        check("--json carries the totals and each engine", code == 0 and '"totals"' in out)

        # the map cross-check: one section per function, as the link writes them
        mp = d / "fixture.map"

        def write_map(warp=0):
            # one input section per function, as the link writes them; warp moves the first one's size
            mp.write_text("".join(" .text.%s\n                0x%08x       0x%x build/felucca.o\n"
                                  % (sym, addr, size + (warp if i == 0 else 0))
                                  for i, (sym, addr, size) in enumerate(pairs)))

        write_map()
        code, out = run(syms, "--map", mp)
        check("the map cross-check passes when both agree", code == 0 and "agree" in out)
        write_map(1)
        code, out = run(syms, "--map", mp)
        check("the map cross-check fails when a function's size differs", code != 0 and "differ" in out)

        big = d / "big.syms"
        # the landmarks the budget lines use: _start, _etext and _bss_end of a build 192 KiB into RAM
        big.write_text(text
                       + sym_line(0x01C10000, ".bss", 0x20000, "ui_widget_nobody_claims2")
                       + sym_line(0x01C38000, ".text", 0x10, "_bss_end")
                       + sym_line(0x02000120, ".text", 0x10, "_start")
                       + sym_line(0x02000200, ".text", 0x10, "_etext"))
        code, out = run(big, "--strict", "--no-image")
        check("--strict exits 1 when .data+.bss leaves the 96 KiB budget", code == 1 and "over budget" in out)

    print("engine_size: %s" % ("all passed" if not bad else "%d FAILED" % bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
