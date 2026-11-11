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
check("gate: UCUN 0xB36014 is a write part too (datasheet parity plus a field install)",
      G.check_chip(0xB36014)["part"].startswith("UCUN"))
check("gate: the bootloader is never a write target, on either part",
      G.capabilities(0x856014)["bootloader"] is True and G.capabilities(0xB36014)["bootloader"] is False)
refused("FM-1_965", "FM-1_965", 0x123456, "chip_unknown", "gate: an unknown JEDEC is refused")
refused("FM-1_965", "FM-1_965", None, "chip_not_identified", "gate: no evidence is refused")
refused("FM-1_965", "FM-1_965", 0xFFFFFF, "chip_not_identified", "gate: a blank answer is refused")
refused("FM-1_965", "GW-1_982", 0x856014, "model_mismatch", "gate: another model is refused")
refused("FM-1_965", "FM-1", 0x856014, "inconsistent_identity",
        "gate: a malformed identity is refused")
refused("FM-1_965", "FM-1_965", 0x1000000, "chip_unknown",
        "gate: an id wider than the JEDEC field is refused")

d = G.authorise("FM-1_965", "FM-1_965", 0xB36014)
check("gate: authorise() allows the UCUN part for an identified unit",
      d["allowed"] is True and d["reason"] == "identified" and d["chip"]["jedec"] == "0xB36014")
d = G.authorise("FM-1_965", "FM-1_965", 0x123456)
check("gate: there is no lab override, so an unknown part is still refused",
      d["allowed"] is False and d["code"] == "chip" and d["reason"] == "chip_unknown")
check("gate: the capability table and the write allowlist are the same decision",
      all(G.capabilities(j)["write_app"] == (j in G.PRODUCTION_JEDEC) for j in G.CAPABILITIES)
      and set(G.CAPABILITIES) == set(G.KNOWN_JEDEC))
d = G.authorise("FM-1_965", "ota-FM-1_965", 0x856014)
check("gate: the loader's identity names the same model",
      d["allowed"] is True and d["reason"] == "identified" and d["chip"]["jedec"] == "0x856014")

print(f"gate tests: {'FAILED' if fails else 'passed'}")
sys.exit(1 if fails else 0)
