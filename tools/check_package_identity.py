#!/usr/bin/env python3
"""Read a .fwsc package and say whose firmware it is, from the bytes themselves.

The hub checks a package by its identity (`FM-1_` plus digits, carried in the first 20 blocks, one
marker byte after each 0x30-byte block: `productOf` in `web/fm1pkg.js`). This tool reads the same
bytes the same way, and adds the two things a release job needs to refuse:

  * the identity must be THIS firmware's (`PRODUCT` in `tools/build.py`, `FM-1_965`);
  * it must not be the identity of another firmware: `FM-1_015` is M-VAVE's V15 and is refused by
    name, and any other number is reported so a human can compare it with the hub's catalogue
    (`tools/check_identity.py` does that comparison online).

It also reports whether the package carries an `ota-FM-1_` loader identity string, and where.

    python3 tools/check_package_identity.py build/felucca.fwsc [--expect FM-1_965]

Exit status: 0 the package is this firmware's own, 1 it is not, 2 the file cannot be read.
"""

import argparse
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
MARK_UNUSED = 0x7D
V15 = "FM-1_015"
IDENT = re.compile(r"^FM-1_\d{3}$")


def product_of(raw):
    if len(raw) < BLOCKS * BLK:
        raise ValueError("not an FM-1 package (shorter than the 20 blocks the identity lives in)")
    out = []
    for i in range(BLOCKS):
        m = raw[i * BLK + KEEP]
        if m != MARK_UNUSED:
            out.append(chr((m - i - 1) & 0xFF))
    return "".join(out)


def expected_product():
    """PRODUCT from tools/build.py, so this tool and the build never disagree."""
    src = (ROOT / "tools" / "build.py").read_text(encoding="utf-8")
    m = re.search(r'^PRODUCT\s*=\s*"([^"]+)"', src, re.M)
    return m.group(1) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("--expect", default=None, help="expected identity (default: PRODUCT in tools/build.py)")
    a = ap.parse_args()

    p = Path(a.package)
    try:
        raw = p.read_bytes()
    except OSError as e:
        print(f"cannot read {p}: {e}")
        return 2

    try:
        ident = product_of(raw)
    except ValueError as e:
        print(f"{p}: {e}")
        return 2

    want = a.expect or expected_product()
    bad = 0
    print(f"package  {p}  {len(raw)} B")
    print(f"sha256   {hashlib.sha256(raw).hexdigest()}")
    print(f"identity {ident!r}")

    if not IDENT.match(ident):
        print(f"FAIL  the identity is not FM-1_<three digits>: the hub will not list it")
        bad += 1
    if ident == V15:
        print(f"FAIL  {V15} is M-VAVE's official V15: never used for a custom package")
        bad += 1
    if want and ident != want:
        print(f"FAIL  the package says {ident!r}, the build says {want!r}")
        bad += 1
    if not bad:
        print(f"ok    this is {want}'s own identity, not another firmware's")

    # the loader string travels inside the loader program: report what is there, do not guess
    found = sorted({m.group(0).decode("ascii", "replace") for m in re.finditer(rb"ota-FM-1_\d{3}", raw)})
    if found:
        print(f"loader   {', '.join(found)} (the identity the update loader answers)")
        foreign = [f for f in found if f != f"ota-{want}"]
        if foreign:
            print(f"note     also carries {foreign}: an inherited string, see docs/FM1_HUB_READINESS.md")
    else:
        print("loader   no ota-FM-1_ pipeline string found in the bytes")

    if len(raw) > 1024 * 1024:
        print(f"FAIL  {len(raw)} B is larger than the FM-1's 1 MiB of SPI NOR")
        bad += 1

    print()
    print("package identity: FAIL" if bad else "package identity: ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
