"""The factory library, read as data: counts, categories, FX, modulation and near-duplicates.

`tests/preset_audit_test.c` measures the SOUND of every preset through the real engine (silence, level,
range, exact duplicates). This tool measures the LIBRARY: what each sound is made of, how the four sends
are used across the whole bank, which sounds carry motion (the `x[]` extra parameters: LFO, matrix,
filter, dist type, voice) and which two presets sit so close together that a player would call them the
same sound.

    build/host/preset_audit_test --dump > build/preset_dump.jsonl
    python3 tools/preset_audit.py [--json build/preset_audit.json]

The data comes from the firmware's own arrays (the test's `--dump` links `engines.c`), never from a regex
over the C source: a preset that is added or changed shows up here on the next dump.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUMP = os.path.join(ROOT, "build", "preset_dump.jsonl")
LABEL_ORDER = ["ANALOG", "PHASE", "LOFI", "VOICE", "WHEEL", "DRUM", "NOISE", "FM6", "WTABLE"]
# every engine's EDIT ranges, from the descriptors in the same files; the C test checks the same bounds
RANGE = {
    "ANALOG": [(0, 9), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127), (-24, 24)],
    "PHASE": [(0, 7), (0, 8), (0, 127), (0, 127), (0, 127), (0, 1), (0, 127), (0, 127)],
    "LOFI": [(0, 4), (0, 6), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127)],
    "VOICE": [(0, 127), (0, 127), (0, 127), (-12, 12), (0, 127), (0, 127), (0, 127), (0, 127)],
    "WHEEL": [(0, 15), (-8, 8), (-8, 8), (-8, 8), (0, 6), (0, 127), (0, 127), (0, 2)],
    "DRUM": [(0, 127)] * 8,
    "NOISE": [(0, 3), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127)],
    "FM6": [(0, 32), (-7, 7), (-64, 63), (-16, 16), (-64, 63), (-7, 7), (0, 127), (0, 127)],
    "WTABLE": [(0, 127), (0, 127), (0, 1), (0, 127), (0, 127), (0, 127), (0, 127), (0, 127)],
}
SEND = ("DIST", "CHORUS", "DELAY", "REVERB")
# the categories the browser sorts by (tools/gen_preset_categories.py is the authority for the rule; this
# mirrors it so a name that no rule claims shows up instead of being silently dropped)
CAT_RULES = [
    ("DRUMS", ("DRUM", "KIT", "909", "808", "LINN", "TRAP", "BEAT", "PERC")),
    ("BELLS", ("BELL", "CHIME", "GONG", "CELEST", "MALLET")),
    ("BASS", ("BASS", "SUB", "REESE", "303", "GROWL")),
    ("LEADS", ("LEAD", "SOLO", "SAW", "SQUELCH", "ACID")),
    ("PADS", ("PAD", "DRONE", "CHOIR", "STRING", "ORCH", "ATMOS", "CLOUD", "AIR")),
    ("KEYS", ("EP", "PIANO", "KEY", "CLAV", "RHODE")),
    ("PLUCKS", ("PLUCK", "HARP", "MARIMBA", "XYLO")),
    ("ARPS", ("ARP", "SEQ", "PULSE", "GATE")),
    ("TEXTURES", ("NOISE", "TEXTURE", "WOBBLE", "GLITCH", "MOTION", "EVOLVE")),
]
CAT_DEFAULT = {"ANALOG": "KEYS", "PHASE": "TEXTURES", "LOFI": "TEXTURES", "VOICE": "PADS",
               "WHEEL": "ORGANS", "DRUM": "DRUMS", "NOISE": "TEXTURES", "FM6": "KEYS", "WTABLE": "PADS"}


def category(engine, name):
    up = name.upper()
    for cat, keys in CAT_RULES:
        if any(k in up for k in keys):
            return cat
    return CAT_DEFAULT.get(engine, "KEYS")


def load():
    if not os.path.exists(DUMP):
        raise SystemExit(f"{DUMP} is missing: run\n"
                         "  cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_audit_test "
                         "tests/preset_audit_test.c -lm\n  build/host/preset_audit_test --dump > "
                         "build/preset_dump.jsonl")
    lib = {}
    for line in open(DUMP, encoding="utf-8"):
        p = json.loads(line)
        p["fx"] = [v - 1 for v in p["fx"]]          # stored +1, 0 = the default
        p["x"] = [tuple(v) for v in p["x"]]
        p["cat"] = category(p["engine"], p["name"])
        lib.setdefault(p["engine"], []).append(p)
    return lib


def main():
    lib, problems = load(), []
    engines = [e for e in LABEL_ORDER if e in lib] + [e for e in lib if e not in LABEL_ORDER]
    for eng in engines:
        presets = lib[eng]
        for k, p in enumerate(presets):
            for i, (lo, hi) in enumerate(RANGE[eng]):
                if not lo <= p["e"][i] <= hi:
                    problems.append(f"{eng} '{p['name']}': E{i} = {p['e'][i]} outside {lo}..{hi}")
            for i, v in enumerate(p["env"]):
                if not 0 <= v <= 127:
                    problems.append(f"{eng} '{p['name']}': ENV[{i}] = {v} outside 0..127")
    # names and exact duplicates inside one engine (the C test checks the same two), plus near duplicates:
    # the same engine parameter vector with every send and envelope stage within a small margin
    for eng, presets in lib.items():
        seen = {}
        for p in presets:
            seen.setdefault(p["name"], 0)
            seen[p["name"]] += 1
        for name, n in seen.items():
            if n > 1:
                problems.append(f"{eng}: '{name}' appears {n} times")
        for i in range(len(presets)):
            for j in range(i + 1, len(presets)):
                a, b = presets[i], presets[j]
                if a["e"] != b["e"]:
                    continue
                if all(abs(a["env"][k] - b["env"][k]) <= 4 for k in range(4)) and \
                        all(abs(a["fx"][k] - b["fx"][k]) <= 4 for k in range(4)):
                    problems.append(f"{eng}: '{a['name']}' and '{b['name']}' are the same sound "
                                    f"(identical engine vector, envelope and sends within 4)")

    total = sum(len(v) for v in lib.values())
    print(f"library: {total} presets in {len(lib)} engines")
    print(f"{'engine':8s} {'n':>3s} {'motion':>6s} {'noFX':>4s}  {'DIST':>5s} {'CHOR':>5s} {'DLY':>4s} {'REV':>4s}  categories")
    for eng in engines:
        presets = lib[eng]
        with_x = sum(1 for p in presets if p["x"])
        nofx = sum(1 for p in presets if not any(p["fx"]))
        avg = [round(sum(p["fx"][k] for p in presets) / max(1, len(presets))) for k in range(4)]
        cats = {}
        for p in presets:
            cats[p["cat"]] = cats.get(p["cat"], 0) + 1
        print(f"{eng:8s} {len(presets):3d} {with_x:6d} {nofx:4d}  {avg[0]:5d} {avg[1]:5d} {avg[2]:4d} {avg[3]:4d}  "
              + " ".join(f"{k}:{v}" for k, v in sorted(cats.items())))
    allcat = {}
    for presets in lib.values():
        for p in presets:
            allcat[p["cat"]] = allcat.get(p["cat"], 0) + 1
    print("categories:", ", ".join(f"{k} {v}" for k, v in sorted(allcat.items(), key=lambda kv: -kv[1])))
    print(f"presets with motion (x[]): {sum(1 for ps in lib.values() for p in ps if p['x'])}/{total}")
    print(f"presets with no send at all: {sum(1 for ps in lib.values() for p in ps if not any(p['fx']))}")
    for eng in engines:
        presets = lib[eng]
        for p in presets:
            if not p["x"]:
                pass
    print(f"problems: {len(problems)}")
    for p in problems:
        print("  " + p)

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"total": total,
                       "engines": {e: [{"name": p["name"], "cat": p["cat"], "fx": p["fx"],
                                               "motion": len(p["x"])} for p in lib[e]] for e in engines},
                       "problems": problems}, f, indent=1)
        print("wrote " + out)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
