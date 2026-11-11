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
  fm1_restore.py --backup DIR-or-FILE --write --programmer file:PATH
  fm1_restore.py --backup DIR-or-FILE --write --programmer flashrom:ch341a_spi

WHY --write NEEDS A PROGRAMMER, and not a MIDI frame: the update path (web/fm1ota.js,
tools/fm1_install.py) serves a *felucca package* to the loader, which writes the application area from
a verified package. Nothing in that protocol has been shown to accept an arbitrary 1 MiB dump, so a
raw restore cannot go over MIDI: it goes through the part's own SPI bus, with the flash held off the
FM-1's own MCU (docs/RECOVERY.md). Two backends implement that here:

  file:PATH       a NOR part simulated on a file (4 KiB erase, 256 B program, program clears bits
                  only). Every sector is erased, programmed and read back, then the whole part is
                  read back again. This is the path the tests drive; it is not hardware.
  flashrom:DEVICE an external SPI programmer through flashrom, which erases, programs and verifies;
                  this tool then reads the whole part back and compares it with the archive.

Neither backend has been run against physical hardware here. --write without --programmer is still
refused, and no MIDI write frame is ever sent.

What it does verify, so a backup is worth something: the archive is complete (both passes ran, are
byte-identical, and cover the whole part), the file's SHA-256 is the manifest's, the manifest's chip
and geometry are an identified 1 MiB part, and the recorded frames were read-only (0x11/0x12/0x13).

Exit codes: 0 the archive is usable (and, with --compare, matches), 1 the archive is not usable,
2 bad arguments, 3 the FM-1 was not found, 4 the comparison found differences, 5 a write was asked
for, 6 the unit does not match the backup's chip or geometry, 8 a write was attempted and the part
does not read back like the archive.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import fm1_image as IMG
import fm1_probe as P

FORMAT = "qg1-backup-1"
SOURCES = ("external-spi", "mflasher-ram-loader")
HEAD = 0x4000          # loader + boot header: unit-specific, never written by default
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
    # Two reads are the rule. M-Flasher's single read is accepted only when the chip's own CRC32 over
    # the whole part agreed with it: that is a second comparison, done by the silicon, not by the host.
    is_chip_verified = (man.get("source") == "mflasher-ram-loader"
                        and "chip's own CRC32" in str(man.get("verified_by") or ""))
    need(man.get("passes") == 2 or (man.get("passes") == 1 and is_chip_verified), "incomplete",
         "the manifest records %r passes and no chip CRC32: two independent reads are the rule"
         % (man.get("passes"),))
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
    need(isinstance(sent, list), "readonly", "the manifest does not record the frames that were sent")
    if man.get("source") in SOURCES:
        # No MIDI phase happened, so there is no frame log to check. What the manifest must prove instead
        # is that nothing wrote: exactly two reads or the chip's own CRC32 over the whole part, no flash
        # write, and no frame. A manifest that claims one of these sources without that receipt is
        # refused like any other bad archive.
        prog = man.get("programmer") or {}
        need(sent == [], "readonly", f"a non-MIDI read recorded MIDI frames: {sent}")
        need(prog.get("flash_writes") in (0, None), "readonly",
             f"the manifest records {prog.get('flash_writes')} bytes written to the flash")
        if man.get("source") == "external-spi":
            need(prog.get("ops") == ["read", "read"], "readonly",
                 f"the programmer's operations are {prog.get('ops')!r}, not exactly two reads")
            need(prog.get("type") in ("file", "flashrom"), "programmer",
                 f"the manifest names no programmer: {prog.get('type')!r}")
        else:
            need(prog.get("type") == "mflasher", "programmer",
                 f"the manifest names no M-Flasher read: {prog.get('type')!r}")
            need(prog.get("ops") == ["read", "read"] or man.get("verified_by"), "readonly",
                 "an M-Flasher archive has to be checked by a second read or the chip's CRC32")
    else:
        need(bool(sent), "readonly", "the manifest does not record the frames that were sent")
        need(all(c in READ_ONLY for c in sent), "readonly",
             f"the backup recorded commands {sent}, outside read-only 0x11/0x12/0x13")
    for k in ("page", "sector", "block"):
        need(isinstance(dev.get(k), int) and dev[k] > 0, "geometry", f"the manifest has no {k} size")
    if man.get("source") in SOURCES:
        # The identity is a fact about the image, not about the JSON: decode the app area and read it
        # there. A stock image has it encrypted, so a plain search is only the fallback for an image
        # this project cannot decode (and then the identity still has to be in the bytes).
        try:
            info = IMG.analyse(raw)
        except IMG.ImageError as e:
            need(dev["identity"].encode() in raw, "identity",
                 f"the image's structure does not decode ({e}) and {dev['identity']} is not in its bytes")
        else:
            need(info["identity"] == dev["identity"], "identity",
                 f"the image decodes to {info['identity'] or 'no identity'}, the manifest says {dev['identity']}")
            need(info["app_status"] == "ok", "image",
                 f"the image's firmware does not verify ({info['app_status']}): not a clean backup")
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


