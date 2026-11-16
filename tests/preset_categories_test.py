#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The factory library: every sound the browser lists has one primary category, no extras, no duplicates."""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import gen_preset_categories as g

ALLOWED = {"BASS", "LEADS", "PADS", "PLUCKS", "KEYS", "BELLS", "ORGANS", "BRASS", "STRINGS",
           "DRUMS", "TEXTURES", "FX"}
rows = g.table()
fails = []
if len(rows) != 273:
    fails.append("expected 273 presets, got %d" % len(rows))
seen = set()
for eng, i, n, c in rows:
    if (eng, n) in seen:
        fails.append("duplicate name in %s: %s" % (eng, n))
    seen.add((eng, n))
    if c not in ALLOWED:
        fails.append("unknown category %s for %s" % (c, n))
m = json.load(open(os.path.join(os.path.dirname(__file__), "..", "web", "preset_categories.json")))
if len(m) != len(rows) or any(x["name"] != n or x["category"] != c for x, (_, _, n, c) in zip(m, rows)):
    fails.append("web/preset_categories.json differs from the firmware table")
# (FASE 2, 2026-11-16) the firmware's own table (build/gen/preset_cats.h) says the same thing, in four bits a
# sound, by engine and by the sound's index in that engine's table (the ids the projects and the editor store)
import re
import tempfile
hdr = os.path.join(os.path.dirname(__file__), "..", "build", "gen", "preset_cats.h")
allrows = g.table_all()
if not os.path.exists(hdr):
    hdr = tempfile.NamedTemporaryFile("w", suffix=".h", delete=False).name
    g.header(hdr)
t = open(hdr, encoding="utf-8").read()
base = [None if x.strip() == "PCAT_GONE" else int(x.rstrip("u")) for x in
        re.search(r"PCAT_BASE\[NENGINES\] = \{(.*?)\};", t).group(1).split(",")]
nib = [int(x.rstrip("u")) for x in re.search(r"PCAT_NIB\[\d+u?\] = \{(.*?)\};", t).group(1).split(",")]
names = re.findall(r'"([^"]+)"', re.search(r"PCAT_NAME\[PCAT_N\]\[7\] = \{(.*?)\};", t).group(1))
cats = sorted({r[4] for r in allrows})
if names != ["ALL"] + cats:
    fails.append("preset_cats.h names %r, the tables say %r" % (names, ["ALL"] + cats))


def cat(e, k):
    b = base[e]
    return 0 if b is None else (nib[(b + k) >> 1] >> (((b + k) & 1) * 4)) & 15


wrong = [(e, i, n, c) for e, num, i, n, c, _leg in allrows if cat(num, i) != cats.index(c) + 1]
if wrong:
    fails.append("preset_cats.h disagrees with the tables: %r" % (wrong[:6],))
if len(nib) != (len(allrows) + 1) // 2:
    fails.append("preset_cats.h packs %d B for %d sounds" % (len(nib), len(allrows)))
if any(base[num] is None for num in {r[1] for r in allrows}):
    fails.append("preset_cats.h has no base for an engine that has sounds")

if fails:
    print("PRESET CATEGORY TEST FAILED")
    for f in fails[:20]:
        print("  " + f)
    sys.exit(1)
print("preset categories: %d/%d, one primary category each" % (len(rows), len(rows)))
