"""The factory library, read as data: counts, categories, variety, motion and how close two sounds really are.

`tests/preset_audit_test.c` measures the SOUND of every preset through the real engine (silence, level,
range, exact duplicates). This tool measures the LIBRARY: what each sound is made of, how the four sends
are used across the whole bank, which sounds carry motion (the `x[]` extra parameters: LFO, matrix,
filter, dist type, voice), how much room each engine's parameter ranges actually get used, and which two
presets sit so close together that a player would call them the same sound.

    build/host/preset_audit_test --dump > build/preset_dump.jsonl
    python3 tools/preset_audit.py [--near 0.05] [--strict] [--json build/preset_audit.json]

Everything comes from the firmware's own arrays: the test's `--dump` links `engines.c` and writes a header
with every engine's order, table size and EDIT ranges (read from the descriptors), then one line per preset.
The tool used to carry the ranges in a hand-kept table, which went stale the moment an engine was added
(ADDTV made it refuse to run). It no longer has an opinion of its own about any engine.

Two presets are "the same sound" when the distance below a small threshold. The distance is measured on
what a player hears, not on the bytes: every EDIT value in its own range, the four envelope stages, the
filter envelope, MONO/POLY, the four sends and the `x[]` extras, each normalised to 0..1 and averaged with
the weights the DSP gives them. Identical vectors are a failure; close ones are a list to work through.
"""
import json
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DUMP = os.path.join(ROOT, "build", "preset_dump.jsonl")
SEND = ("DIST", "CHORUS", "DELAY", "REVERB")
DEFAULT_NEAR = 0.035
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
               "WHEEL": "ORGANS", "DRUM": "DRUMS", "NOISE": "TEXTURES", "FM6": "KEYS", "WTABLE": "PADS",
               "ADDTV": "BELLS"}
