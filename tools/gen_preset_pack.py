#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""(FASE 1/8) THE FACTORY LIBRARY, PACKED BY DIFFERENCE, FROM THE COMPILER'S OWN BYTES.

The 306 factory presets were 10 404 B of `preset_t` in XIP: 34 B each, and most of every record repeats the
engine's common values (a default envelope, no matrix slots, no legacy id). This tool reads the tables the
way the compiler laid them out and writes the compact form the firmware decodes at boot.

Nothing is estimated: the input is an object file of the real sources, built with `-DQG_PRESET_SOURCE=1`
(build.py compiles it just before the app), and the output is verified by decoding it again here and
comparing every byte against the compiler's. A mismatch stops the build; it never ships.

The encoding, per engine (all offsets into one blob, so no per-table pointer costs anything):

    default   32 B   the common value of each payload byte (the mode over the table)
    masks     4 B x n   bit j set: payload byte j differs from the default
    values    1 B each  the differing bytes, in byte order, preset by preset
    names     2 B x n   the offset in PRESET_NAMES (a delta was measured: the tables do not list their names
                        in increasing order -- DRUM's legacy kits and the reused ones jump back -- so 31 of
                        the 306 would have needed an escape; 2 absolute bytes is the smaller honest form)

So a preset costs 4 B + the bytes that differ + 2 B of name, instead of 34 B. The firmware's
presets_pack_init() (engines.c) rebuilds the exact `preset_t` arrays in .bss at boot, so nothing that reads
a preset (`e->presets[i]`, PR_NAME, the loader) knows this file exists.

  gen_preset_pack.py OBJ DESTDIR     (OBJ: an ELF object of felucca.c with -DQG_PRESET_SOURCE=1)
