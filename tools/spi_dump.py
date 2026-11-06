#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify a flash dump taken with an external SPI reader (two passes), without the device.

`tools/fm1_backup.py` needs 0x13, which exists only inside our firmware: on a stock unit there is no
read path over USB-MIDI. This tool covers the other route, an SOIC clip or a socket programmer
(flashrom's ch341a_spi, a Bus Pirate, a dedicated reader), and it only ever looks at files. It cannot
and does not talk to hardware.

  spi_dump.py --a passA.bin --b passB.bin --out DIR [--jedec 0x856014] [--map regions.json]

Two passes are not decoration: a single read that silently repeats a page, or returns 0xFF where the
boot header must be, looks exactly like a valid dump. Requiring two independent reads to agree, and
then looking for the failure patterns that still survive agreement, is the difference between an
archive and a file.

Exit: 0 = usable dump, 1 = rejected, 2 = usage or unknown part.
"""
import argparse
import hashlib
import json
import sys
import time
from pathlib import Path

PAGE = 256
BLOCK = 0x10000
ALLOWED_JEDEC = {0x856014: "P25Q80H (documented OEM part)", 0xB36014: "P25Q80H variant (documented)"}
DEFAULT_CAPACITY = 1 << 20
SPARE = 5


def sha(b):
    return hashlib.sha256(b).hexdigest()


def find_repeats(img, min_run=8):
    """A long run of byte-identical pages that are not a constant fill: the classic reader fault.

    Two equal pages prove nothing — flash holds repeated tables and zeroed settings for real. A page
    the reader re-sent hundreds of times does, and a constant 0x00/0xFF run is a legitimate empty
    region, so neither of those is what this looks for.
    """
    hits = []
    run_start, run_len, prev = None, 0, None
    for off in range(0, len(img), PAGE):
        page = img[off:off + PAGE]
        run_len = run_len + 1 if prev is not None and page == prev else 1
        if run_len == 1:
            run_start = off
        prev = page
        if run_len == min_run and page != b"\x00" * PAGE and page != b"\xff" * PAGE:
            hits.append(run_start)
    return hits


def find_blank(img, app_end):
    """0xFF pages below the end of the application: erased where the image should be."""
    return [o for o in range(0, min(app_end, len(img)), PAGE) if img[o:o + PAGE] == b"\xff" * PAGE]


def block_hashes(img):
    return {f"0x{o:06X}": sha(img[o:o + BLOCK]) for o in range(0, len(img), BLOCK)}


def region_hashes(img, regions):
    out = {}
    for name, spec in regions.items():
        lo, hi = spec["from"], spec["to"]
        if not (0 <= lo < hi <= len(img)):
            out[name] = "out of range"
            continue
        out[name] = {"from": f"0x{lo:06X}", "to": f"0x{hi:06X}", "bytes": hi - lo,
                     "sha256": sha(img[lo:hi])}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="first pass, raw dump")
    ap.add_argument("--b", required=True, help="second pass, raw dump")
    ap.add_argument("--out", default="backup")
    ap.add_argument("--jedec", help="the id the reader reports, e.g. 0x856014")
    ap.add_argument("--capacity", type=lambda s: int(s, 0), default=DEFAULT_CAPACITY)
    ap.add_argument("--app-end", type=lambda s: int(s, 0), default=None,
                    help="where the application image ends; erased pages below it are a failure."
                         " Omit it when the layout of this part is not known: guessing it turns a"
                         " good dump into a rejected one")
    ap.add_argument("--map", help="JSON {name: {from: addr, to: addr}} for per-region hashes")
    a = ap.parse_args()

    a_img, b_img = Path(a.a).read_bytes(), Path(a.b).read_bytes()
    bad = []

    if a.jedec:
        jid = int(a.jedec, 0)
        if jid not in ALLOWED_JEDEC:
            print(f"REJECT unknown part: {jid:#08x} is not in the allowlist "
                  f"({', '.join(hex(k) for k in ALLOWED_JEDEC)})")
            return 2
        print(f"part: {jid:#08x} — {ALLOWED_JEDEC[jid]}")

    for name, img in (("A", a_img), ("B", b_img)):
        if len(img) != a.capacity:
            bad.append(f"pass {name} is {len(img)} B, expected {a.capacity} B")
    if a_img != b_img and len(a_img) == len(b_img):
        diffs = [i for i in range(len(a_img)) if a_img[i] != b_img[i]]
        bad.append(f"passes differ in {len(diffs)} byte(s), first at 0x{diffs[0]:06X}, "
                   f"last at 0x{diffs[-1]:06X}")
    img = a_img

    reps = find_repeats(img)
    if reps:
        bad.append(f"{len(reps)} run(s) of {8}+ identical pages (reader fault?): "
                   + ", ".join(f"0x{o:06X}" for o in reps[:SPARE]))
    blanks = find_blank(img, min(a.app_end, len(img))) if a.app_end else []
    if blanks:
        bad.append(f"{len(blanks)} erased page(s) below the application end: "
                   + ", ".join(f"0x{o:06X}" for o in blanks[:SPARE]))

    Path(a.out).mkdir(parents=True, exist_ok=True)
    ident = "fm1-dump"
    stamp = time.strftime("%Y-%m-%d")
    out_bin = Path(a.out) / f"{ident}-{stamp}-a.bin"
    out_bin.write_bytes(img)

    man = {
        "tool": "tools/spi_dump.py",
        "source": "external SPI reader, two passes",
        "date": stamp,
        "capacity": len(img),
        "jedec": (a.jedec or None),
        "identity": None,
        "sha256": sha(img),
        "pass_a_sha256": sha(a_img),
        "pass_b_sha256": sha(b_img),
        "passes_identical": a_img == b_img,
        "blocks_64k": block_hashes(img),
        "regions": region_hashes(img, json.loads(Path(a.map).read_text())) if a.map else {},
        "defects": bad,
        "complete": not bad,
    }
    out_man = Path(a.out) / f"{ident}-{stamp}.manifest.json"
    out_man.write_text(json.dumps(man, indent=2) + "\n")

    print(f"image  {out_bin}  {len(img)} B  sha256 {man['sha256']}")
    print(f"blocks {len(man['blocks_64k'])} x 64 KiB hashed")
    for msg in bad:
        print("REJECT " + msg)
    if bad:
        print(f"not a usable archive; manifest kept for the record at {out_man}")
        return 1
    print("two passes identical, no erased hole below the application, no repeated page")
    print("this dump is a candidate archive — it is still not a tested restore")
    return 0


if __name__ == "__main__":
    sys.exit(main())
