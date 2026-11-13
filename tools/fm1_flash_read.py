#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Read an FM-1's NOR with an external programmer, twice, and write a restorable archive.

This is the only read path that does not need the unit to boot anything: the part is read through a
clip or a socket, so it works on a stock V15 unit whose firmware has never heard of 0x12/0x13
(docs/FLASH_READER.md). It reads the whole part twice with the *same* programmer operation, compares
the two images byte for byte, and writes the same qg1-backup-1 manifest that tools/fm1_restore.py
already validates -- with `source: external-spi` and the programmer operations it ran, in order.

  fm1_flash_read.py --out backup/stock-01 --programmer file:part.bin --jedec 0x856014
  fm1_flash_read.py --out backup/stock-01 --programmer flashrom:ch341a_spi --jedec 0x856014

What it will not do: write, erase, program, set a status register, or run flashrom with anything but
-r and --flash-name. The only programmer operations the manifest can record are reads.

The JEDEC is *declared* from the part's marking, because a generic programmer reads the silicon but
does not run the vendor's 0x9F identification; the manifest says so (`jedec_source`). The identity is
not declared: it is decoded out of the image (the stock firmware keeps `FM-1_0NN` encrypted inside
app.bin, so a plain search finds nothing on a real unit -- tools/fm1_image.py decodes the app area),
and an identity the owner states must be found there or the read is refused. A manifest with no
identity found is still written, and tools/fm1_restore.py refuses it: an archive nobody can tie to a
unit is not restorable.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import fm1_image as IMG
    import fm1_probe as P
except ImportError:                # imported from the repository root, where tools/ is not on the path
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import fm1_image as IMG
    import fm1_probe as P
import fm1_safewrite as SW

TOOL = "fm1_flash_read 1.0"
FORMAT = "qg1-backup-1"
FLASHROM = "flashrom"
IDENT = re.compile(rb"FM-1_(\d{3})")


class ReadError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def scan_identity(raw):
    """every distinct FM-1_0NN in the image, with the offsets it was found at"""
    hits = {}
    for m in IDENT.finditer(raw):
        key = m.group(0).decode()
        hits.setdefault(key, []).append(m.start())
    return hits


def part_name_matches(name, jedec):
    """does the programmer's detected part name belong to this JEDEC id?"""
    if not name:
        return None
    want = P.PARTS[jedec][0].lower()
    n = str(name).lower()
    return want in n or n.endswith(want) or want.replace("-", "") in n.replace("-", "")


def _flashrom(exe, programmer, *args):
    r = subprocess.run([exe, "-p", programmer, *args], capture_output=True, text=True)
    if r.returncode:
        tail = (r.stderr or r.stdout).strip().splitlines()[-1:] or ["no output"]
        raise ReadError("programmer", "flashrom -p %s %s failed (%d): %s"
                        % (programmer, args[0], r.returncode, tail[0]))
    return r.stdout + r.stderr


def read_external(spec, tmp):
    """-> (image, second read, {programmer facts}, [operations]); reads twice with the same op"""
    kind, _, arg = spec.partition(":")
    if kind == "file":
        p = Path(arg)
        if not p.is_file():
            raise ReadError("programmer", "no such part image: %s" % p)
        first = p.read_bytes()
        second = p.read_bytes()             # the same operation, twice, from the same source
        # no part name is reported by a file: there is no silicon to ask, and a path is not a part
        return first, second, {"type": "file", "name": "", "path": str(p)}, ["read", "read"]
    if kind != "flashrom":
        raise ReadError("programmer", "unknown programmer %r: use file:PATH or flashrom:DEVICE" % spec)
    exe = shutil.which(FLASHROM) if not Path(FLASHROM).is_file() else FLASHROM
    if not exe:
        raise ReadError("programmer", "%s is not installed: apt-get install flashrom, or use "
                                      "--programmer file:PATH to exercise the engine" % FLASHROM)
    name = ""
    try:
        name = _flashrom(exe, arg, "--flash-name").strip()
    except ReadError:
        name = ""                            # the name is a corroboration, not a requirement
    a_path, b_path = Path(tmp) / "pass-a.bin", Path(tmp) / "pass-b.bin"
    _flashrom(exe, arg, "-r", str(a_path))
    _flashrom(exe, arg, "-r", str(b_path))
    return a_path.read_bytes(), b_path.read_bytes(), \
        {"type": "flashrom", "device": arg, "name": name[:200]}, ["read", "read"]


