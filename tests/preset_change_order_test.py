#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""A preset change on a SILENT track must not reset the instrument-wide audio state.

Reported: changing the preset from GLO MIXER interrupts the audio even when the preset being selected is
not playing. Two global subsystems were being reset for every preset change:

  * the FX buses (`fx_preset_flush`): fixed first, gated on the track having a voice;
  * the performance layer (`perf_reset`): it latches `perf_kill` (the ISR plays the FX map as empty for a
    block), clears the four knob macros, the MACRO latch and every track's punch-in buffer.

The fix is the gate itself, and it is what this check reads: in `apply_preset_to`, both calls must sit inside
the same `if (any)` where `any` is "this track has an active voice", and neither may appear unguarded.

Class: static check of this tree. What it cannot do is hear the result: that needs the unit.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ui = (ROOT / "firmware/src/ui.c").read_text()
bad = []

i = ui.find("static void apply_preset_to(track_t *t, uint32_t pi)")
if i < 0:
    bad.append("apply_preset_to is gone: this check needs rewriting")
else:
    body = ui[i:ui.find("\nstatic void set_engine_of", i)]
    # the block that owns both resets
    m = re.search(r"if \(any\) \{\s*\n\s*perf_reset\(\);\s*\n\s*fx_preset_flush\(\);\s*\n\s*\}", body)
    if not m:
        bad.append("perf_reset() and fx_preset_flush() are not both inside the `if (any)` gate")
    if re.search(r"(?<!if \(any\) \{\n        )perf_reset\(\);", body) is None and not m:
        bad.append("perf_reset() is not reachable in apply_preset_to")
    # `any` must be "this track has a voice"
    if "any |= V(t, i).active;" not in body:
        bad.append("the gate does not read this track's own voices")
    # and nothing may call the global reset outside that gate
    for name in ("perf_reset", "fx_preset_flush"):
        for mm in re.finditer(name + r"\(\);", body):
            start = body.rfind("if (any) {", 0, mm.start())
            if start < 0 or body.find("}", start) < mm.start():
                bad.append(f"{name}() is called outside the `if (any)` gate")

for label, ok in (("gate present", not any("not both inside" in b for b in bad)),
                  ("this track's voices", not any("own voices" in b for b in bad)),
                  ("no unguarded reset", not any("outside the" in b for b in bad))):
    print(f"preset change: {label:22s} {'ok' if ok else 'FAIL'}")
print(f"preset change: {'FAILED' if bad else 'ok'}" + ("" if not bad else ": " + "; ".join(bad)))
sys.exit(1 if bad else 0)
