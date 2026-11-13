#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""What boot payload two .fwsc packages carry, and whether it is the same one.

`uboot.boot` is the SPL that travels inside every package and is written to the boot block of the unit
that installs it. The manifests name the SDK file each build read, which is a claim; this reads the
packages themselves and reports what is in them, so two releases can be compared without trusting a JSON.

    python3 tools/spl_diff.py release/felucca-v2.9.0.fwsc release/felucca-v2.9.2.fwsc

Prints, per package: the boot table entry (offset, length, CRC16), the SHA-256 of the stored container,
and the BANKCB payload's length and SHA-256 when the container parses. Then it compares the payloads:
common prefix and suffix and the size of the region that differs. It does not claim the two payloads are
functionally equivalent or not: that needs the SPL's source or a unit, neither of which is here.
"""
import argparse
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_image as IMG            # noqa: E402
import fm1_mflasher_accept as MFA  # noqa: E402


class Package(bytes):
    """parse_package wants bytes with a name; the name is only used for its progress lines."""

    name = ""


def read(path):
    p = Package(Path(path).read_bytes())
    name = Path(path).name
    p.name = name
    flash = MFA.parse_package(p)["flash"]
    entry = next((e for e in IMG.boot_table(flash) if e["name"] == "uboot.boot"), None)
    if entry is None:
        raise SystemExit(f"{name}: the boot table names no uboot.boot")
    spl = flash[entry["addr"]:entry["addr"] + entry["len"]]
    payload = IMG.bankcb(spl)
    return name, entry, spl, payload


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("packages", nargs="+", metavar="PACKAGE.fwsc")
    a = ap.parse_args(argv)
    rows = []
    for path in a.packages:
        name, e, spl, payload = read(path)
        print(f"{name}: uboot.boot at {e['addr']:#06x}, {e['len']} B, crc16 {e['crc']:#06x}, "
              f"sha256 {hashlib.sha256(spl).hexdigest()}")
        if payload is None:
            print("  the BANKCB container does not parse (header CRC or payload CRC)")
        else:
            print(f"  payload {len(payload)} B, sha256 {hashlib.sha256(payload).hexdigest()}")
        rows.append((name, spl, payload))
    if len(rows) > 1:
        print("comparison against %s:" % rows[0][0])
        for name, spl, payload in rows[1:]:
            same = "the same bytes" if spl == rows[0][1] else "different bytes"
            print(f"  {name}: the stored container is {same} "
                  f"({len(spl)} vs {len(rows[0][1])} B)")
            p0, p1 = rows[0][2], payload
            if p0 is None or p1 is None:
                print("    the payloads cannot be compared: one of the containers does not parse")
                continue
            i = 0
            while i < min(len(p0), len(p1)) and p0[i] == p1[i]:
                i += 1
            j = 0
            while j < min(len(p0), len(p1)) - i and p0[-1 - j] == p1[-1 - j]:
                j += 1
            print(f"    payloads {len(p0)} vs {len(p1)} B, common prefix {i} B, common suffix {j} B, "
                  f"the region between them is {len(p0) - j - i} vs {len(p1) - j - i} B")
            print("    a size change in the payload is not a re-container: the boot code itself differs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
