#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Validate a raw flash backup of an FM-1, and compare it with the unit in front of you.

`fm1_backup.py` writes DIR/<identity>-<date>.bin plus a manifest: two independent passes, their
SHA-256, the part the device identified with 0x12, the frames that were sent. This tool is the other
half: it checks that archive before anyone relies on it, and it can read the unit again and say
exactly which ranges differ.

  fm1_restore.py --backup DIR-or-FILE            validate the archive on its own (no MIDI)
  fm1_restore.py --backup DIR-or-FILE --device   validate it and compare with the connected FM-1
  fm1_restore.py --backup DIR-or-FILE --compare  read the whole part again and list the differences
  fm1_restore.py --backup DIR-or-FILE --write    refused: see below

WHY --write IS REFUSED, and not hidden: the update path (web/fm1ota.js, tools/fm1_install.py) serves a
*felucca package* to the loader, which reads the logical image and writes the firmware. Nothing in
that protocol has been shown to accept an arbitrary 1 MiB dump, and no raw write has been verified on
a bench here. A tool that offered one would be a way to brick a unit while looking like recovery.
Restoring a backup needs either the vendor's bootloader path or an external programmer, both
documented in docs/RECOVERY.md; until one is demonstrated on a lab unit, this tool stops before the
first frame that could write.

What it does verify, so a backup is worth something: the archive is complete (both passes ran, are
byte-identical, and cover the whole part), the file's SHA-256 is the manifest's, the manifest's chip
and geometry are an identified 1 MiB part, and the recorded frames were read-only (0x11/0x12/0x13).

Exit codes: 0 the archive is usable (and, with --compare, matches), 1 the archive is not usable,
2 bad arguments, 3 the FM-1 was not found, 4 the comparison found differences, 5 a write was asked
for (refused), 6 the unit does not match the backup's chip or geometry.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import fm1_probe as P

FORMAT = "qg1-backup-1"
CAP = 0x100000
READ_ONLY = {0x11, 0x12, 0x13}
IDENTITY_RE = re.compile(r"^(ota-)?[^_]+_\d+$")
MANIFEST_NAME = re.compile(r"\.manifest\.json$")


