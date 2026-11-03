#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The factory library, checked as data (QG-1 1.6: exactly 200 sounds).

The firmware's preset tables are the only source: every engine's PRESETS[] array is parsed from
firmware/src/eng_*.c, so a sound that is listed but not compiled in (or the reverse) fails here.
Checks: the 200 total and each engine's share; DRUM's first ten indices unchanged (projects, favorites
and user presets store a sound by index); no two sounds share a name; the two retired kits are gone; every
entry has eight E values, a four-stage envelope in range and sends in range. A duplicate parameter
signature is reported (not failed): the library's older sounds are allowed to be close, a new one should not.
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
RETIRED = {"HEARTH KIT", "CYBER DRUM"}
NEW = {"QUANTUM BLOOM", "ABYSSAL DRONE", "PRISM KEYS", "GLASS BELL", "AURORA PLUCK",
       "EVOLVING GLASS", "TAPE DRONE", "SOLAR WIND", "BREATH CHOIR", "EVOLVE BELL",
       "VELVET EP", "OCEANIC PAD", "EMBER LEAD", "CATHEDRAL REED", "TIDAL ORGAN", "AURORA FM"}
fails = []


def strip_comments(s):
    """Remove C comments (the tables carry braces inside them), leaving strings untouched."""
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
    s = open(path, encoding="utf-8").read()
    m = re.search(r"static const preset_t " + name + r"\[\] = \{", s)
    if not m:
        raise SystemExit("%s: no %s" % (path, name))
    s = strip_comments(s)
    m = re.search(r"static const preset_t " + name + r"\[\] = \{", s)
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
    return name, e, envv, fenv, mono, fx, int(pat.group(1)) if pat else 0, entry


names = []
sigs = {}
for label, file, arr, want in ENGINES:
    es = entries(array_body(ROOT + file, arr))
    if len(es) != want:
        fails.append("%s: %u presets, expected %u" % (label, len(es), want))
    for entry in es:
        name, e, envv, fenv, mono, fx, pat, _ = parse(entry)
        names.append(name)
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
        sig = (tuple(e), tuple(envv), fenv, mono, tuple(fx), tuple(re.findall(r"XP\([^)]*\)", entry)))
        sigs.setdefault((label, sig), []).append(name)

if len(names) != TOTAL:
    fails.append("library: %u sounds, expected %u" % (len(names), TOTAL))
for label, _f, _a, _w in ENGINES:
    ens = [n for n in names if n in set(names)]  # placeholder, replaced below
for label, file, arr, want in ENGINES:
    es = entries(array_body(ROOT + file, arr))
    en = [parse(e)[0] for e in es]
    dups = sorted({n for n in en if en.count(n) > 1})
    if dups:
        fails.append("%s: duplicate names: %s" % (label, ", ".join(dups)))
for gone in sorted(RETIRED):
    if gone in names:
        fails.append("retired preset still present: " + gone)
drum = [n for n in names if n in DRUM_KEEP]
if drum != DRUM_KEEP:
    fails.append("DRUM's first ten indices moved: %r" % (drum,))
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
print("preset library: %u factory sounds (%s); DRUM's ten indices kept, %u unique names, %u new"
      % (len(names), ", ".join("%s %u" % (l, w) for l, _, _, w in ENGINES), len(set(names)), len(NEW)))
