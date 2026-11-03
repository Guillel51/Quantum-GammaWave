#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Primary musical category for every factory sound, from the firmware tables.

Names and order come from eng_*.c. The category is a documented rule over the engine and the name's
keywords (BASS, LEAD, PAD, ...); the engine default is used only when no keyword is present. This does not
change names, ids, parameters or order, and it is the single source for docs/PRESET_CATEGORIES.md and
web/preset_categories.json.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINES = [
    ("ANALOG", "eng_analog.c", "ANALOG_PRESETS", "KEYS"),
    ("PHASE", "eng_phase.c", "PHASE_PRESETS", "TEXTURES"),
    ("LOFI", "eng_lofi.c", "LOFI_PRESETS", "TEXTURES"),
    ("FORMANT", "eng_formant.c", "FORMANT_PRESETS", "PADS"),
    ("WHEEL", "eng_wheel.c", "WHEEL_PRESETS", "ORGANS"),
    ("DRUM", "eng_drum.c", "DRUM_PRESETS", "DRUMS"),
    ("NOISE", "eng_noise.c", "NOISE_PRESETS", "TEXTURES"),
    ("FM6", "eng_fm6.c", "FM6_PRESETS", "KEYS"),
]
RULES = [
    ("DRUMS", ("DRUM", "KIT", "909", "808", "LINN", "TRAP", "BEAT", "PERC")),
    ("BELLS", ("BELL", "CHIME", "GONG", "CELEST", "MALLET")),
    ("BASS", ("BASS", "SUB", "REESE", "303", "GROWL")),
    ("LEADS", ("LEAD", "SOLO", "SAW", "SQUELCH", "ACID")),
    ("PADS", ("PAD", "DRONE", "CHOIR", "STRING", "ORCH", "ATMOS", "CLOUD", "AIR")),
    ("PLUCKS", ("PLUCK", "HARP", "KALIMBA", "GUITAR", "KOTO", "SITAR")),
    ("KEYS", ("KEY", "EP", "PIANO", "CLAV", "RHODE", "WURLI")),
    ("ORGANS", ("ORGAN", "REED", "HARMON", "ACCORD")),
    ("BRASS", ("BRASS", "HORN", "TRUMP", "SAX")),
    ("FX", ("FX", "NOISE", "GLITCH", "WIND", "METAL", "RADIO", "TAPE")),
]


def names(path, array):
    s = open(path, encoding="utf-8").read()
    m = re.search(r"static const preset_t " + array + r"\[\] = \{", s)
    if not m:
        raise SystemExit("no %s in %s" % (array, path))
    body = s[m.end():]
    out, depth, quote, esc, start = [], 0, False, False, None
    for j, c in enumerate(body):
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
            if depth == 1:
                start = j
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                entry = body[start:j + 1]
                nm = re.match(r'\{\s*"([^"]+)"\s*,', entry)
                if nm:
                    out.append(nm.group(1))
                start = None
            if depth < 0:
                break
    return out


def classify(engine, name, default):
    up = name.upper()
    for cat, words in RULES:
        if any(w in up for w in words):
            return cat
    return default


def table():
    rows = []
    for eng, f, arr, default in ENGINES:
        ns = names(os.path.join(ROOT, "firmware/src", f), arr)
        if eng == "DRUM":
            ns = ns[:10]                               # legacy kits 10/11 stay browsable in the firmware, not in the 200
        for i, n in enumerate(ns):
            rows.append((eng, i, n, classify(eng, n, default)))
    return rows


def main():
    rows = table()
    os.makedirs(os.path.join(ROOT, "docs"), exist_ok=True)
    with open(os.path.join(ROOT, "docs/PRESET_CATEGORIES.md"), "w", encoding="utf-8") as f:
        f.write("# Factory preset categories (1.7)\n\n")
        f.write("Generated from `firmware/src/eng_*.c` by `tools/gen_preset_categories.py`. Names, ids and\n")
        f.write("order are untouched. Primary category rules: engine default plus the name keywords below.\n\n")
        f.write("| # | Engine | Preset | Category |\n|---:|---|---|---|\n")
        for k, (eng, i, n, c) in enumerate(rows, 1):
            f.write("| %d | %s | %s | %s |\n" % (k, eng, n, c))
        f.write("\nRules: " + ", ".join("%s=%s" % (c, "/".join(w)) for c, w in RULES) + ".\n")
    with open(os.path.join(ROOT, "web/preset_categories.json"), "w", encoding="utf-8") as f:
        json.dump([{"engine": e, "index": i, "name": n, "category": c} for e, i, n, c in rows],
                  f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("preset categories: %d" % len(rows))


if __name__ == "__main__":
    main()