# how much each part of a patch counts when two sounds are compared: the engine vector is what makes the
# sound what it is, the sends are the room around it
W_E, W_ENV, W_FENV, W_MONO, W_FX, W_X = 1.0, 0.55, 0.2, 0.15, 0.35, 0.45


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
    head, lib, order = None, {}, []
    with open(DUMP, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if "engines" in line and head is None and not line.startswith('{"engine"'):
                head = json.loads(line)["engines"]
                continue
            p = json.loads(line)
            p["fx"] = [v - 1 for v in p["fx"]]          # stored +1, 0 = the default
            p["x"] = [tuple(v) for v in p["x"]]
            p["cat"] = category(p["engine"], p["name"])
            if p["engine"] not in lib:
                lib[p["engine"]] = []
                order.append(p["engine"])
            lib[p["engine"]].append(p)
    if head is None:
        raise SystemExit(f"{DUMP} has no engine header: dump it again with the current "
                         "tests/preset_audit_test.c")
    meta = {}
    for h in head:
        if h["npresets"]:
            meta[h["name"]] = h
    return lib, order, meta


ENUM_FMT = (8, 11)          # core.h: F_ENUM, F_ONOFF -- a choice, not a number
# and some columns ARE choices written as numbers: FM6's ALG and SLOT (0 == 14 is not "a bit brighter", it is another
# patch), a table's index, a scale. Two presets on different ones are two sounds, so they count 0 or 1 too.
CHOICE_LABELS = {"ALG", "SLOT", "WAVE", "WAV#", "ALGO", "TYPE", "KIT", "MODE", "SCALE", "ORDER", "DIV", "PATCH"}


def distance(a, b, rng, typ, lbl):
    """0 = the same sound. Each term is a mean absolute difference in its own 0..1 scale; an enumerated column
    (the wave, the FM6 algorithm or its patch slot, a kit's instrument) counts 0 or 1, never in between."""
    def norm(i, v):
        lo, hi = rng[i]
        return (v - lo) / (hi - lo) if hi > lo else 0.0
    choice = [typ[i] in ENUM_FMT or lbl[i] in CHOICE_LABELS for i in range(8)]
    de = sum((0.0 if a["e"][i] == b["e"][i] else 1.0) if choice[i] else
             abs(norm(i, a["e"][i]) - norm(i, b["e"][i])) for i in range(8)) / 8.0
    denv = sum(abs(a["env"][k] - b["env"][k]) for k in range(4)) / (4 * 127.0)
    dfenv = abs(a["fenv"] - b["fenv"]) / 128.0
    dmono = 0.0 if a["mono"] == b["mono"] else 1.0
    dfx = sum(abs(a["fx"][k] - b["fx"][k]) for k in range(4)) / (4 * 128.0)
    ax, bx = dict(a["x"]), dict(b["x"])
    keys = set(ax) | set(bx)
    if keys:
        dx = sum(0.0 if ax.get(k) == bx.get(k) else 1.0 for k in keys) / float(len(keys))
    else:
        dx = 0.0
    w = W_E + W_ENV + W_FENV + W_MONO + W_FX + W_X
    return (W_E * de + W_ENV * denv + W_FENV * dfenv + W_MONO * dmono + W_FX * dfx + W_X * dx) / w


FP = os.path.join(ROOT, "build", "preset_fingerprint.jsonl")


def load_sound(path=None):
    """The measured fingerprints: `preset_audit_test --fingerprint`. What the bank SOUNDS like, per preset."""
    path = path or FP
    if not os.path.exists(path):
        raise SystemExit(f"{path} is missing: run\n  build/host/preset_audit_test --fingerprint > {path}")
    lib = {}
    for line in open(path, encoding="utf-8"):
        p = json.loads(line)
        bands = [math.log10(1.0 + b) for b in p["bands"]]
        mean = sum(bands) / len(bands)
        p["shape"] = [b - mean for b in bands]          # the timbre, with the level taken out
        p["level"] = math.log10(1.0 + p["rms"])
        p["atk"] = math.log10(1.0 + p["atk_ms"])
        p["dec"] = math.log10(1.0 + p["dec_ms"])
        lib.setdefault(p["engine"], []).append(p)
    return lib


def sound_distance(a, b):
    """How far apart two MADE sounds are: the spectral shape (what it sounds like), the level and the envelope."""
    ds = sum(abs(x - y) for x, y in zip(a["shape"], b["shape"])) / len(a["shape"]) / 1.5
    dl = abs(a["level"] - b["level"])
    de = (abs(a["atk"] - b["atk"]) + abs(a["dec"] - b["dec"])) / 2.0 / 1.5
    return 0.55 * ds + 0.2 * dl + 0.25 * de


def sound_report(lib, th):
    print("\nsound: what the bank MEASURES like (one held note per preset, ten bands and the envelope)")
    print(f"{'engine':8s} {'n':>3s}  closest pairs (distance, 0 = the same sound)")
    pairs = []
    for eng in lib:
        ps = lib[eng]
        best = None
        for i in range(len(ps)):
            for j in range(i + 1, len(ps)):
                d = sound_distance(ps[i], ps[j])
                pairs.append((d, eng, ps[i]["name"], ps[j]["name"]))
                if best is None or d < best[0]:
                    best = (d, ps[i]["name"], ps[j]["name"])
        if best:
            print(f"{eng:8s} {len(ps):3d}  {best[0]:5.3f}  '{best[1]}' / '{best[2]}'")
    pairs.sort()
    near = [x for x in pairs if x[0] < th]
    print(f"\nclosest pairs by sound (under {th:g}: to look at):")
    for d, eng, a, b in pairs[:20]:
        print(f"  {d:5.3f}  {eng:8s} '{a}' / '{b}'")
    print(f"pairs under {th:g}: {len(near)}")
    for d, eng, a, b in near:
        print(f"  {d:5.3f}  {eng:8s} '{a}' / '{b}'")
    return near


def inventory(path, dump=None, sound=None):
    """`--inventory FILE`: the whole bank as data -- counts, categories, motion, sends, measured level, and the
    closest pairs by parameters and by sound. Everything here comes from the firmware's own tables and from the
    render, so the document cannot drift from the bank the way a hand-written count does."""
    import datetime
    dump = dump or os.path.join(ROOT, "build", "preset_dump.jsonl")
    sound = sound or FP
    rows = [json.loads(l) for l in open(dump, encoding="utf-8") if l.startswith("{")]
    meta = rows[0]["engines"] if "engines" in rows[0] else None
    presets = [r for r in rows if "name" in r]
    sl = load_sound(sound)
    fp = {(r["engine"], r["name"]): r for rs in sl.values() for r in rs}
    cats = {}
    cp = os.path.join(ROOT, "web", "preset_categories.json")
    if os.path.exists(cp):
        for c in json.load(open(cp, encoding="utf-8")):
            cats[(c["engine"], c["name"])] = c["category"]
    out = {
        "what": "Quantum GammaWave factory preset inventory",
        "source_commit": os.popen("git rev-parse --short HEAD").read().strip(),
        "class": "host x86, the firmware's own tables, one held note through the real load and render per preset",
        "generated": datetime.date.today().isoformat(),
        "total": len(presets),
        "engines": {},
        "categories": {},
        "nearest_pairs": [],
        "nearest_sound": [],
    }
    byeng = {}
    for r in presets:
        byeng.setdefault(r["engine"], []).append(r)
    for eng, ps in byeng.items():
        m = next(e for e in meta if e["name"] == eng)
        c = {}
        for r in ps:
            k = cats.get((eng, r["name"]))
            if k:
                c[k] = c.get(k, 0) + 1
                out["categories"][k] = out["categories"].get(k, 0) + 1
        rms = [20 * math.log10(max(1e-9, fp[(eng, r["name"])]["rms"]) / 32768.0) for r in ps if (eng, r["name"]) in fp]
        pk = [20 * math.log10(max(1e-9, fp[(eng, r["name"])]["peak"]) / 32768.0) for r in ps if (eng, r["name"]) in fp]
        lv = [x[1] for r in ps for x in r["x"] if x[0] == 0]
        near = sorted(({"a": a["name"], "b": b["name"], "distance": round(distance(a, b, m["rng"], m["typ"], m["lbl"]), 4)}
                       for i, a in enumerate(ps) for b in ps[i + 1:]),
                      key=lambda x: x["distance"])[:5]
        out["engines"][eng] = {
            "count": m["npresets"],
            "visible": len(ps),
            "legacy": m["legacy_n"],
            "categories": dict(sorted(c.items())),
            "with_motion": sum(1 for r in ps if r["x"]),
            "no_send": sum(1 for r in ps if not any(r["fx"][1:])),
            "rms_median_db": round(sorted(rms)[len(rms) // 2], 2) if rms else None,
            "rms_min_db": round(min(rms), 2) if rms else None,
            "rms_max_db": round(max(rms), 2) if rms else None,
            "peak_max_db": round(max(pk), 2) if pk else None,
            "level_median": sorted(lv)[len(lv) // 2] if lv else None,
            "nearest": near,
        }
        for i, a in enumerate(ps):
            for b in ps[i + 1:]:
                if (eng, a["name"]) in fp and (eng, b["name"]) in fp:
                    out["nearest_sound"].append({"engine": eng, "a": a["name"], "b": b["name"],
                                                 "distance": round(sound_distance(fp[(eng, a["name"])], fp[(eng, b["name"])]), 4)})
    out["nearest_sound"].sort(key=lambda x: x["distance"])
    out["nearest_sound"] = out["nearest_sound"][:20]
    open(path, "w", encoding="utf-8").write(json.dumps(out, indent=2, ensure_ascii=False) + "\n")
    print(f"inventory: {out['total']} presets in {len(out['engines'])} engines -> {path}")


def variety(presets, rng):
    """How much of each EDIT range the engine's bank actually visits (distinct values used)."""
    return [len({p["e"][i] for p in presets}) for i in range(8)]


def gate_sound():
    """Two presets of one engine that MEASURE alike are a copy, whatever their parameters say. It reads
    the fingerprints only, so it runs before `load()`: nothing in the suite writes
    `build/preset_dump.jsonl`, and a gate that needs a leftover file from a manual run does not gate
    anything -- v1.3.6 and v1.3.7 could not run it on a clean tree (their CI failed here, on the sound
    gate, not on the bank)."""
    i = sys.argv.index("--gate-sound")
    gate = float(sys.argv[i + 1])
    path = sys.argv[sys.argv.index("--fingerprint") + 1] if "--fingerprint" in sys.argv else None
    lib = load_sound(path)
    bad = [(d, eng, a, b) for d, eng, a, b in
           ((sound_distance(ps[i], ps[j]), eng, ps[i]["name"], ps[j]["name"])
            for eng, ps in lib.items() for i in range(len(ps)) for j in range(i + 1, len(ps)))
           if d < gate]
    print(f"preset similarity: {sum(len(p) * (len(p) - 1) // 2 for p in lib.values())} pairs measured, "
          f"{len(bad)} closer than {gate:g}")
    for d, eng, a, b in sorted(bad):
        print(f"  too alike: {eng:8s} '{a}' / '{b}' measure {d:.3f} apart")
    sys.exit(1 if bad else 0)


def main():
    if "--gate-sound" in sys.argv:
        gate_sound()                  # it exits: the dump is not its input
    lib, order, meta = load()
    problems, near, pairs = [], [], []
    near_th = DEFAULT_NEAR
    if "--near" in sys.argv:
        near_th = float(sys.argv[sys.argv.index("--near") + 1])

    engines = [e for e in order if e in lib]
    for eng in engines:
        presets = lib[eng]
        rng, typ = meta[eng]["rng"], meta[eng]["typ"]
        for p in presets:
            for i, (lo, hi) in enumerate(rng):
                if not lo <= p["e"][i] <= hi:
                    problems.append(f"{eng} '{p['name']}': E{i} = {p['e'][i]} outside {lo}..{hi} "
                                    f"(the firmware's descriptor says {lo}..{hi})")
            for i, v in enumerate(p["env"]):
                if not 0 <= v <= 127:
                    problems.append(f"{eng} '{p['name']}': ENV[{i}] = {v} outside 0..127")
        n_legacy = sum(1 for p in presets if p["legacy"])
        if n_legacy != meta[eng]["legacy_n"]:
            problems.append(f"{eng}: {n_legacy} preset(s) flagged legacy, the engine declares "
                            f"legacy_n = {meta[eng]['legacy_n']}")
        seen = {}
        for p in presets:
            seen[p["name"]] = seen.get(p["name"], 0) + 1
        for name, n in seen.items():
            if n > 1:
                problems.append(f"{eng}: '{name}' appears {n} times")
        # exact duplicates are a failure; near ones are the working list
        for i in range(len(presets)):
            for j in range(i + 1, len(presets)):
                a, b = presets[i], presets[j]
                if a["e"] == b["e"] and a["env"] == b["env"] and a["fx"] == b["fx"] and a["fenv"] == b["fenv"]:
                    problems.append(f"{eng}: '{a['name']}' and '{b['name']}' are the same patch "
                                    f"(identical engine vector, envelope, filter env and sends)")
                    continue
                d = distance(a, b, rng, meta[eng]["typ"], meta[eng]["lbl"])
                pairs.append((d, eng, a["name"], b["name"]))
                if d < near_th:
                    near.append((d, eng, a["name"], b["name"]))

    total = sum(len(v) for v in lib.values())
    visible = sum(1 for ps in lib.values() for p in ps if not p["legacy"])
    print(f"library: {total} presets in {len(lib)} engines ({visible} visible, {total - visible} legacy)")
    print(f"{'engine':8s} {'n':>3s} {'leg':>3s} {'motion':>6s} {'noFX':>4s}  "
          f"{'DIST':>5s} {'CHOR':>5s} {'DLY':>4s} {'REV':>4s}  {'distinct E0..E7':>20s}  categories")
    for eng in engines:
        presets = lib[eng]
        rng = meta[eng]["rng"]
        with_x = sum(1 for p in presets if p["x"])
        nofx = sum(1 for p in presets if not any(p["fx"]))
        avg = [round(sum(p["fx"][k] for p in presets) / max(1, len(presets))) for k in range(4)]
        cats = {}
        for p in presets:
            cats[p["cat"]] = cats.get(p["cat"], 0) + 1
        var = variety(presets, rng)
        print(f"{eng:8s} {len(presets):3d} {meta[eng]['legacy_n']:3d} {with_x:6d} {nofx:4d}  "
              f"{avg[0]:5d} {avg[1]:5d} {avg[2]:4d} {avg[3]:4d}  "
              f"{'/'.join(str(v) for v in var):>20s}  "
              + " ".join(f"{k}:{v}" for k, v in sorted(cats.items())))
    allcat = {}
    for presets in lib.values():
        for p in presets:
            allcat[p["cat"]] = allcat.get(p["cat"], 0) + 1
    print("categories:", ", ".join(f"{k} {v}" for k, v in sorted(allcat.items(), key=lambda kv: -kv[1])))
    print(f"presets with motion (x[]): {sum(1 for ps in lib.values() for p in ps if p['x'])}/{total}")
    print(f"presets with no send at all: {sum(1 for ps in lib.values() for p in ps if not any(p['fx']))}")

    pairs.sort()
    print(f"\nclosest pairs overall (distance, 0 = the same sound; under {near_th:g}: to look at):")
    for d, eng, a, b in pairs[:15]:
        print(f"  {d:5.3f}  {eng:8s} '{a}' / '{b}'")
    print(f"\npairs under {near_th:g}: {len(near)}")
    for d, eng, a, b in near:
        print(f"  {d:5.3f}  {eng:8s} '{a}' / '{b}'")

    print(f"\nproblems: {len(problems)}")
    for p in problems:
        print("  " + p)

    if "--inventory" in sys.argv:
        inventory(sys.argv[sys.argv.index("--inventory") + 1])
        sys.exit(0)

    if "--sound" in sys.argv:
        sth = 0.06
        if "--near-sound" in sys.argv:
            sth = float(sys.argv[sys.argv.index("--near-sound") + 1])
        sound_report(load_sound(), sth)

    if "--json" in sys.argv:
        out = sys.argv[sys.argv.index("--json") + 1]
        os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"total": total, "visible": visible,
                       "engines": {e: {"npresets": meta[e]["npresets"],
                                       "legacy_n": meta[e]["legacy_n"],
                                       "distinct": variety(lib[e], meta[e]["rng"]),
                                       "presets": [{"name": p["name"], "cat": p["cat"], "fx": p["fx"],
                                                    "motion": len(p["x"]), "legacy": p["legacy"]}
                                                   for p in lib[e]]} for e in engines},
                       "closest": [{"d": round(d, 4), "engine": e, "a": a, "b": b}
                                   for d, e, a, b in pairs[:40]],
                       "near_threshold": near_th,
                       "near": [{"d": round(d, 4), "engine": e, "a": a, "b": b} for d, e, a, b in near],
                       "problems": problems}, f, indent=1)
        print("wrote " + out)
    if problems:
        return 1
    if "--strict" in sys.argv and near:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
