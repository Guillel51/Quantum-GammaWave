#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Every factory preset's x[] fits the six slots preset_t gives it.

A seventh XP(...) in the initializer is not an error to the compiler -- it is a warning, and the build runs
with -w -- so the list is written, the seventh value is dropped, and the preset names a modulation it does
not get. AURORA DRIFT did exactly that: its aftertouch to cutoff was in the seventh slot from 1.5.19 to
1.12.11, and the sound never had it. The tables are read here, so the count is a test rather than a hope.
"""
import glob
import re
import sys

SLOTS = 6
entry = re.compile(r'\{"([^"]+)",\s*\{[^}]*\},\s*\{[^}]*\},\s*-?\d+,\s*\d+,\s*FX\([^)]*\),\s*'
                   r'(?:PAT\(\d+\),\s*)?\{([^}]*)\}')
bad = 0
n = 0
for f in sorted(glob.glob("firmware/src/eng_*.c")):
    src = open(f, encoding="utf-8").read()
    for m in entry.finditer(src):
        n += 1
        used = len(re.findall(r"XP\(", m.group(2)))
        if used > SLOTS:
            print("preset_slots: %s: %s has %d entries in %d slots" % (f, m.group(1), used, SLOTS))
            bad += 1
if not n:
    print("preset_slots: no factory presets found")
    sys.exit(1)
print("preset_slots: %d factory presets, all inside their %d slots" % (n, SLOTS))
sys.exit(1 if bad else 0)