class NorFile:
    """A NOR part simulated on a file: 4 KiB sectors, 256 B pages, programming only clears bits."""

    def __init__(self, path, size=CAP, blank=0xFF):
        self.path, self.size = Path(path), size
        if self.path.exists():
            data = bytearray(self.path.read_bytes())
            if len(data) != size:
                raise RestoreError("programmer", f"{self.path} holds {len(data)} bytes, the part is {size}")
        else:
            data = bytearray([blank]) * size
        self.data, self.erased, self.programmed = data, 0, 0

    def erase(self, addr, sector):
        if addr % sector or addr + sector > self.size:
            raise RestoreError("programmer", f"erase of 0x{addr:05X} is not a {sector} B sector of this part")
        self.data[addr:addr + sector] = b"\xff" * sector
        self.erased += sector

    def program(self, addr, chunk):
        for i, b in enumerate(chunk):
            self.data[addr + i] &= b          # NOR can only turn 1s into 0s
        self.programmed += len(chunk)

    def read(self, addr, n):
        return bytes(self.data[addr:addr + n])

    def save(self):
        tmp = self.path.with_suffix(self.path.suffix + ".tmp")
        tmp.write_bytes(bytes(self.data))
        tmp.replace(self.path)


def write_nor(raw, nor, sector, page, start=0):
    """erase, program and read back sector by sector from `start`; -> receipt."""
    if start % sector:
        raise RestoreError("programmer", f"start 0x{start:05X} is not a {sector} B sector boundary")
    if len(raw) % sector or sector % page or len(raw) > nor.size:
        raise RestoreError("programmer", f"image {len(raw)} B / sector {sector} B / page {page} B do not tile the part")
    for off in range(start, len(raw), sector):
        chunk = raw[off:off + sector]
        nor.erase(off, sector)
        for p in range(0, len(chunk), page):
            nor.program(off + p, chunk[p:p + page])
        got = nor.read(off, len(chunk))
        if got != chunk:
            bad = next(i for i in range(len(chunk)) if got[i] != chunk[i])
            raise RestoreError("program", f"sector 0x{off:05X} does not read back: first difference at "
                                          f"+0x{bad:X}, wrote 0x{chunk[bad]:02X}, read 0x{got[bad]:02X}")
    if nor.read(start, len(raw) - start) != raw[start:]:
        raise RestoreError("program", "the written range does not read back like the image")
    return {"back": "file", "bytes": len(raw) - start, "sectors": (len(raw) - start) // sector,
            "erased": nor.erased, "programmed": nor.programmed, "from": start}


FLASHROM = "flashrom"


