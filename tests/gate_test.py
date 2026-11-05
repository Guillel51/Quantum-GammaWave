#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Host tests for tools/fm1_gate.py: the one decision both installers ask before writing.

The point is the policy, not the plumbing: a part that has not been validated with a
lab unit must be readable and refused for writing, and no identity mistake may pass.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
import fm1_gate as G

fails = 0


def check(what, ok):
    global fails
    print(f"{what:<74} {'ok' if ok else 'FAIL'}")
    if not ok:
        fails += 1


def refused(product, identity, jedec, reason, what):
    try:
        G.require(product, identity, jedec)
    except G.Refusal as e:
        check(what, e.reason == reason)
        return
    check(what, False)


check("gate: P25Q80H is the validated write part",
      G.check_chip(0x856014)["part"].startswith("Puya"))
refused("FM-1_982", "FM-1_982", 0xB36014, "chip_read_only",
        "gate: UCUN 0xB36014 is refused for writing (read-only until a lab unit proves it)")
refused("FM-1_982", "FM-1_982", 0x123456, "chip_unknown", "gate: an unknown JEDEC is refused")
refused("FM-1_982", "FM-1_982", None, "chip_not_identified", "gate: no evidence is refused")
refused("FM-1_982", "FM-1_982", 0xFFFFFF, "chip_not_identified", "gate: a blank answer is refused")
refused("FM-1_982", "GW-1_982", 0x856014, "model_mismatch", "gate: another model is refused")
refused("FM-1_982", "FM-1", 0x856014, "inconsistent_identity",
        "gate: a malformed identity is refused")
refused("FM-1_982", "FM-1_982", 0x1000000, "chip_unknown",
        "gate: an id wider than the JEDEC field is refused")

d = G.authorise("FM-1_982", "FM-1_982", 0xB36014)
check("gate: authorise() reports chip_read_only without raising",
      d["allowed"] is False and d["reason"] == "chip_read_only" and d["chip"] is None)
d = G.authorise("FM-1_982", "FM-1_982", 0xB36014, lab=True)
check("gate: the lab override says so and is never silent",
      d["allowed"] is True and d["reason"] == "lab" and "lab override" in d["detail"])
d = G.authorise("FM-1_982", "ota-FM-1_982", 0x856014)
check("gate: the loader's identity names the same model",
      d["allowed"] is True and d["reason"] == "identified" and d["chip"]["jedec"] == "0x856014")

print(f"gate tests: {'FAILED' if fails else 'passed'}")
sys.exit(1 if fails else 0)