def build_manifest(a, b, facts, ops, jedec, identity_want):
    """the archive the restore tool validates; a and b are the two reads, in order"""
    if jedec not in P.PARTS:
        raise ReadError("chip", "0x%06X is not a part this project documents (%s)"
                        % (jedec, ", ".join("0x%06X" % k for k in P.PARTS)))
    part, cap, page, sector, block = P.PARTS[jedec]
    hits = scan_identity(a)                 # plain occurrences, for the record
    decoded, why = "", ""
    try:
        info = IMG.analyse(a)
    except IMG.ImageError as e:
        why = str(e)
    else:
        decoded = info["identity"]
        if info["app_status"] != "ok":
            why = "the firmware in the image does not verify (%s)" % info["app_status"]
    if identity_want:
        if identity_want not in (decoded, *hits):
            raise ReadError("identity", "%s does not appear in the image (decoded: %s; found in the "
                                        "bytes: %s). A read that does not contain the identity on the "
                                        "label is not this unit's, so nothing was written."
                            % (identity_want, decoded or "none", ", ".join(sorted(hits)) or "none"))
        identity = identity_want
    elif decoded:
        identity = decoded
    elif len(hits) == 1:
        identity = next(iter(hits))
    else:
        identity = ""                        # none, or several: not a fact this tool may pick
    if not identity and why:
        print("note: %s" % why, file=sys.stderr)
    same = bool(a) and a == b
    if not a:
        status = "unverifiable"
    elif not same:
        status = "inconsistent"
    elif len(a) != cap:
        status = "partial"
    else:
        status = "complete"
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    stem = "%s-%s" % (identity or "external", stamp)
    man = {
        "format": FORMAT,
        "tool": TOOL,
        "created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "source": "external-spi",
        "programmer": dict(facts, ops=ops),
        "device": {
            "identity": identity,
            "identity_source": ("decoded from app.bin in the image" if decoded else
                                "searched in the image bytes (FM-1_0NN)" if identity else
                                "not found in the image"),
            "product": "FM-1",
            "firmware": "",
            "jedec": "0x%06X" % jedec,
            "jedec_source": "declared from the part marking; this programmer does not run 0x9F",
            "part": part,
            "capacity": cap,
            "page": page,
            "sector": sector,
            "block": block,
            "identification": "external SPI programmer, two reads, same operation",
            "nonce": "",
        },
        "passes": 2,
        "chunk": 0,
        "size": len(a),
        "expected_size": cap,
        "sha256_pass_a": sha256(a),
        "sha256_pass_b": sha256(b),
        "sha256": sha256(a) if same else "",
        "identical": same,
        "status": status,
        "unread_ranges": [],
        "errors": [],
        "commands": [],
        "command_counts": {},
        "commands_sent": [],
        "frames": 0,
        "files": {"pass_a": stem + ".bin", "pass_b": (stem + ".passB.bin") if not same and b else None},
        "restore": "restorable only when complete and the identity was found in the image",
        "identity_hits": {k: v[:4] for k, v in hits.items()},
        "part_name_reported": facts.get("name", ""),
    }
    if facts.get("name"):
        m = part_name_matches(facts["name"], jedec)
        man["device"]["part_name_agrees"] = m
        if m is False:
            raise ReadError("chip", "the programmer reports %r, which is not the declared 0x%06X (%s): "
                                    "fix the declaration, do not force it"
                            % (facts["name"], jedec, part))
    return man, stem


def run(a):
    tmpdir = tempfile.mkdtemp(prefix="fm1-read-")
    try:
        t0 = time.time()
        if not a.yes and sys.stdin.isatty():
            print("read-only: %s will be read twice. Nothing is written to the part." % a.programmer)
            if input("continue? [y/N] ").strip().lower() not in ("y", "yes"):
                raise ReadError("cancelled", "cancelled")
        first, second, facts, ops = read_external(a.programmer, tmpdir)
        man, stem = build_manifest(first, second, facts, ops, a.jedec, a.identity)
        man["seconds"] = round(time.time() - t0, 1)
        out = Path(a.out)
        out.mkdir(parents=True, exist_ok=True)
        binp = out / man["files"]["pass_a"]
        man_path = out / (stem + ".manifest.json")
        sha_path = out / (stem + ".sha256")
        # Evidence is never replaced, and a reported file is fsynced (M-Flasher 0.11.0's rule).
        for p in (binp, man_path, sha_path):
            if p.exists():
                raise ReadError("exists", "%s already exists: refusing to replace a backup" % p)
        try:
            SW.write_new(binp, first)
            if man["files"]["pass_b"]:
                SW.write_new(out / man["files"]["pass_b"], second)
            SW.write_new(man_path, (json.dumps(man, indent=2, sort_keys=True) + "\n").encode("utf-8"))
            if man["status"] == "complete":
                SW.write_new(sha_path, ("%s  %s\n" % (man["sha256"], binp.name)).encode("utf-8"))
            SW.sync_dir(out)
        except FileExistsError as e:
            raise ReadError("exists", "%s already exists: refusing to replace a backup" % e.filename)
        print("\n%s: %s" % (man["status"], man_path))
        if man["status"] == "complete":
            print("  %s  %s" % (man["sha256"], binp))
            if not man["device"]["identity"]:
                print("  no FM-1_0NN in the image: tools/fm1_restore.py will refuse this archive", file=sys.stderr)
            return 0
        print("  a %s read is not a backup" % man["status"], file=sys.stderr)
        return 1
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def main(argv=None):
    ap = argparse.ArgumentParser(description="read an FM-1's NOR twice with an external programmer")
    ap.add_argument("--out", default="backup", help="output directory")
    ap.add_argument("--programmer", required=True,
                    help="file:PATH (a part simulated on a file) or flashrom:DEVICE (an external SPI programmer)")
    ap.add_argument("--jedec", required=True, help="the JEDEC id printed on the part, e.g. 0x856014")
    ap.add_argument("--identity", help="the identity on the unit's label, e.g. FM-1_015 (must be in the image)")
    ap.add_argument("--yes", action="store_true", help="do not ask before reading")
    a = ap.parse_args(argv)
    try:
        a.jedec = int(str(a.jedec), 16)
    except ValueError:
        print("--jedec must be a hex id such as 0x856014", file=sys.stderr)
        return 2
    try:
        return run(a)
    except ReadError as e:
        print("refused: %s" % e, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
