#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Is this FM-1 identity free on The Firmware Hub?

The hub has no reservation: its catalogue is the only register there is, and an identity is taken when a
listed firmware's newest package reports it (`product`) or when it falls inside the range that firmware
has answered over its releases (`ident`, a regex). This reads that catalogue and answers for one
identity, so the choice is measured instead of assumed -- and can be repeated before every release.

    python3 tools/check_identity.py                 # tools/build.py's PRODUCT
    python3 tools/check_identity.py FM-1_965        # a candidate
    python3 tools/check_identity.py --catalog hub.json FM-1_940   # an offline copy

Exit 0: none of the catalogue's own patterns covers it. Exit 1: something does (it is printed). Exit 2:
the catalogue could not be read, so nothing is claimed either way -- fail closed, never "probably free".

The hub's own rules, from https://thefirmwarehub.com/kb/ (read 2026-11-10):
  * the identity starts with `FM-1_` and travels in the package's first 20 blocks, one marker byte after
    each 0x30 block: at most 20 characters, digits after the underscore;
  * `FM-1_9NN` (three digits, starting with 9) is the range the hub and this project's installers resume
    an interrupted update in -- the check below keeps that shape unless --any-form is given;
  * `FM-1_015` is M-VAVE's official V15: never use it, never move off it in a package.
"""
import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CATALOG = "https://thefirmwarehub.com/catalog.json"
# the catalogue refuses a request without a User-Agent (403), which is for a browser, not for this tool
UA = "quantum-gammawave-check-identity/1 (+https://github.com/Guillel51/Quantum-GammaWave)"
# some listings carry no identity of their own and the hub files them under this pattern: it matches
# every number, so it claims none. The hub's own page filters it out for the same reason.
GENERIC = re.compile(r"^FM-1_\\d\{3,8\}$")
RESUMABLE = re.compile(r"^FM-1_9\d\d$")        # the form the update loader's resume recognises
OFFICIAL = "FM-1_015"


def product_in_tree():
    m = re.search(r'^PRODUCT = "([^"]+)"', (ROOT / "tools" / "build.py").read_text(), re.M)
    return m.group(1) if m else None


def load(path):
    if path:
        return json.loads(Path(path).read_text()), str(path), "the file given on the command line"
    req = urllib.request.Request(CATALOG, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r), CATALOG, "fetched now"


def entries(catalog):
    """every listed firmware, as (name, product, ident): both the exact number and the answered range"""
    rows = list(catalog.get("firmwares") or [])
    for key in ("smk37", "cycles", "lxr"):
        rows += [r for r in (catalog.get(key) or []) if isinstance(r, dict)]
    return [(str(f.get("name")), str(f.get("product") or ""), str(f.get("ident") or "")) for f in rows]


def matches(identity, rows):
    """-> (the entries that claim this identity, how many declare none and therefore claim nothing)

    A claim is an exact `product` or a range (`ident`) that covers the identity. The catch-all pattern
    is not a claim: it is a listing the hub could not read an identity from, and the hub itself ignores
    it when a real range matches."""
    hits, unknown = [], 0
    for name, product, ident in rows:
        if product == identity:
            hits.append((name, "product", product))
        elif ident:
            if GENERIC.match(ident):
                unknown += 1
                continue
            try:
                if re.fullmatch(ident, identity):
                    hits.append((name, "ident", ident))
            except re.error:
                pass                                  # a malformed range in the hub's data claims nothing
    return hits, unknown


def main():
    ap = argparse.ArgumentParser(description="is this FM-1 identity free on The Firmware Hub?")
    ap.add_argument("identity", nargs="?", help="FM-1_9NN (default: tools/build.py PRODUCT)")
    ap.add_argument("--catalog", help="a saved catalog.json instead of the live one")
    ap.add_argument("--any-form", action="store_true",
                    help="accept a form other than FM-1_9NN (it then cannot resume an interrupted update)")
    a = ap.parse_args()
    identity = a.identity or product_in_tree()
    if not identity:
        print("check_identity: no identity given and none in tools/build.py", file=sys.stderr)
        return 2
    try:
        catalog, source, how = load(a.catalog)
    except Exception as e:                            # network, DNS, a truncated file: no claim at all
        print(f"check_identity: cannot read the catalogue ({e}): nothing is claimed either way",
              file=sys.stderr)
        return 2
    rows = entries(catalog)
    hits, unknown = matches(identity, rows)
    print(f"identity  {identity}")
    print(f"catalogue {source} ({how}), updated {catalog.get('updated', '?')}, {len(rows)} listing(s)")
    if identity == OFFICIAL:
        print("REFUSED   FM-1_015 is M-VAVE's official V15: a custom package may not use it")
        return 1
    if not re.fullmatch(r"FM-1_\d{1,20}", identity):
        print("REFUSED   not of the form FM-1_<digits> (the package carries it in 20 marker bytes)")
        return 1
    if not a.any_form and not RESUMABLE.match(identity):
        print("REFUSED   not an FM-1_9NN identity: the hub and this installer cannot resume an update "
              "interrupted under it (--any-form overrides, knowing that)")
        return 1
    if hits:
        print("TAKEN     claimed by:")
        for name, why, what in hits:
            print(f"          {name}: {why} {what}")
        return 1
    print("FREE      no listing's product and no listing's answered range covers it")
    if unknown:
        print(f"          ({unknown} listing(s) carry no identity of their own; they match every number "
              "and claim none)")
    print("          (the hub reserves nothing: this is what its catalogue says now, not a promise)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