"""
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# the tables, by the symbol the source gives them (each ends in _PRESETS: engines.c builds its decode list
# from the macro emitted below, so an engine added here is decoded without a second list to keep in step)
TABLES = ["ANALOG_PRESETS", "PHASE_PRESETS", "LOFI_PRESETS", "FORMANT_PRESETS", "WHEEL_PRESETS", "DRUM_PRESETS",
          "NOISE_PRESETS", "FM6_PRESETS", "WT_PRESETS", "AD_PRESETS", "DIGITAL_PRESETS"]
PRESET_SIZE = 34          # core.h preset_t: name (u16) + 32 bytes of payload; asserted on the firmware side
PAYLOAD = PRESET_SIZE - 2
MASK_BYTES = 4            # PAYLOAD / 8


def sections(obj):
    """name -> (file offset, size) from readelf -SW (the toolchain's, or the host's)"""
    out = subprocess.run(["readelf", "-SW", str(obj)], capture_output=True, text=True)
    if out.returncode:
        raise SystemExit("gen_preset_pack: readelf failed: " + out.stderr.strip())
    secs = {}
    for ln in out.stdout.splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)", ln)
        if m:
            secs[m.group(2)] = (int(m.group(4), 16), int(m.group(5), 16))
    return secs


def table_bytes(obj, secs, table):
    """the bytes of one table as the compiler laid them out (-fdata-sections gives each its own section)"""
    name = ".rodata.%s" % table
    if name not in secs:
        raise SystemExit(f"gen_preset_pack: no {name} in {obj}: build the object with -DQG_PRESET_SOURCE=1")
    off, size = secs[name]
    with open(obj, "rb") as f:
        f.seek(off)
        data = f.read(size)
    if size % PRESET_SIZE:
        raise SystemExit(f"gen_preset_pack: {name} is {size} B, not a multiple of {PRESET_SIZE}")
    return data


def pack(name, data):
    """(default, masks, values, name deltas) for one table; raises when the encoding would not round-trip"""
    rows = [data[i:i + PRESET_SIZE] for i in range(0, len(data), PRESET_SIZE)]
    if not 0 < len(rows) <= 255:
        raise SystemExit(f"gen_preset_pack: {name} has {len(rows)} presets (1..255)")
    # the common value of each payload byte; a tie goes to the smaller value, so the output is deterministic
    default = bytearray()
    for j in range(PAYLOAD):
        counts = {}
        for r in rows:
            counts[r[2 + j]] = counts.get(r[2 + j], 0) + 1
        best = max(counts.values())
        default.append(min(v for v, c in counts.items() if c == best))
    masks, values, names = [], bytearray(), bytearray()
    for r in rows:
        m = 0
        for j in range(PAYLOAD):
            if r[2 + j] != default[j]:
                m |= 1 << j
                values.append(r[2 + j])
        masks.append(m)
        names.append(r[0])
        names.append(r[1])
    return bytes(default), masks, bytes(values), bytes(names)


def decode(default, masks, values, names):
    """the inverse of pack(): the exact 34-byte records the firmware's presets_pack_init rebuilds"""
    out, v = bytearray(), 0
    for i, m in enumerate(masks):
        rec = bytearray(PRESET_SIZE)
        rec[0], rec[1] = names[2 * i], names[2 * i + 1]
        for j in range(PAYLOAD):
            if (m >> j) & 1:
                rec[2 + j] = values[v]
                v += 1
            else:
                rec[2 + j] = default[j]
        out += rec
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("gen_preset_pack.py OBJ DESTDIR")
    obj, dest = Path(sys.argv[1]), Path(sys.argv[2])
    secs = sections(obj)
    blob, counts, entry = bytearray(), [], []
    for t in TABLES:
        data = table_bytes(obj, secs, t)
        default, masks, values, names = pack(t, data)
        if decode(default, masks, values, names) != data:
            raise SystemExit(f"gen_preset_pack: {t}: the packed form does not decode to the compiler's bytes")
        dflt = len(blob)
        blob += default
        bits = len(blob)
        blob += b"".join(struct.pack("<I", m) for m in masks)
        vals = len(blob)
        blob += values
        nm = len(blob)
        blob += names
        entry.append((dflt, bits, vals, nm, len(masks)))
        counts.append((t, len(masks)))
        print("gen_preset_pack: %-18s %2d presets  %4d B -> %4d B (default 32, masks %d, values %d, names %d)"
              % (t, len(masks), len(data), 32 + 4 * len(masks) + len(values) + len(names),
                 4 * len(masks), len(values), len(names)))
    if len(blob) > 0xFFFF:
        raise SystemExit("gen_preset_pack: the blob needs 16-bit offsets (over 65 535 B)")

    dest.mkdir(parents=True, exist_ok=True)
    with open(dest / "preset_counts.h", "w", encoding="utf-8") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only\n"
                " * GENERATED by tools/gen_preset_pack.py -- do not edit.\n"
                " * The size of each factory library: the .bss array the boot decoder fills (eng_*.c). */\n")
        f.write("#ifndef QG_PRESET_COUNTS_H\n#define QG_PRESET_COUNTS_H\n")
        for t, n in counts:
            f.write(f"#define PRESET_N_{t} {n}\n")
        f.write("#endif\n")
    with open(dest / "preset_pack.h", "w", encoding="utf-8") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only\n"
                " * GENERATED by tools/gen_preset_pack.py -- do not edit.\n"
                " * The factory library by difference: engines.c presets_pack_init() rebuilds the preset_t\n"
                " * arrays from it at boot. The format is documented in the tool. */\n")
        f.write("#ifndef QG_PRESET_PACK_H\n#define QG_PRESET_PACK_H\n")
        f.write('#include "preset_counts.h"\n')
        f.write(f"#define PRESET_PACK_N {len(entry)}\n")
        f.write(f"#define PRESET_BLOB_N {len(blob)}\n")
        f.write("/* the engines, in the order PRESET_PACK lists them (engines.c builds its array names from this) */\n")
        f.write("#define QG_PRESET_TABLES(X) \\\n")
        f.write(" \\\n".join("    X(%s)" % t for t in TABLES) + "\n")
        f.write("typedef struct { uint16_t dflt, bits, vals, nm; uint8_t n; } preset_pack_t;\n")
        f.write("static const uint8_t PRESET_BLOB[%d] = {\n" % len(blob))
        for i in range(0, len(blob), 20):
            f.write("    " + ",".join(str(b) for b in blob[i:i + 20]) + ",\n")
        f.write("};\n")
        f.write("static const preset_pack_t PRESET_PACK[%d] = {\n" % len(entry))
        for dflt, bits, vals, nm, n in entry:
            f.write(f"    {{{dflt}, {bits}, {vals}, {nm}, {n}}},\n")
        f.write("};\n#endif\n")
    print("gen_preset_pack: blob %d B in %s (tables: raw %d B, saved %d B)"
          % (len(blob), dest / "preset_pack.h", PRESET_SIZE * sum(n for _, n in counts),
             PRESET_SIZE * sum(n for _, n in counts) - len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
