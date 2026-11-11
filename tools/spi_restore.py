#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check a dump is good enough to restore from, and print the restore plan. It never writes.

Restoring through an external reader is the only route that reaches the bootloader, and that is
exactly why this file refuses to touch hardware: the operator runs flashrom, after reading what this
prints. There is no --yes, no subprocess and no path in this file that programs a chip.

  spi_restore.py --dump DIR [--port ch341a_spi]

Refuses unless the manifest says `complete`, the bin's hash matches the manifest, the part is on the
allowlist and the image is a whole part.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ALLOWED = (0x856014, 0xB36014)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", required=True, help="directory produced by tools/spi_dump.py")
    ap.add_argument("--port", default="ch341a_spi")
    a = ap.parse_args()

    d = Path(a.dump)
    mans = sorted(d.glob("*.manifest.json"))
    bins = sorted(d.glob("*-a.bin"))
    if not mans or not bins:
        print(f"REJECT no manifest/bin pair in {d}")
        return 1
    man = json.loads(mans[-1].read_text())
    img = bins[-1].read_bytes()
    refusals = []

    if not man.get("complete"):
        refusals.append(f"manifest is not complete: {man.get('defects')}")
    if man.get("sha256") != hashlib.sha256(img).hexdigest():
        refusals.append("the bin changed after the manifest was written")
    if not man.get("passes_identical"):
        refusals.append("the two reads disagreed")
    jid = man.get("jedec")
    if not jid:
        refusals.append("no JEDEC in the manifest: the part was never identified")
    elif int(jid, 0) not in ALLOWED:
        refusals.append(f"unknown part {jid}")
    if len(img) != man.get("capacity"):
        refusals.append("image length does not match the recorded capacity")

    for r in refusals:
        print("REJECT " + r)
    if refusals:
        print("this dump must not be flashed: it would replace the bootloader with unverified bytes")
        return 1

    print(f"dump ok: {len(img)} B  sha256 {man['sha256']}  part {jid}")
    print("plan (run by hand, on the unit this dump came from, nothing here executes it):\n")
    print(f"  flashrom -p {a.port} --flash-name                  # must report the same part")
    print(f"  flashrom -p {a.port} -r check.bin && cmp check.bin {bins[-1]}")
    print(f"  flashrom -p {a.port} -w {bins[-1]} -v")
    print(f"  flashrom -p {a.port} -r after.bin && cmp after.bin {bins[-1]}")
    print("\nThe write replaces every byte of the part, bootloader included. If the clip moves or")
    print("power drops mid-write, the unit does not boot from flash: keep the clip on and re-run")
    print("the write, do not power-cycle hoping for a recovery mode that has not been proven.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
