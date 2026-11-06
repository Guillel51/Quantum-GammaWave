#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Compare the .fwsc (and loader) this checkout builds with another build of the same commit, either
# a directory of CI artefacts or a sha256sum file. Exits 1 on any difference, naming the file.
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WANTED = ("felucca.bin", "ota.bin", "felucca.fwsc")


def digests(base):
    out = {}
    for p in Path(base).rglob("*"):
        if p.is_file() and p.name in WANTED:
            out[p.name] = (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_size)
    return out


def main():
    if len(sys.argv) != 2:
        print("usage: tools/compare_build.py <artefact dir | sha256sum file>")
        return 2
    other = Path(sys.argv[1])
    if other.is_file():                              # a sha256sum listing
        ref = {}
        for line in other.read_text().splitlines():
            bits = line.split()
            if len(bits) >= 2 and Path(bits[-1]).name in WANTED:
                ref[Path(bits[-1]).name] = (bits[0], None)
    else:
        ref = digests(other)
    here = digests(ROOT / "build")
    if not here or not ref:
        print(f"nothing to compare: local {sorted(here)}, other {sorted(ref)}")
        return 2
    bad = 0
    for name in WANTED:
        a, b = here.get(name), ref.get(name)
        if not a or not b:
            print(f"{name:<14} only on one side: local={bool(a)} other={bool(b)}")
            bad += 1
        elif a[0] != b[0]:
            print(f"{name:<14} DIFFERS\n  local {a[0]} ({a[1]} B)\n  other {b[0]}")
            bad += 1
        else:
            print(f"{name:<14} {a[0]} identical")
    print("identical builds" if not bad else f"{bad} difference(s): not reproducible end to end")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
