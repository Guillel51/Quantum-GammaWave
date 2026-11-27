#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Every factory preset's x[] fits the six slots preset_t gives it.

A seventh XP(...) in the initializer is not an error to the compiler -- it is a warning, and the build runs
with -w -- so the list is written, the seventh value is dropped, and the preset names a modulation it does
not get. AURORA DRIFT did exactly that: its aftertouch to cutoff was in the seventh slot from 1.5.19 to
1.12.11, and the sound never had it. The tables are read here, so the count is a test rather than a hope.

(FASE 1/10) a preset's first field is no longer its name but its offset in PRESET_NAMES, and (FASE 1/8) the
tables are written inside `#ifdef QG_PRESET_SOURCE`: the old pattern, which wanted `{"NAME", ...}` on one
line, had been matching NOTHING and this test had been passing on an empty list. The records are read by
their braces now (a record may span lines, use a macro for its EDIT values, or carry `.legacy = 1`), and the
count is checked against the library's own: 306 factory presets across the eleven tables.
"""
import glob
import re
import sys

SLOTS = 6
TABLES = ["ANALOG_PRESETS", "PHASE_PRESETS", "LOFI_PRESETS", "FORMANT_PRESETS", "WHEEL_PRESETS", "DRUM_PRESETS",
          "NOISE_PRESETS", "FM6_PRESETS", "WT_PRESETS", "AD_PRESETS", "DIGITAL_PRESETS"]
TOTAL = 306          # what the eleven tables hold (docs: the 306-sound library)


def records(body):
    """the record initializers of one array body: each one starts a line at four spaces, in the tables' layout
    (reading them by braces counts the nested e[] / env[] / x[] groups of a macro written with braces, which is
    how this test came to see 315 records where the library has 306)"""
    starts = [m.start() for m in re.finditer(r"^    \{", body, re.M)]
    return [body[a:b] for a, b in zip(starts, starts[1:] + [len(body)])]


bad = 0
n = 0
for f in sorted(glob.glob("firmware/src/*.c")):
    src = open(f, encoding="utf-8").read()
    for table in TABLES:
        at = src.find("static const preset_t %s[] = {" % table)
        if at < 0:
            continue
        body = src[src.find("{", at):src.find("\n};", at) + 1]
        recs = records(body)
        if not recs:
            print("preset_slots: %s: no records read from %s" % (f, table))
            bad += 1
        for r in recs:
            n += 1
            used = r.count("XP(")
            if used > SLOTS:
                print("preset_slots: %s: %s has %d entries in %d slots" % (f, table, used, SLOTS))
                bad += 1
if n != TOTAL:
    print("preset_slots: read %d factory presets, the library has %d" % (n, TOTAL))
    bad += 1
print("preset_slots: %d factory presets, all inside their %d slots" % (n, SLOTS))
sys.exit(1 if bad else 0)