class RestoreError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def find_backup(path):
    """-> (manifest_path, bin_path) from a .manifest.json, a .bin, or a directory holding one"""
    p = Path(path)
    if p.is_dir():
        mans = sorted(p.glob("*.manifest.json"))
        if len(mans) != 1:
            raise RestoreError("archive", f"{p}: expected exactly one *.manifest.json, found {len(mans)}")
        p = mans[0]
    if MANIFEST_NAME.search(str(p)):
        man_path = p
        try:
            man = json.loads(p.read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            raise RestoreError("archive", f"{p}: cannot read the manifest ({e})")
        name = (man.get("files") or {}).get("pass_a")
        if not name:
            raise RestoreError("archive", f"{p}: the manifest names no pass A file")
        return p, p.parent / name
    return None, p


def validate(man, raw, bin_path):
    """-> the facts a restore may rely on; raises RestoreError with the first thing that is wrong"""
    def need(cond, code, msg):
        if not cond:
            raise RestoreError(code, msg)

    need(isinstance(man, dict), "archive", "the manifest is not an object")
    need(man.get("format") == FORMAT, "format", f"manifest format {man.get('format')!r}, expected {FORMAT!r}")
    status = man.get("status")
    need(status == "complete", "incomplete",
         f"the backup is {status!r}, not 'complete': only a complete archive can be restored "
         f"(unread ranges: {man.get('unread_ranges') or 'none'})")
    need(man.get("identical") is True, "incomplete", "the two passes are not marked identical")
    need(man.get("passes") == 2, "incomplete", "the manifest does not record two passes")
    size = man.get("size")
    need(size == len(raw), "size", f"the file is {len(raw)} bytes, the manifest says {size}")
    need(size == man.get("expected_size") == CAP, "size",
         f"the image is {size} bytes, expected {man.get('expected_size')} / {CAP} (a full 1 MiB part)")
    sha = hashlib.sha256(raw).hexdigest()
    need(sha == man.get("sha256") == man.get("sha256_pass_a") == man.get("sha256_pass_b"), "hash",
         "the file's SHA-256 is not the manifest's (damaged, truncated or swapped file)")
    dev = man.get("device") or {}
    need(dev.get("capacity") == CAP, "geometry", f"capacity {dev.get('capacity')} is not the 1 MiB part")
    need(dev.get("jedec") in {f"0x{j:06X}" for j in P.PARTS}, "chip",
         f"JEDEC {dev.get('jedec')!r} is not a part this project knows")
    need(dev.get("identity") and IDENTITY_RE.match(str(dev["identity"])), "identity",
         f"device identity {dev.get('identity')!r} is not an FM-1 identity")
    sent = man.get("commands_sent")
    need(isinstance(sent, list) and sent, "readonly", "the manifest does not record the frames that were sent")
    need(all(c in READ_ONLY for c in sent), "readonly",
         f"the backup recorded commands {sent}, outside read-only 0x11/0x12/0x13")
    for k in ("page", "sector", "block"):
        need(isinstance(dev.get(k), int) and dev[k] > 0, "geometry", f"the manifest has no {k} size")
    return {"sha256": sha, "size": size, "jedec": dev["jedec"], "part": dev.get("part"),
            "identity": dev["identity"], "capacity": dev["capacity"], "sector": dev["sector"],
            "page": dev["page"], "block": dev["block"]}


def compare(link, raw, session, chunk=P.CHUNK):
    """read the whole part through 0x13 and -> [[start, end], ...] of differing ranges"""
    ranges, start = [], None
    off = 0
    while off < len(raw):
        n = min(chunk, len(raw) - off)
        got, errors = P.read_range(link, off, n, session, chunk=n)
        if errors:
            raise RestoreError("read", f"read of 0x{off:X}+{n} failed: {errors[0][2]}")
        for i in range(n):
            if got[i] != raw[off + i]:
                if start is None:
                    start = off + i
            elif start is not None:
                ranges.append([start, off + i])
                start = None
        off += n
    if start is not None:
        ranges.append([start, len(raw)])
    return ranges


def open_device(port=None):
    """-> (link, evidence) from the running app's 0x12; needs mido like the other tools"""
    try:
        import fm1_install as I
    except ImportError as e:
        raise RestoreError("usage", f"needs the installer's MIDI stack ({e})")
    up = I.Updater(I.MidiBackend(), port)
    dev = up.find()
    if not dev:
        raise RestoreError("notfound", "no FM-1 answered on the MIDI ports")
    if dev.id.loader:
        dev.link.close()
        raise RestoreError("notfound", "the FM-1 is in update mode: it cannot identify its NOR. "
                                       "Finish or replug it first")
    try:
        ev = P.probe(dev.link)
    except P.ProbeError as e:
        dev.link.close()
        raise RestoreError("chip", f"the NOR could not be identified ({e.code}: {e})")
    return dev.link, ev


def check_device(facts, ev):
    if f"0x{ev['jedec']:06X}" != facts["jedec"]:
        raise RestoreError("chip", f"this unit is 0x{ev['jedec']:06X}, the backup came from {facts['jedec']}")
    if ev["cap"] != facts["capacity"]:
        raise RestoreError("geometry", f"this unit has {ev['cap']} bytes, the backup {facts['capacity']}")
    for k in ("page", "sector", "block"):
        if ev[k] != facts[k]:
            raise RestoreError("geometry", f"{k} size differs: unit {ev[k]}, backup {facts[k]}")


def run(a):
    man_path, bin_path = find_backup(a.backup)
    if man_path is None:
        raise RestoreError("archive", f"{bin_path}: give the .manifest.json (or a directory with one), "
                                      f"not the bare image: the image alone cannot be verified")
    man = json.loads(man_path.read_text(encoding="utf-8"))
    try:
        raw = bin_path.read_bytes()
    except OSError as e:
        raise RestoreError("archive", f"cannot read {bin_path}: {e.strerror}")
    facts = validate(man, raw, bin_path)

    if a.write:
        raise RestoreError("write", "refused: this tool has no verified raw-write path. An FM-1 is put "
                                    "back with the vendor bootloader or an external programmer "
                                    "(docs/RECOVERY.md); nothing here sends a frame that could write")

    print(f"backup:  {bin_path}")
    print(f"device:  {facts['identity']}  {facts['jedec']} {facts['part']}  {facts['capacity'] // 1024} KiB")
    print(f"sha256:  {facts['sha256']}")
    print(f"passes:  2, identical, status {man['status']}")
    print("archive: usable")
    if not (a.compare or a.device):
        return 0

    link, ev = open_device(a.port)
    try:
        check_device(facts, ev)
        print(f"unit:    JEDEC 0x{ev['jedec']:06X}, {ev['cap'] // 1024} KiB, session {ev['session']:08X}")
        if a.compare:
            diffs = compare(link, raw, ev["session"])
            if diffs:
                print(f"compare: {len(diffs)} range(s) differ")
                for s, e in diffs[:20]:
                    print(f"  0x{s:05X}..0x{e:05X}  ({e - s} bytes)")
                if len(diffs) > 20:
                    print(f"  ... and {len(diffs) - 20} more")
                return 4
            print("compare: the unit reads back byte for byte like the backup")
        else:
            print("device:  the chip and its geometry match the backup")
    finally:
        link.close()
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--backup", required=True, help="a .manifest.json, a directory with one, or a .bin")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    ap.add_argument("--device", action="store_true", help="also check the connected FM-1's chip and geometry")
    ap.add_argument("--compare", action="store_true", help="read the unit again and list differing ranges")
    ap.add_argument("--write", action="store_true", help="refused by design: see the module docstring")
    a = ap.parse_args(argv)
    try:
        return run(a)
    except RestoreError as e:
        print(f"error: {e}", file=sys.stderr)
        return {"archive": 1, "format": 1, "incomplete": 1, "size": 1, "hash": 1, "readonly": 1,
                "identity": 1, "chip": 6, "geometry": 6, "usage": 2, "notfound": 3,
                "read": 1, "write": 5}.get(e.code, 1)


if __name__ == "__main__":
    sys.exit(main())
