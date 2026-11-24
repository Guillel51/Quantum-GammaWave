#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""One reproducible benchmark of everything this repository can measure without the unit.

    tools/dsp_bench.py --record          # write tests/bench_baseline.json from this tree
    tools/dsp_bench.py                   # measure and print the table
    tools/dsp_bench.py --compare         # measure, diff against the baseline, exit 1 on a regression

What it measures and the class of each number (AGENTS.md §4):

  host/*      tests/regress.c on this host, cc -O2: CPU per sample of the mix scenes, and the golden
              renders' verdict. Host x86-64 -- NOT the AC791N and not an FM-1.
  isr/*       tests/isr_bench.c: ns per TIMER5 tick, host.
  frame/*     tests/frame_bench.c: ns per main-loop frame and per autosave signature, host.
  target/*    tests/target_budget.py over build/felucca.dis: instructions in the render loops of the
              pi32v2 build, and the four memory windows of build/felucca.map. This is the one column
              that is about the target, and it is a static instruction count, not a cycle count.
  flash/*     the image the linker produced.

The point of recording it is A/B: a change that claims to save CPU, RAM or flash shows up as a number
in this table, and a change that claims to be invisible keeps host/* and the golden renders identical.
Blocks the environment cannot measure (no JieLi toolchain, no disassembly, no valgrind) are reported as
`null` and named, never filled with a guess.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / "tests" / "bench_baseline.json"
TOL = 0.25           # host CPU: the suite's own +25 % rule for a noisy shared machine
TOL_TARGET = 0.03    # target instructions: the suite's own +3 % rule


def run(cmd, cwd=ROOT):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def rel(p):
    return str(Path(p).relative_to(ROOT)) if Path(p).is_absolute() else str(p)


def need(binary):
    return (ROOT / binary).exists()


def host_regress():
    """CPU per sample of the mix scenes, host x86-64 (tests/regress.c)."""
    if not need("build/regress"):
        return None
    # The host is shared, and one sample of a scene's median moves by tens of percent between runs
    # with no code change at all (measured: idle_click 76.5 -> 118.8 ns over two runs of the same
    # binary). regress already keeps the smallest window it sees; this keeps the smallest RUN of three,
    # which is the least noisy estimator of the work a scene does.
    cmd = ["build/regress", "tests/golden.txt", "tests/cpu_baseline.txt"]
    samples = {}
    rc, out = run(cmd)
    for _ in range(2):
        rc2, out2 = run(cmd)
        out += "\n" + out2
        rc = rc or rc2
    for m in re.finditer(r"regress:\s+(?:cpu\s+)?(\S+)\s+(\d+) instr\s+([0-9.]+) ns", out):
        v = float(m.group(3))
        if m.group(1) not in samples or v < samples[m.group(1)]:
            samples[m.group(1)] = v
    # the mix scenes are the ones that say what the whole instrument costs
    mix = {}
    for key in ("idle_drums", "3parts_full_drums", "6parts_full", "idle_click"):
        if key in samples:
            mix[key] = samples[key]
    gold = re.search(r"(\d+) golden renders \((\d+) changed", out)
    return {
        "mix_ns_per_sample": mix,
        "golden_renders": int(gold.group(1)) if gold else None,
        "golden_changed": int(gold.group(2)) if gold else None,
        "exit": rc,
    }


def isr_bench():
    if not need("build/isr_bench"):
        return None
    rc, out = run(["build/isr_bench", "60000"])
    ticks = {}
    for m in re.finditer(r"scene \d+ (\S+).*?\s([0-9.]+) ns CPU / tick", out):
        ticks[m.group(1)] = float(m.group(2))
    return {"ns_per_tick": ticks, "exit": rc}


def frame_bench():
    if not need("build/frame_bench"):
        return None
    rc, out = run(["build/frame_bench", "600"])
    got = {}
    for m in re.finditer(r"^\s+(\S+)\s+.*?\s([0-9.]+) ns CPU / call", out, re.M):
        got[m.group(1)] = float(m.group(2))
    keep = {}
    for k in ("frame", "draw", "input", "sig", "poll"):
        if k in got:
            keep[k] = got[k]
    return {"ns_per_call": keep, "exit": rc}


def target():
    """Static instruction counts of the pi32v2 build plus the memory windows."""
    out = {"paths": {}, "image_bytes": None}
    if need("build/felucca.dis"):
        rc, txt = run([sys.executable, "tests/target_budget.py", "build/felucca.dis",
                       "tests/target_budget.txt"])
        for m in re.finditer(r"^target: (DRUM_PATH|BLOCK_PATH)\s+.*?cost\s+(\d+)", txt, re.M):
            out["paths"][m.group(1)] = int(m.group(2))
        for m in re.finditer(r"^target: (\w+)\s+(\d+) instructions,.*?, (\d+) divides", txt, re.M):
            out.setdefault("functions", {})[m.group(1)] = {
                "instructions": int(m.group(2)), "divides": int(m.group(3))}
        out["exit"] = rc
    if need("build/felucca.map"):
        rc, txt = run([sys.executable, "tools/mem_budget.py", "--show"])
        # window    length   used    free   floor
        for m in re.finditer(r"^\s{2}(\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", txt, re.M):
            out.setdefault("windows", {})[m.group(1)] = {
                "used": int(m.group(3)), "free": int(m.group(4)), "size": int(m.group(2))}
        out["mem_exit"] = rc
    if need("build/felucca.bin"):
        out["image_bytes"] = (ROOT / "build/felucca.bin").stat().st_size
    return out


def stack():
    """Host stack high-water mark of the heaviest scenes (tests/stack_watermark.c)."""
    if not need("build/stack_watermark"):
        return None
    rc, out = run(["build/stack_watermark"])
    got = {}
    for m in re.finditer(r"stack:\s+(.+?)\s+(\d+) B of the \d+ B painted", out):
        got[m.group(1).strip()] = int(m.group(2))
    return {"high_water_bytes": got, "exit": rc}


def collect():
    return {
        "host": host_regress(),
        "isr": isr_bench(),
        "frame": frame_bench(),
        "stack": stack(),
        "target": target(),
    }


def flat(d, pre=""):
    if isinstance(d, dict):
        for k, v in d.items():
            if k in ("exit", "mem_exit"):
                continue
            yield from flat(v, pre + k + "/")
    elif d is not None:
        yield pre[:-1], d


def compare(now, base):
    a, b = dict(flat(now)), dict(flat(base))
    bad = []
    for k, new in a.items():
        if k not in b:
            print(f"  new      {k:44s} {new}")
            continue
        old = b[k]
        if not isinstance(new, (int, float)) or not isinstance(old, (int, float)):
            continue
        if k.startswith("host/") or k.startswith("isr/") or k.startswith("frame/"):
            tol, why = TOL, "host CPU, +25 % allowed (the suite's own rule)"
        elif k.startswith("target/"):
            tol, why = TOL_TARGET, "target instructions, +3 % allowed"
        else:
            tol, why = 0.0, "must not grow"
        if old == 0:
            continue
        d = (new - old) / abs(old)
        flag = "ok  " if d <= tol else "FAIL"
        if d > tol:
            bad.append(f"{k}: {old} -> {new} ({d * 100:+.1f} %, {why})")
        print(f"  {flag}     {k:44s} {old:>10} -> {new:>10}  ({d * 100:+.1f} %)")
    for k in b:
        if k not in a:
            print(f"  gone     {k}")
    return bad


def main(argv):
    if "--record" in argv:
        data = collect()
        BASE.write_text(json.dumps(data, indent=1, sort_keys=True) + "\n")
        for k, v in flat(data):
            print(f"  {k:44s} {v}")
        print(f"dsp_bench: recorded {rel(BASE)}")
        return 0
    data = collect()
    if "--compare" in argv:
        if not BASE.exists():
            print("dsp_bench: no baseline; run --record first", file=sys.stderr)
            return 2
        base = json.loads(BASE.read_text())
        print(f"dsp_bench: {rel(BASE)} vs this tree ({rel(BASE.parent.parent)})")
        bad = compare(data, base)
        if bad:
            print("dsp_bench: REGRESSIONS")
            for line in bad:
                print("  " + line)
            return 1
        # a golden render that changed is not a CPU argument, it is a different instrument
        g = (data.get("host") or {}).get("golden_changed")
        if g:
            print(f"dsp_bench: {g} golden renders CHANGED -- not an invisible optimisation")
            return 1
        print("dsp_bench: ok -- no regression against the baseline")
        return 0
    for k, v in flat(data):
        print(f"  {k:44s} {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
