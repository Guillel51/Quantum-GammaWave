#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The factory library, checked as data (QG-1 1.6: 200 sounds, plus two legacy kits).

The firmware's tables are the only source: every engine's PRESETS[] is parsed from firmware/src/eng_*.c.
Checks: 200 browsable sounds and each engine's share; DRUM's ten indices unchanged and its two pre-1.6
kits still present as legacy entries at indices 10/11 (so an old project loads the exact sound it saved);
no two sounds of an engine share a name; every entry's E count, envelope and sends in range. Duplicate
parameter signatures are reported (not failed): the older library is allowed to be close.
"""
import re
import sys

ROOT = "firmware/src/"
ENGINES = [
    ("ANALOG", "eng_analog.c", "ANALOG_PRESETS", 56),
    ("PHASE", "eng_phase.c", "PHASE_PRESETS", 25),
    ("LOFI", "eng_lofi.c", "LOFI_PRESETS", 25),
    ("FORMANT", "eng_formant.c", "FORMANT_PRESETS", 20),
    ("WHEEL", "eng_wheel.c", "WHEEL_PRESETS", 18),
    ("DRUM", "eng_drum.c", "DRUM_PRESETS", 10),
    ("NOISE", "eng_noise.c", "NOISE_PRESETS", 17),
    ("FM6", "eng_fm6.c", "FM6_PRESETS", 29),
]
TOTAL = 200
DRUM_KEEP = ["DRUM-X", "909 PUNCH", "808 SUB", "LINNDRUM", "ACOUSTIC", "TRAP 808",
             "LOFI VINYL", "INDUSTRIAL", "ELECTRO", "QUANTUM BEAT"]
LEGACY = ["HEARTH KIT", "CYBER DRUM"]
NEW = {"QUANTUM BLOOM", "ABYSSAL DRONE", "PRISM KEYS", "GLASS BELL", "AURORA PLUCK",
       "EVOLVING GLASS", "TAPE DRONE", "SOLAR WIND", "BREATH CHOIR", "EVOLVE BELL",
       "VELVET EP", "OCEANIC PAD", "EMBER LEAD", "CATHEDRAL REED", "TIDAL ORGAN", "AURORA FM"}
fails = []


def strip_comments(s):
    out, i, n, quote, esc = [], 0, len(s), False, False
    while i < n:
        c = s[i]
        if quote:
            out.append(c)
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                quote = False
            i += 1
        elif c == '"':
            quote = True
            out.append(c)
            i += 1
        elif s.startswith("/*", i):
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif s.startswith("//", i):
            j = s.find("\n", i + 2)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def array_body(path, name):
    s = strip_comments(open(path, encoding="utf-8").read())
    m = re.search(r"static const preset_t " + name + r"\[\] = \{", s)
    if not m:
        raise SystemExit("%s: no %s" % (path, name))
    i = s.index("{", m.start())
    depth, quote, esc = 0, False, False
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
                return s[i + 1:j]
    raise SystemExit("%s: unbalanced %s" % (path, name))


def entries(body):
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
                out.append(body[start:j + 1])
                start = None
    return out


def parse(entry):
    em = re.match(r'^\{\s*"([^"]+)"\s*,\s*(?:\{([^}]*)\}|([A-Z][A-Z0-9_]*))', entry)
    name = em.group(1)
    e = [x.strip() for x in em.group(2).split(",") if x.strip()] if em.group(2) is not None else ["MACRO"] * 8
    env = re.search(r'\{\s*([^}]*)\}\s*,\s*(-?\d+)\s*,\s*(\d+)', entry)
    envv = [int(x) for x in re.findall(r"-?\d+", env.group(1))]
    fenv, mono = int(env.group(2)), int(env.group(3))
    fx = [int(x) for x in re.findall(r"-?\d+", re.search(r"FX\(([^)]*)\)", entry).group(1))]
    pat = re.search(r"PAT\((\d+)\)", entry)
    legacy = bool(re.search(r"\.legacy\s*=\s*1", entry))
    return name, e, envv, fenv, mono, fx, int(pat.group(1)) if pat else 0, legacy


names, active, legacy, sigs = [], {}, {}, {}
for label, file, arr, want in ENGINES:
    es = entries(array_body(ROOT + file, arr))
    act, leg = [], []
    for entry in es:
        name, e, envv, fenv, mono, fx, pat, is_legacy = parse(entry)
        (leg if is_legacy else act).append(name)
        if len(e) != 8:
            fails.append("%s: %s has %u E values, expected 8" % (label, name, len(e)))
        if len(envv) != 4 or any(v < 0 or v > 127 for v in envv):
            fails.append("%s: %s envelope %r out of range" % (label, name, envv))
        if not -64 <= fenv <= 63:
            fails.append("%s: %s filter envelope %d out of range" % (label, name, fenv))
        if mono not in (0, 1):
            fails.append("%s: %s mono %d" % (label, name, mono))
        if len(fx) != 4 or any(v < 0 or v > 128 for v in fx):
            fails.append("%s: %s sends %r out of range" % (label, name, fx))
        if not 0 <= pat <= 32:
            fails.append("%s: %s pattern %d" % (label, name, pat))
        if not is_legacy:
            names.append(name)
            sig = (tuple(e), tuple(envv), fenv, mono, tuple(fx), tuple(re.findall(r"XP\([^)]*\)", entry)))
            sigs.setdefault((label, sig), []).append(name)
    if len(act) != want:
        fails.append("%s: %u library sounds, expected %u" % (label, len(act), want))
    active[label], legacy[label] = act, leg
    en = act + leg
    dups = sorted({n for n in en if en.count(n) > 1})
    if dups:
        fails.append("%s: duplicate names: %s" % (label, ", ".join(dups)))

if len(names) != TOTAL:
    fails.append("library: %u sounds, expected %u" % (len(names), TOTAL))
if active.get("DRUM") != DRUM_KEEP:
    fails.append("DRUM's ten library indices moved: %r" % (active.get("DRUM"),))
if legacy.get("DRUM") != LEGACY:
    fails.append("DRUM's legacy indices 10/11 changed: %r" % (legacy.get("DRUM"),))
for label, leg in legacy.items():
    if label != "DRUM" and leg:
        fails.append("%s: unexpected legacy sounds: %r" % (label, leg))
missing = sorted(NEW - set(names))
if missing:
    fails.append("1.6 sounds missing: " + ", ".join(missing))
for (label, _), group in sorted(sigs.items()):
    if len(group) > 1:
        print("note: %s: %s share one parameter set" % (label, ", ".join(group)))

if fails:
    for f in fails:
        print("FAIL: " + f)
    sys.exit(1)
print("preset library: %u factory sounds (%s); DRUM 10 indices + legacy 10/11 kept, %u unique names, %u new"
      % (len(names), ", ".join("%s %u" % (l, len(active[l])) for l, _, _, _ in ENGINES), len(set(names)), len(NEW)))
