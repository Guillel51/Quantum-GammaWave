#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Static budget report for QG-1 (host build; no hardware claims).

Compiles a sizeof probe of the real headers for NVOICE 8, 10 and 12, counts the browsable factory
library from the preset tables, and points at the CPU artefacts. Every number is labelled: static RAM
is exact for the host ABI (the target's pointers are 32-bit, so the report also prints a pointer-free
lower bound); CPU ns come from the host and are not AC791N measurements.
"""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware/src"
GEN = ROOT / "build/gen"
ENGINES = [("ANALOG", "eng_analog.c"), ("PHASE", "eng_phase.c"), ("LOFI", "eng_lofi.c"),
           ("FORMANT", "eng_formant.c"), ("WHEEL", "eng_wheel.c"), ("DRUM", "eng_drum.c"),
           ("NOISE", "eng_noise.c"), ("FM6", "eng_fm6.c")]
PROBE = r'''
#include <stdio.h>
#include "core.h"
int main(void) {
    printf("%zu %zu %zu %zu\n", sizeof(voice_t), sizeof(track_t), sizeof(preset_t), sizeof(track_t) * NTRK);
    return 0;
}
'''


def sizes(nvoice):
    with tempfile.TemporaryDirectory() as d:
        c = Path(d) / "probe.c"
        exe = Path(d) / "probe"
        c.write_text(PROBE)
        subprocess.run(["cc", "-w", "-DNVOICE=%d" % nvoice, "-I" + str(GEN), "-I" + str(SRC),
                        "-o", str(exe), str(c)], check=True)
        out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout.split()
        return [int(x) for x in out]


def library():
    counts, legacy = {}, 0
    for name, file in ENGINES:
        s = (SRC / file).read_text()
        m = re.search(r"static const preset_t \w+PRESETS\[\] = \{", s)
        i = s.index("{", m.start())
        depth, quote, esc = 0, False, False
        end = None
        for j in range(i, len(s)):
            c = s[j]
            if quote:
                if esc:
                    esc = False
                elif c == "\\":
                    esc = True
                elif c == '"':
                    quote = False
                continue
            if c == '"':
                quote = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    end = j
                    break
        if end is None:
            raise SystemExit("%s: unbalanced %s" % (file, name))
        body = s[i + 1:end]
        names = re.findall(r'^\s*\{\s*"([^"]+)"', body, re.M)
        leg = len(re.findall(r"\.legacy\s*=\s*1", body))
        counts[name] = len(names) - leg
        legacy += leg
    return counts, legacy


def costs():
    """Host instructions per sample per engine, from tests/cpu_baseline.txt (NOT AC791N)."""
    path = ROOT / "tests/cpu_baseline.txt"
    per = {}
    if not path.exists():
        return {}
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, _, val = line.partition(" ")
        parts = name.split("/")
        if len(parts) != 3 or parts[0] != "cpu":
            continue
        try:
            v = int(val)
        except ValueError:
            continue
        per.setdefault(parts[1], []).append((v, parts[2]))
    out = {}
    for eng, vals in per.items():
        vals.sort()
        out[eng] = {"count": len(vals), "max": vals[-1][0], "max_name": vals[-1][1],
                    "avg": sum(v for v, _ in vals) // len(vals)}
    return out


def main():
    if not (GEN / "felucca_tables.h").exists():
        print("budget: run ./build.sh once (build/gen missing)", file=sys.stderr)
        return 2
    rows = []
    for n in (8, 10, 12):
        voice, track, preset, per_track = sizes(n)
        rows.append({"voices": n, "voice_t": voice, "track_t": track, "preset_t": preset,
                     "six_tracks": voice * 6 * n})
    counts, legacy = library()
    report = {
        "library": {"browsable": sum(counts.values()), "legacy_drum": legacy, "per_engine": counts},
        "voices": rows,
        "notes": [
            "static sizes: host ABI (64-bit pointers); the target uses 32-bit pointers, so voice_t and track_t are smaller there",
            "voices RAM = NVOICE x sizeof(voice_t) x 6 tracks, outside the audio pool",
            "CPU ns/instructions must come from the JieLi toolchain or an FM-1; tests/cpu_baseline.txt is host data",
        ],
    }
    if "--json" in sys.argv:
        print(json.dumps(report, indent=2))
        return 0
    print("QG-1 static budget (host build; NOT an AC791N measurement)")
    print("library: %u browsable sounds, %u legacy DRUM entries (indices 10/11)"
          % (report["library"]["browsable"], legacy))
    print("  " + ", ".join("%s %u" % (k, v) for k, v in counts.items()))
    print("voices  voice_t  six_tracks   vs 10")
    base = rows[1]["six_tracks"]
    for r in rows:
        print("%6d  %7d  %10d  %+6d" % (r["voices"], r["voice_t"], r["six_tracks"],
                                        r["six_tracks"] - base))
    print("RAM per voice is exact for this ABI; the target pointer size makes it smaller, never larger.")
    c = costs()
    if c:
        print("host cost per engine (instructions/sample, tests/cpu_baseline.txt; not AC791N):")
        for eng, v in sorted(c.items(), key=lambda kv: -kv[1]["max"]):
            print("  %-8s max %6d (%s), avg %6d, %2d baselines" % (eng, v["max"], v["max_name"], v["avg"], v["count"]))
    fw = ROOT / "build/felucca.fwsc"
    if fw.exists():
        print("package: build/felucca.fwsc %.0f KiB (host build; the target image differs)" % (fw.stat().st_size / 1024.0))
    print("CPU: run tests/regress.c for host ns, or the JieLi build + board for instructions/sample.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
