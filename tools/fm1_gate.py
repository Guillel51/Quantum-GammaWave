#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The one decision that authorises writing to an FM-1, shared by both installers.

`tools/fm1_install.py` (CLI) and `web/fm1ota.js` (the page) both ask this module
before they send the frame that starts an update. It is deliberately a small,
pure function: no MIDI, no files, no environment reads except the explicit `lab`
argument, so it can be tested on its own.

The rule is evidence, not memory: a write is allowed only when the caller can
present the physical NOR identity of *this* unit, read from the device or with an
external reader (`--jedec`), and that identity is in the documented allowlist.
Nothing in the current protocol reports it, so in practice the answer today is
NO-GO and the installers refuse to start.
"""
import re

# The documented 1 MiB parts. Evidence for a new part means a unit that was
# identified, read, written and verified; add it here with that record.
KNOWN_JEDEC = {
    0x856014: "Puya P25Q80H (1 MiB)",
    0xB36014: "UCUN 1 MiB",
}
MAX_PACKAGE = 0x100000        # the part is 1 MiB; the image is the whole flash


class Refusal(Exception):
    def __init__(self, code, reason, detail=""):
        super().__init__(reason)
        self.code, self.reason, self.detail = code, reason, detail


def model_of(text):
    return (text or "").split("_")[0].removeprefix("ota-")


def check_identity(product, identity_text):
    """-> the model both sides agree on; raises when they do not."""
    if not product or not re.fullmatch(r"[^_]+_\d+", product):
        raise Refusal("badpkg", "bad_identity", f"package identity {product!r}")
    if not identity_text or not re.fullmatch(r"(ota-)?[^_]+_\d+", identity_text):
        raise Refusal("identity", "inconsistent_identity", repr(identity_text))
    if model_of(identity_text) != model_of(product):
        raise Refusal("model", "model_mismatch", f"device {identity_text} vs package {product}")
    return model_of(product)


def check_chip(jedec):
    """-> the chip description when the evidence is complete and known."""
    if jedec is None:
        raise Refusal("chip", "chip_not_identified",
                      "the physical NOR was not identified: the identity packet carries the model "
                      "only and the protocol has no host-initiated flash read. Read the JEDEC with an "
                      "external reader (--jedec) or wait for the loader command")
    try:
        jedec = int(jedec)
    except (TypeError, ValueError):
        raise Refusal("chip", "bad_chip_evidence", f"{jedec!r} is not a JEDEC id")
    if jedec in (0xFFFFFF, 0x000000, 0xFFFFFFFF):
        raise Refusal("chip", "chip_not_identified",
                      f"the flash answered {jedec:#08x}: no usable JEDEC")
    if jedec not in KNOWN_JEDEC:
        raise Refusal("chip", "chip_unknown",
                      f"JEDEC {jedec:#08x} is not in the documented 1 MiB allowlist")
    return {"jedec": f"0x{jedec:06X}", "part": KNOWN_JEDEC[jedec]}


def authorise(product, identity_text, jedec=None, lab=False):
    """-> {'allowed', 'reason', 'code', 'detail', 'chip'} without raising, for reports."""
    try:
        check_identity(product, identity_text)
        chip = check_chip(jedec)
        if lab:
            return {"allowed": True, "reason": "lab", "code": None,
                    "detail": "lab override: the chip was not identified", "chip": chip}
        return {"allowed": True, "reason": "identified", "code": None, "detail": "", "chip": chip}
    except Refusal as e:
        if lab and e.code == "chip":
            return {"allowed": True, "reason": "lab", "code": None,
                    "detail": "lab override: the chip was not identified", "chip": None}
        return {"allowed": False, "reason": e.reason, "code": e.code, "detail": e.detail, "chip": None}


def require(product, identity_text, jedec=None, lab=False):
    """Raise Refusal unless this exact unit may be written. Called right before UPGRADE."""
    d = authorise(product, identity_text, jedec, lab)
    if not d["allowed"]:
        raise Refusal(d["code"], d["reason"], d["detail"])
    return d
