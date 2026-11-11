#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Is build/emu the emulator of this tree?

`web/emu/build.sh` writes build/emu/manifest.json with the commit, the emcc version and the flags the WASM was
compiled with, and `web/test_provenance.mjs` refuses to build a site whose emulator and firmware name different
commits. That refusal is right, and it costs half an hour to read when the cause is simply "I rebuilt the firmware
and not the emulator". This says it in one line, before the suite gets there.

    python3 tools/emu_stamp.py            # the verdict, exit 1 when the emulator is not this tree's
    python3 tools/emu_stamp.py --quiet    # exit code only

States, and they are not the same thing:

  ok        the emulator, the firmware manifest and HEAD all name one commit
  stale     the emulator was built from another commit (rebuild it: `sh web/emu/build.sh`)
  missing   there is no build/emu (a checkout that never built one: the web checks skip themselves)
  unversioned  build/emu/manifest.json has no commit field (an emulator older than the field)
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def head():
    try:
        out = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True)
        return out.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def read(path):
    try:
        with open(os.path.join(ROOT, path), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def short(sha):
    return (sha or "?")[:7]


def same(a, b):
    """Two ids for one commit: build/emu writes 12 characters, HEAD and the firmware manifest carry 40.
    Comparing them with != says the same commit is different, which reads as "the emulator is stale" over
    and over for an emulator that is exactly this tree's. The shorter one must be a prefix, and 7
    characters is the floor (the shortest id a person types)."""
    a, b = str(a or ""), str(b or "")
    n = min(len(a), len(b))
    return n >= 7 and a[:n] == b[:n]


def check():
    """(state, message) — the one function the suite and a human share."""
    emu = read("build/emu/manifest.json")
    fw = read("build/manifest.json")
    h = head()
    if not os.path.isdir(os.path.join(ROOT, "build/emu")):
        return "missing", "no build/emu here (the web and provenance checks skip themselves)"
    if emu is None:
        return "unversioned", "build/emu is there but its manifest.json is missing or not JSON"
    e = emu.get("commit")
    if not e:
        return "unversioned", "build/emu/manifest.json names no commit: rebuild it with sh web/emu/build.sh"
    f = (fw or {}).get("commit")
    parts = []
    if f and not same(f, e):
        parts.append(f"the firmware manifest is {short(f)}")
    if h and not same(h, e):
        parts.append(f"HEAD is {short(h)}")
    if not parts:
        return "ok", f"build/emu is this tree ({short(e)}, emcc {emu.get('emcc', '?').split()[-1] if emu.get('emcc') else '?'})"
    return "stale", ("build/emu was built from %s but %s: rebuild it with `sh web/emu/build.sh` (needs the pinned "
                     "emcc, docs/BUILD_AC791N.md)" % (short(e), " and ".join(parts)))


def main(argv):
    quiet = "--quiet" in argv
    state, msg = check()
    if not quiet:
        print("emulator: %s: %s" % (state, msg))
    return 0 if state in ("ok", "missing") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
