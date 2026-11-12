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
if len(rows) != 251:
    fails.append("expected 251 presets, got %d" % len(rows))
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
if fails:
    print("PRESET CATEGORY TEST FAILED")
    for f in fails[:20]:
        print("  " + f)
    sys.exit(1)
print("preset categories: %d/%d, one primary category each" % (len(rows), len(rows)))