def flashrom_restore(programmer, raw):
    """erase+program+verify with flashrom, then read the part back and compare -> receipt"""
    exe = shutil.which(FLASHROM) if not Path(FLASHROM).is_file() else FLASHROM
    if not exe:
        raise RestoreError("programmer", f"{FLASHROM} is not installed: apt-get install flashrom, "
                                         f"or use --programmer file:PATH to exercise the engine")
    with tempfile.TemporaryDirectory(prefix="fm1-restore-") as tmp:
        img, back = Path(tmp) / "image.bin", Path(tmp) / "readback.bin"
        img.write_bytes(raw)
        for args in (("-w", str(img)), ("-r", str(back))):
            r = subprocess.run([exe, "-p", programmer, *args], capture_output=True, text=True)
            if r.returncode:
                tail = (r.stderr or r.stdout).strip().splitlines()[-1:] or ["no output"]
                raise RestoreError("programmer", f"flashrom -p {programmer} {args[0]} failed "
                                                 f"({r.returncode}): {tail[0]}")
        got = back.read_bytes()
    if len(got) != len(raw):
        raise RestoreError("program", f"the part reads back {len(got)} bytes, the archive has {len(raw)}")
    if got != raw:
        bad = next(i for i in range(len(raw)) if got[i] != raw[i])
        raise RestoreError("program", f"the part does not read back like the archive: first difference at "
                                      f"0x{bad:05X}, wrote 0x{raw[bad]:02X}, read 0x{got[bad]:02X}")
    return {"back": "flashrom", "programmer": programmer, "bytes": len(raw), "verified": True}


def restore(a, facts, raw):
    spec = (getattr(a, "programmer", None) or "").strip()
    if not spec:
        raise RestoreError("write", "refused: no programmer. The MIDI protocol has no frame that writes "
                                    "a raw image, so a restore goes through the part's SPI bus: give "
                                    "--programmer file:PATH or --programmer flashrom:DEVICE (docs/RECOVERY.md)")
    kind, _, arg = spec.partition(":")
    if kind == "file":
        if not arg:
            raise RestoreError("programmer", "file: needs a path to the simulated part")
        nor = NorFile(arg, size=facts["capacity"])
        start = 0 if getattr(a, "include_head", False) else HEAD
        rec = write_nor(raw, nor, facts["sector"], facts["page"], start=start)
        nor.save()
        rec["part"] = arg
    elif kind == "flashrom":
        if not getattr(a, "include_head", False):
            raise RestoreError("write", "flashrom writes the whole part, boot header included: "
                                        "pass --include-head to accept that, or use --programmer file:PATH "
                                        "to write from 0x4000")
        rec = flashrom_restore(arg or "ch341a_spi", raw)
    else:
        raise RestoreError("programmer", f"unknown programmer {spec!r}: use file: or flashrom:")
    where = "the whole part" if rec.get("from", 0) == 0 else f"from 0x{rec['from']:05X} (boot header kept)"
    print(f"write:   {rec['bytes']} B, {rec.get('sectors', '-')} sectors {where}, erased, programmed, read back")
    print(f"verify:  the part reads back identical to {facts['sha256'][:16]}...")
    print("restore: written")
    return 0


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
        return restore(a, facts, raw)

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
    ap.add_argument("--write", action="store_true", help="write the image back through --programmer")
    ap.add_argument("--include-head", action="store_true",
                    help="also write the first 16 KiB (loader/boot header): unit-specific, off by default")
    ap.add_argument("--programmer", metavar="SPEC",
                    help="file:PATH (a part simulated on a file) or flashrom:DEVICE (an external SPI programmer)")
    a = ap.parse_args(argv)
    try:
        return run(a)
    except RestoreError as e:
        print(f"error: {e}", file=sys.stderr)
        return {"archive": 1, "format": 1, "incomplete": 1, "size": 1, "hash": 1, "readonly": 1,
                "identity": 1, "chip": 6, "geometry": 6, "usage": 2, "notfound": 3,
                "read": 1, "write": 5, "program": 8, "programmer": 2}.get(e.code, 1)


if __name__ == "__main__":
    sys.exit(main())
