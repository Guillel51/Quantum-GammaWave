#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Turn an M-Flasher 0.9.x backup into the archive tools/fm1_restore.py validates.

M-Flasher (MIT, (c) jvitkauskas) is the first backup path that needs neither an opened unit nor an
external programmer: it puts the stock firmware in the chip's USB download mode, runs a flash loader
from RAM, reads the whole 1 MiB part and checks the read against a CRC32 the chip computes itself.
Its backup action only reads -- `Session::open`/`save_backup` in src/ops.rs call `dev.read` and
`dev.crc32`, and the only writes in the whole program sit behind restore/install plans
(docs/M-FLASHER.md has the line-by-line audit).

This tool does not run M-Flasher and never touches a unit. It takes the two files M-Flasher wrote --
the image and its .json -- and proves what can be proved from them: the SHA-256, the full 1 MiB, the
JieLi boot/app tables and their CRCs, the flash layout, and the `FM-1_0NN` identity that the stock
firmware carries encrypted inside app.bin. What it cannot prove is that the read was honest; that is
the chip's CRC32 (pass it with --crc32 from M-Flasher's log) or a second independent backup
(--second), and without one of the two the archive is written but marked unverifiable and
tools/fm1_restore.py refuses it.

  fm1_mflasher.py import --bin fm1-20261106-101500.bin --crc32 0x60894105 --out backup/stock-01
  fm1_mflasher.py import --bin a.bin --second b.bin --out backup/stock-01
  fm1_mflasher.py check backup/stock-01/*.manifest.json

The JSON M-Flasher writes is read, never trusted: each field it supplies is either checked against the
image or recorded under `declared` as the tool's statement, not as this project's fact.

Exit: 0 = a restorable archive, 1 = refused, 2 = usage.
"""
import argparse
import hashlib
import json
import re
import shutil
import sys
import zlib
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_image as IMG  # noqa: E402
import fm1_probe as P  # noqa: E402

TOOL = "fm1_mflasher 1.0"
FORMAT = "qg1-backup-1"
SOURCE = "mflasher-ram-loader"
# What this project has validated for writing. A UCUN part is readable and its backup is valid
# evidence, but no QG-1 build has been written to one, so it is not authorised for a write.
WRITE_OK = {0x856014}
TOOL_RE = re.compile(r"^m-flasher (\d+)\.(\d+)\.(\d+)$")


class ImportError_(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def load_json(path):
    try:
        man = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        raise ImportError_("json", "cannot read %s: %s" % (path, e))
    if not isinstance(man, dict):
        raise ImportError_("json", "%s is not a JSON object" % path)
    need = ("tool", "model", "firmware", "app", "chip key", "flash", "unit id", "loader", "sha256", "saved")
    missing = [k for k in need if k not in man]
    if missing:
        raise ImportError_("json", "%s is not an M-Flasher backup record (no %s)" % (path, ", ".join(missing)))
    m = TOOL_RE.match(str(man["tool"]).strip())
    if not m:
        raise ImportError_("tool", "%s was not written by M-Flasher (%r)" % (path, man["tool"]))
    if (int(m.group(1)), int(m.group(2))) < (0, 9):
        raise ImportError_("tool", "%s is from M-Flasher %s; 0.9 or newer is what this reads"
                           % (path, man["tool"]))
    return man


def parse_hex(v, what):
    try:
        return int(str(v).strip(), 16)
    except ValueError:
        raise ImportError_("json", "M-Flasher's %s field is not hex: %r" % (what, v))


def one(name, path, man):
    """Check one M-Flasher backup: its .json against its bytes, then the bytes against themselves."""
    raw = Path(path).read_bytes()
    if len(raw) != IMG.CAP:
        raise ImportError_("size", "%s is %d bytes; an FM-1 part is %d (1 MiB, nothing less is a backup)"
                           % (path, len(raw), IMG.CAP))
    digest = sha256(raw)
    if digest != str(man["sha256"]).lower():
        raise ImportError_("hash", "%s's SHA-256 is %s, its .json says %s" % (path, digest, man["sha256"]))
    jedec = parse_hex(man["flash"], "flash")
    if jedec not in P.PARTS:
        raise ImportError_("chip", "M-Flasher read a flash id of 0x%06X, which this project does not "
                                   "document (%s)" % (jedec, ", ".join("0x%06X" % k for k in P.PARTS)))
    chip_key = parse_hex(man["chip key"], "chip key")
    if chip_key != IMG.CHIP_KEY:
        # the identity is encrypted with it: a different key means this project cannot read the unit
        raise ImportError_("chip", "the unit's chip key is 0x%04X, not 0x%04X: this project can only "
                                   "decode a unit it knows" % (chip_key, IMG.CHIP_KEY))
    try:
        info = IMG.analyse(raw, chip_key)
    except IMG.ImageError as e:
        raise ImportError_("image", "%s does not decode (%s): %s" % (path, e.code, e))
    if info["app_status"] != "ok":
        raise ImportError_("image", "%s: the firmware does not verify (%s), so this is not a clean "
                                    "backup of a working unit" % (path, info["app_status"]))
    if not info["fm1_layout"]:
        raise ImportError_("model", "%s is not an FM-1 image: %s" % (path, info["layout_evidence"]))
    if not info["identity"]:
        raise ImportError_("identity", "%s: no FM-1_0NN in the decoded app, so the archive cannot be "
                                       "tied to a unit" % path)
    if not str(man["app"]).startswith("OK"):
        raise ImportError_("image", "M-Flasher recorded the app as %r while the image decodes as ok"
                           % man["app"])
    return raw, info, jedec, digest


def build(first, first_man, second=None, second_man=None, chip_crc32=None, out=None):
    """-> (manifest, image, identity); raises ImportError_ with the first thing that is wrong."""
    raw, info, jedec, digest = one("first", first, first_man)
    second_digest = None
    if second:
        raw2, info2, jedec2, second_digest = one("second", second, second_man or first_man)
        if raw2 != raw:
            raise ImportError_("reads", "the two backups differ: %d bytes do not match (first %s, second %s)"
                               % (sum(1 for a, b in zip(raw, raw2) if a != b), digest, second_digest))
        if jedec2 != jedec:
            raise ImportError_("chip", "the two backups name different chips: 0x%06X and 0x%06X"
                               % (jedec, jedec2))
        if info2["identity"] != info["identity"]:
            raise ImportError_("identity", "the two backups decode to different units: %s and %s"
                               % (info["identity"], info2["identity"]))
    digest_whole = zlib.crc32(raw) & 0xFFFFFFFF
    if chip_crc32 is not None:
        got = int(str(chip_crc32), 0) & 0xFFFFFFFF
        if got != digest_whole:
            raise ImportError_("crc32", "the image's CRC32 is 0x%08X, the chip's was 0x%08X: the read is "
                                        "not what the chip holds" % (digest_whole, got))
    verified = bool(second) or chip_crc32 is not None
    part, cap, page, sector, block = P.PARTS[jedec]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    stem = "mflasher-%s-%s" % (info["identity"], stamp)
    man = {
        "format": FORMAT,
        "tool": TOOL,
        "created": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "source": SOURCE,
        "programmer": {
            "type": "mflasher",
            "ops": ["read", "read"] if second else ["read"],
            "flash_writes": 0,
            "ram_writes": ["flash loader at 0x01C20000 or 0x01C28000", "jump to it"],
            "crc32": "0x%08X" % digest_whole,
            "crc32_source": ("computed here over the image; the chip's own CRC32 was given as %s"
                             % chip_crc32) if chip_crc32 is not None else
                            "computed here over the image; no chip CRC32 was supplied",
            "declared_by": first_man["tool"],
            "declared_commit": "9da825b2000143d89e8ca7ac016f35abe304158c (v0.9.1)",
        },
        "device": {
            "identity": info["identity"],
            "identity_source": "decoded from app.bin in the image with the chip key 0x%04X" % IMG.CHIP_KEY,
            "product": "FM-1",
            "product_source": info["layout_evidence"],
            "firmware": info["firmware"],
            "app_bin": info["app_files"][0] if info["app_files"] else None,
            "bootloader": info["bootloader"],
            "jedec": "0x%06X" % jedec,
            "jedec_source": "reported by M-Flasher's loader from the chip's identification",
            "part": part,
            "capacity": cap,
            "page": page,
            "sector": sector,
            "block": block,
            "chip_key": "0x%04X" % IMG.CHIP_KEY,
            "unit_id": str(first_man["unit id"]),
            "identification": "USB download mode, flash loader in RAM, whole-part read",
            "nonce": "",
        },
        "passes": 2 if second else 1,
        "chunk": 0,
        "size": len(raw),
        "expected_size": cap,
        "sha256_pass_a": digest,
        "sha256_pass_b": second_digest or (digest if chip_crc32 is not None else ""),
        "sha256": digest,
        "identical": bool(second) or chip_crc32 is not None,
        "verified_by": ("two independent backups, byte for byte" if second else
                        ("the chip's own CRC32 over the whole part" if chip_crc32 is not None else
                         "nothing: one read, and no CRC32 from the chip")),
        "status": "complete" if verified else "unverifiable",
        "unread_ranges": [],
        "errors": [],
        "commands": [],
        "command_counts": {},
        "commands_sent": [],
        "frames": 0,
        "files": {"pass_a": stem + ".bin", "pass_b": None},
        "restore": "restorable only when complete: a second read or the chip's CRC32 has to agree",
        "write_authorised": jedec in WRITE_OK,
        "write_authorised_note": ("known part with a validated write path" if jedec in WRITE_OK else
                                 "readable and backed up, but no QG-1 write has been validated on this "
                                 "part: restoring this archive is fine, installing firmware is not"),
        "declared": {k: first_man.get(k) for k in
                     ("tool", "model", "bootloader", "firmware", "app", "chip key", "flash",
                      "unit id", "loader", "saved")},
        "audit": {
            "mflasher_backup_writes_flash": False,
            "evidence": "docs/M-FLASHER.md section 3: Session::open/read/crc32 only; erase/program are "
                        "reachable only from restore/install plans",
            "decoded_here": {"boot_table": len(info["boot_table"]), "app_status": info["app_status"],
                             "layout": info["layout_evidence"]},
        },
    }
    return man, raw, info["identity"]


def write_archive(man, raw, out):
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    binp = out / man["files"]["pass_a"]
    binp.write_bytes(raw)
    man_path = out / (Path(man["files"]["pass_a"]).stem + ".manifest.json")
    man_path.write_text(json.dumps(man, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    if man["status"] == "complete":
        (out / (Path(man["files"]["pass_a"]).stem + ".sha256")).write_text(
            "%s  %s\n" % (man["sha256"], binp.name))
    return man_path, binp


def check(man_path):
    """Re-read an archive this tool wrote and redo every check, from the bytes."""
    man_path = Path(man_path)
    man = json.loads(man_path.read_text(encoding="utf-8"))
    if man.get("source") != SOURCE:
        raise ImportError_("source", "%s is not an M-Flasher import (%r)" % (man_path, man.get("source")))
    raw = (man_path.parent / man["files"]["pass_a"]).read_bytes()
    if sha256(raw) != man["sha256"]:
        raise ImportError_("hash", "the image's SHA-256 is not the manifest's")
    info = IMG.analyse(raw, IMG.CHIP_KEY)
    if info["identity"] != man["device"]["identity"]:
        raise ImportError_("identity", "the image decodes to %s, the manifest says %s"
                           % (info["identity"], man["device"]["identity"]))
    if man["status"] != "complete":
        raise ImportError_("incomplete", "the archive is %r: %s" % (man["status"], man["verified_by"]))
    return man, info


def main(argv=None):
    ap = argparse.ArgumentParser(description="import an M-Flasher 0.9.x backup as a QG-1 archive")
    sub = ap.add_subparsers(dest="cmd", required=True)
    imp = sub.add_parser("import", help="validate M-Flasher's pair and write a qg1-backup-1 manifest")
    imp.add_argument("--bin", required=True, help="the image M-Flasher saved")
    imp.add_argument("--json", help="its .json (default: same name with .json)")
    imp.add_argument("--second", help="the image of a second backup, for a byte-for-byte check")
    imp.add_argument("--second-json", help="the second backup's .json (default: same name with .json)")
    imp.add_argument("--crc32", help="the CRC32 M-Flasher printed for the whole flash, e.g. 0x60894105")
    imp.add_argument("--out", default="backup", help="where to write the archive")
    chk = sub.add_parser("check", help="re-verify an archive this tool wrote")
    chk.add_argument("manifest")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "check":
            man, info = check(a.manifest)
            print("%s: %s, %s, %d B, %s" % (a.manifest, man["device"]["identity"], man["device"]["jedec"],
                                            man["size"], man["verified_by"]))
            print("  %s" % man["sha256"])
            return 0
        first_man = load_json(a.json or Path(a.bin).with_suffix(".json"))
        second_man = None
        if a.second:
            second_man = load_json(a.second_json or Path(a.second).with_suffix(".json"))
        man, raw, identity = build(a.bin, first_man, a.second, second_man, a.crc32, a.out)
        man_path, binp = write_archive(man, raw, a.out)
        print("%s: %s" % (man["status"], man_path))
        print("  %s  %s" % (man["sha256"], binp))
        print("  identity %s, %s, %s" % (identity, man["device"]["jedec"], man["device"]["firmware"]))
        print("  verified by %s" % man["verified_by"])
        if man["status"] != "complete":
            print("  no second read and no chip CRC32: tools/fm1_restore.py will refuse this archive.\n"
                  "  Run M-Flasher's backup again, or pass --crc32 with the value it printed.",
                  file=sys.stderr)
            return 1
        if not man["write_authorised"]:
            print("  note: %s" % man["write_authorised_note"], file=sys.stderr)
        return 0
    except (ImportError_, IMG.ImageError) as e:
        print("refused (%s): %s" % (getattr(e, "code", "image"), e), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
