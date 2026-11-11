#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The one decision that authorises writing to an FM-1, shared by both installers.

`tools/fm1_install.py` (CLI) and `web/fm1ota.js` (the page) both ask this module
before they send the frame that starts an update. It is deliberately a small,
pure function: no MIDI, no files, no environment reads except the explicit `lab`
argument, so it can be tested on its own.

The rule is evidence, not memory: a write is allowed only when the caller can
present the physical NOR identity of *this* unit, read from the device with 0x12
in the same session, and that identity is in the documented allowlist. There is
no argument that supplies a chip and no lab switch: a JEDEC typed by hand is not
evidence of the part in front of the installer.
"""
import re

# A part can be READ for diagnostics as soon as it is identified. Only a part with a
# record of "identified, read, written and verified on an FM-1" may be WRITTEN: evidence
# for a new part means that record, not a datasheet and not another project's allowlist.
PRODUCTION_JEDEC = {
    0x856014: "Puya P25Q80H (1 MiB)",
    # docs/JEDEC_COMPATIBILITY.md: the same 1 MiB geometry, page/sector/block sizes, command set
    # (9F/05/35/06/02/20/D8/03/0B) and status bits as the Puya part, from UCUN's datasheet, and a
    # third-party FM-1 with this id took a full firmware install and kept settings and projects
    # across a power cycle. The bootloader column stays false: this project never writes it, and
    # the one field report of a UCUN unit that did not boot was a bootloader install.
    0xB36014: "UCUN UC25HQ80 (1 MiB)",
}
# Identified and readable, never written here. 1 MiB SPI NOR chips known in the FM-1 and AC791N
# ecosystem: non-destructive diagnostics and backup are permitted; writes are strictly rejected
# until a lab unit and physical test proves write and power-cut cycle safety.
READ_ONLY_JEDEC = {
    0xC84014: "GigaDevice GD25Q80 (1 MiB)",
    0xEF4014: "Winbond W25Q80 (1 MiB)",
    0x5E4014: "Zbit ZB25VQ80 (1 MiB)",
    0x684014: "BoyaMicro BY25D80 (1 MiB)",
    0x204014: "XMC XM25QH80B (1 MiB)",
}
KNOWN_JEDEC = {**PRODUCTION_JEDEC, **READ_ONLY_JEDEC}
MAX_PACKAGE = 0x100000        # the part is 1 MiB; the image is the whole flash

# The four questions are answered separately, because "readable" never implies "writable" and a write
# another project performed never implies one this project validated. docs/M-FLASHER.md §5 explains
# each cell; this table is what the tools answer, so the two cannot drift apart.
CAPABILITIES = {
    0x856014: {"part": "Puya P25Q80H", "read": True, "backup": True, "write_app": True,
               "bootloader": True, "qg_install": True,
               "basis": "documented OEM part; the write path is the one every test in this repo drives"},
    0xB36014: {"part": "UCUN UC25HQ80", "read": True, "backup": True, "write_app": True,
               "bootloader": False, "qg_install": True,
               "basis": "UCUN datasheet: same 1 MiB geometry, 256 B page, 4 KiB sector, 64 KiB block, "
                        "02/20/D8/05/06/9F commands and WIP/WEL bits; a third-party FM-1 with this id "
                        "installed a full firmware image and kept settings and a project across a power "
                        "cycle (Felucca #320). The bootloader is never written by this project"},
    0xC84014: {"part": "GigaDevice GD25Q80", "read": True, "backup": True, "write_app": False,
               "bootloader": False, "qg_install": False,
               "basis": "Known 1 MiB part in JieLi reference designs; read/backup supported, write blocked until hardware verified"},
    0xEF4014: {"part": "Winbond W25Q80", "read": True, "backup": True, "write_app": False,
               "bootloader": False, "qg_install": False,
               "basis": "Standard 1 MiB SPI NOR; read/backup supported, write blocked until hardware verified"},
    0x5E4014: {"part": "Zbit ZB25VQ80", "read": True, "backup": True, "write_app": False,
               "bootloader": False, "qg_install": False,
               "basis": "1 MiB SPI NOR; read/backup supported, write blocked until hardware verified"},
    0x684014: {"part": "BoyaMicro BY25D80", "read": True, "backup": True, "write_app": False,
               "bootloader": False, "qg_install": False,
               "basis": "1 MiB SPI NOR; read/backup supported, write blocked until hardware verified"},
    0x204014: {"part": "XMC XM25QH80B", "read": True, "backup": True, "write_app": False,
               "bootloader": False, "qg_install": False,
               "basis": "1 MiB SPI NOR; read/backup supported, write blocked until hardware verified"},
}


def capabilities(jedec):
    """-> the capability row for a part, or None when the id is not documented here."""
    return CAPABILITIES.get(jedec)


def matrix():
    """-> the rows as text, for a terminal or a document that wants to quote the code."""
    cols = ("read", "backup", "write_app", "bootloader", "qg_install")
    out = ["JEDEC     part             " + " ".join("%s" % c for c in cols)]
    for j, row in sorted(CAPABILITIES.items()):
        out.append("0x%06X  %-16s " % (j, row["part"]) + " ".join(
            ("yes" if row[c] else "no ").ljust(len(c)) for c in cols))
    return "\n".join(out)


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
    """-> the chip description when the evidence is complete and the part may be written."""
    if jedec is None:
        raise Refusal("chip", "chip_not_identified",
                      "the physical NOR was not identified on this unit: 0x12 has to answer with a part "
                      "in the documented allowlist before anything is written")
    try:
        jedec = int(jedec)
    except (TypeError, ValueError):
        raise Refusal("chip", "bad_chip_evidence", f"{jedec!r} is not a JEDEC id")
    if jedec in (0xFFFFFF, 0x000000, 0xFFFFFFFF):
        raise Refusal("chip", "chip_not_identified",
                      f"the flash answered {jedec:#08x}: no usable JEDEC")
    if jedec in READ_ONLY_JEDEC:
        raise Refusal("chip", "chip_read_only",
                      f"JEDEC {jedec:#08x} ({READ_ONLY_JEDEC[jedec]}) is not validated for writing: "
                      "read it, back it up, and do not install on it until a lab unit proves the write")
    if jedec not in PRODUCTION_JEDEC:
        raise Refusal("chip", "chip_unknown",
                      f"JEDEC {jedec:#08x} is not in the documented 1 MiB allowlist")
    return {"jedec": f"0x{jedec:06X}", "part": PRODUCTION_JEDEC[jedec]}


def authorise(product, identity_text, jedec=None):
    """-> {'allowed', 'reason', 'code', 'detail', 'chip'} without raising, for reports."""
    try:
        check_identity(product, identity_text)
        chip = check_chip(jedec)
        return {"allowed": True, "reason": "identified", "code": None, "detail": "", "chip": chip}
    except Refusal as e:
        return {"allowed": False, "reason": e.reason, "code": e.code, "detail": e.detail, "chip": None}


def require(product, identity_text, jedec=None):
    """Raise Refusal unless this exact unit may be written. Called right before UPGRADE."""
    d = authorise(product, identity_text, jedec)
    if not d["allowed"]:
        raise Refusal(d["code"], d["reason"], d["detail"])
    return d


if __name__ == "__main__":
    import sys
    print(matrix())
    sys.exit(0)
