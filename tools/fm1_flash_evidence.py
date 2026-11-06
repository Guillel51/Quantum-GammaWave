#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Check a read-only flash evidence bundle, and say what it does and does not prove.

`tools/fm1_backup.py` reads an FM-1's flash twice (0x11 handshake, 0x12 identification, 0x13 read,
nothing else) and writes an image plus a manifest. This tool does not talk to a device: it takes that
directory apart and holds every claim in it to the bytes on disk.

    python3 tools/fm1_flash_evidence.py check backup/read-01
    python3 tools/fm1_flash_evidence.py check backup/read-01 --second backup/read-02
    python3 tools/fm1_flash_evidence.py check backup/read-01 --json out/evidence.json

What it verifies: the manifest is a qg1-backup-1 record; both passes ran; the image is exactly the
capacity the identification announced; the SHA-256 in the manifest is the SHA-256 of the file; a second
pass file, when present, is byte-identical; every frame the tool logged is 0x11/0x12/0x13 (so the run
was read-only by its own log, not by its description); the JEDEC id is one of the parts this project
documents, and the geometry in the manifest is that part's; the identity is a stock FM-1 the donor rule
can start from. With --second, two independent reads must agree byte for byte.

What it does not do, on purpose: it never decides that an install may run. A verified read is evidence
about a unit and a part; it is not a signature, not a backup you have restored, and not a recovery
path. The installer still reads 0x12 on the live link before it writes, and keeps refusing while the
release key is unpinned.
"""
import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

try:
    import fm1_image as IMG        # decodes the boot/app tables so the image can speak for itself
    import fm1_probe as P          # the one geometry table: JEDEC -> (part, cap, page, sector, block)
except ImportError:                # imported from the repository root, where tools/ is not on the path
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import fm1_image as IMG
    import fm1_probe as P

FORMAT_IN = "qg1-backup-1"
FORMAT_OUT = "qg1-flash-evidence-1"
READ_ONLY = {0x11, 0x12, 0x13}
# reading is allowed on both documented parts; writing is only validated on the OEM one
WRITABLE = {0x856014}
IDENTITY = re.compile(r"^FM-1_0\d\d$")     # a stock unit: the donor the installer may start from
HEX64 = re.compile(r"^[0-9a-f]{64}$")

checks = []


def check(name, ok, detail=""):
    checks.append({"check": name, "ok": bool(ok), "detail": str(detail)[:300]})
    print("evidence: %-62s %s%s" % (name, "ok" if ok else "FAIL", ("  " + str(detail)[:120]) if detail and not ok else ""))
    return bool(ok)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def read_manifest(bundle):
    """the one qg1-backup-1 manifest in a bundle directory (or the failure that says why not)"""
    bundle = Path(bundle)
    if not bundle.is_dir():
        return None, None, "not a directory: %s" % bundle
    found = sorted(bundle.glob("*.manifest.json"))
    if not found:
        return None, None, "no *.manifest.json in %s" % bundle
    if len(found) > 1:
        return None, None, "%d manifests in %s: one bundle is one read" % (len(found), bundle)
    try:
        return json.loads(found[0].read_text(encoding="utf-8")), found[0], None
    except (OSError, ValueError) as e:
        return None, found[0], "%s is not readable JSON: %s" % (found[0].name, e)


def check_bundle(bundle, second=None, out_json=None):
    checks.clear()
    man, man_path, why = read_manifest(bundle)
    if man is None:
        check("a single, readable manifest is present", False, why)
        return finish(bundle, None, out_json)

    check("the manifest is a %s record" % FORMAT_IN, man.get("format") == FORMAT_IN, man.get("format"))
    check("it names the tool that wrote it", bool(man.get("tool")), man.get("tool"))
    check("it has a creation timestamp", bool(man.get("created")), man.get("created"))

    dev = man.get("device") or {}
    jedec_s = str(dev.get("jedec") or "")
    try:
        jedec = int(jedec_s, 16)
    except ValueError:
        jedec = None
    known = P.PARTS.get(jedec)
    part = None
    check("the identification returned a JEDEC id", bool(jedec), jedec_s)
    check("the JEDEC id is a part this project documents",
          known is not None, "%s is not in %s" % (jedec_s, ", ".join("0x%06X" % k for k in P.PARTS)))
    if known:
        part = {"part": known[0], "cap": known[1], "writable": jedec in WRITABLE}
        check("the manifest's part name is the probe's for that id", dev.get("part") == known[0],
              "%r vs %r" % (dev.get("part"), known[0]))
        for want, key in ((1, "capacity"), (2, "page"), (3, "sector"), (4, "block")):
            check("the geometry agrees with the part on %s" % key, dev.get(key) == known[want],
                  "%r vs %r" % (dev.get(key), known[want]))
    external = man.get("source") in ("external-spi", "mflasher-ram-loader")
    if external:
        prog = man.get("programmer") or {}
        if man.get("source") == "external-spi":
            check("the read came from an external programmer, and the manifest says which",
                  prog.get("type") in ("file", "flashrom")
                  and bool(prog.get("device") or prog.get("path")),
                  prog.get("type"))
            check("the programmer ran exactly two reads, in order", prog.get("ops") == ["read", "read"],
                  prog.get("ops"))
            check("the JEDEC was declared, and the manifest says where from",
                  "marking" in str(dev.get("jedec_source") or ""), dev.get("jedec_source"))
        else:
            check("the read came from M-Flasher's RAM loader", prog.get("type") == "mflasher",
                  prog.get("type"))
            check("M-Flasher's backup wrote nothing to the flash",
                  prog.get("flash_writes") == 0, prog.get("flash_writes"))
            check("the read was checked by a second backup or the chip's CRC32",
                  prog.get("ops") == ["read", "read"] or bool(man.get("verified_by")),
                  man.get("verified_by"))
            check("the JEDEC was read from the chip, and the manifest says so",
                  "chip" in str(dev.get("jedec_source") or ""), dev.get("jedec_source"))
        if dev.get("part_name_agrees") is False:
            check("the programmer's detected part name agrees with the declared JEDEC", False,
                  man.get("part_name_reported"))
        elif dev.get("part_name_agrees") is True:
            check("the programmer's detected part name agrees with the declared JEDEC", True)
    else:
        check("two 0x12 identifications were taken, not one",
              "two readings" in str(dev.get("identification") or ""), dev.get("identification"))
        check("the unit identified as a stock FM-1 this project may start from",
              bool(IDENTITY.match(str(dev.get("identity") or ""))), dev.get("identity"))
        check("the firmware the unit reported is recorded", str(dev.get("firmware") or "") != "",
              dev.get("firmware"))

    # the read itself: two reads, or M-Flasher's one read checked against the CRC32 the chip computed
    chip_verified = (man.get("source") == "mflasher-ram-loader"
                     and "chip's own CRC32" in str(man.get("verified_by") or ""))
    check("two passes were run, or the chip's own CRC32 agreed with the one read",
          man.get("passes") == 2 or (man.get("passes") == 1 and chip_verified), man.get("passes"))
    check("the result is complete", man.get("status") == "complete", man.get("status"))
    check("no range was left unread", not man.get("unread_ranges"), man.get("unread_ranges"))
    check("no read error is recorded", not man.get("errors"), len(man.get("errors") or []))
    check("the passes are identical", man.get("identical") is True, man.get("identical"))
    cap = dev.get("capacity")
    check("the image is the capacity the part announced", cap and man.get("size") == cap and man.get("expected_size") == cap,
          "size %r, expected %r, part %r" % (man.get("size"), man.get("expected_size"), cap))

    files = man.get("files") or {}
    a_name = files.get("pass_a")
    base = man_path.parent
    a_path = base / a_name if a_name else None
    have_a = bool(a_path and a_path.is_file())
    check("the first pass image is in the bundle", have_a, a_name or "no file named")
    if have_a:
        real = sha256(a_path)
        check("the image's size is the manifest's", a_path.stat().st_size == man.get("size"),
              "%d vs %r" % (a_path.stat().st_size, man.get("size")))
        check("the image's SHA-256 is the manifest's", real == man.get("sha256"),
              real[:16] + " vs " + str(man.get("sha256"))[:16])
        check("the first pass hash is that same hash", real == man.get("sha256_pass_a"),
              real[:16] + " vs " + str(man.get("sha256_pass_a"))[:16])
        check("the manifest's hash is a SHA-256", bool(HEX64.match(str(man.get("sha256") or ""))), man.get("sha256"))
        side = base / ("%s.sha256" % Path(a_name).name.split(".")[0]) if a_name else None
        hash_files = sorted(base.glob("*.sha256"))
        if hash_files:
            first = hash_files[0].read_text(encoding="utf-8").split()
            check("the .sha256 sidecar names this image and this hash",
                  len(first) == 2 and first[0] == real and first[1] == a_path.name,
                  " ".join(first[:2]))
        if external:
            # The structure has to agree with the manifest: a JSON is not evidence by itself.
            try:
                info = IMG.analyse(a_path.read_bytes())
            except IMG.ImageError as e:
                # not every readable part is a JieLi-structured one (a bare programmer read of a
                # blank part, say): then the identity still has to be in the bytes somewhere.
                check("the image's JieLi structures decode, or its identity is in its bytes",
                      str(dev.get("identity") or "").encode() in a_path.read_bytes(),
                      "%s: %s" % (e.code, e))
            else:
                check("the image's JieLi structures decode", True)
                check("the image's firmware verifies (app.bin and its CRC16)",
                      info["app_status"] == "ok", info["app_status"])
                check("the image decodes to the identity the manifest names",
                      info["identity"] == dev.get("identity"),
                      "%r vs %r" % (info["identity"], dev.get("identity")))
                check("the image declares the FM-1 flash layout", info["fm1_layout"],
                      info["layout_evidence"])
    b_name = files.get("pass_b")
    if b_name:
        b_path = base / b_name
        have_b = b_path.is_file()
        check("a second pass file is present when the manifest names one", have_b, b_name)
        if have_b:
            check("the second pass is byte-identical to the first", sha256(b_path) == man.get("sha256"),
                  sha256(b_path)[:16])
    else:
        check("no second pass file means the two passes were identical", man.get("identical") is True)

    # the transport: read-only by what was actually sent (or, externally, by what was never sent)
    cmds = man.get("commands")
    counts = man.get("command_counts") or {}
    known = isinstance(cmds, list) and all(isinstance(c, int) for c in cmds)
    check("the frame log is a list of command bytes", known, type(cmds).__name__)
    if known and external:
        check("a non-MIDI read sent no frames at all", cmds == [] and man.get("frames") in (0, None)
              and (man.get("commands_sent") == []), (len(cmds), man.get("frames")))
    elif known:
        extra = sorted({c for c in cmds if c not in READ_ONLY})
        check("every logged frame is 0x11, 0x12 or 0x13", not extra,
              "unexpected: %s" % ", ".join("0x%02X" % c for c in extra))
        check("the log has frames in it", len(cmds) > 0, len(cmds))

    def hexkey(c):
        try:
            return int(str(c), 16)
        except ValueError:
            return None

    if not external:
        check("the recorded command counts are read-only too",
              bool(counts) and all(hexkey(c) in READ_ONLY for c in counts), list(counts))
        check("the tool's own summary lists only read-only commands",
              all(c in READ_ONLY for c in (man.get("commands_sent") or [0x11])), man.get("commands_sent"))

    second_ok = None
    if second:
        man2, man2_path, why2 = read_manifest(second)
        if man2 is None:
            check("a second read bundle is readable", False, why2)
        else:
            check("the two reads are of the same part",
                  man2.get("device", {}).get("jedec") == dev.get("jedec"),
                  "%r vs %r" % (man2.get("device", {}).get("jedec"), dev.get("jedec")))
            check("the two reads have the same SHA-256", man2.get("sha256") == man.get("sha256"),
                  "%s vs %s" % (str(man2.get("sha256"))[:16], str(man.get("sha256"))[:16]))
            f2 = (man2.get("files") or {}).get("pass_a")
            p2 = man2_path.parent / f2 if f2 else None
            second_ok = bool(p2 and p2.is_file() and have_a and sha256(p2) == sha256(a_path))
            check("the second read's image is byte-identical to the first", second_ok,
                  "missing %s" % (f2 or "file") if not p2 else "")
    return finish(bundle, man, out_json, second, second_ok, part)


def finish(bundle, man, out_json, second=None, second_ok=None, part=None):
    good = all(c["ok"] for c in checks)
    names = [c["check"] for c in checks if not c["ok"]]
    record = {
        "format": FORMAT_OUT,
        "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "bundle": str(bundle),
        "source_manifest": (man or {}).get("tool"),
        "source": (man or {}).get("source") or "device-0x12",
        "device": (man or {}).get("device"),
        "image": {
            "size": (man or {}).get("size"),
            "sha256": (man or {}).get("sha256"),
        },
        "read_only": {
            "commands": (man or {}).get("commands_sent"),
            "frames": (man or {}).get("frames"),
            "verdict": "read-only by the tool's own frame log" if man else "no log",
        },
        "second_read": {"bundle": str(second) if second else None, "identical": second_ok},
        "checks": checks,
        "result": {
            "status": "verified-read" if good else "failed",
            "failed": names,
            # an external read with no FM-1_0NN in it is a verified read of *a* part, not of *this*
            # unit: the restore tool refuses it, and the record says so instead of letting a green
            # "verified-read" imply more than it can.
            "restorable": bool(((man or {}).get("device") or {}).get("identity")),
            # this is the part-specific half: a read-only part is a verified read and NOT a green
            # light for writing, which is why the record says it separately instead of folding it in
            "write_capable_part": bool(part and part.get("writable")),
            "authorises_install": False,
        },
        "limits": [
            "a read is not a restore: recovery has not been demonstrated on this unit",
            "the QG flash map against the stock layout is a manual comparison and is not done here",
            "the SoC, the clock and the bootloader are not exposed by this protocol",
            "no signature, no release key and no package are involved: this record cannot authorise a write",
        ],
    }
    body = {k: v for k, v in record.items() if k != "record_sha256"}
    record["record_sha256"] = hashlib.sha256(json.dumps(body, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    if out_json:
        p = Path(out_json)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print("evidence: record written to %s (%s)" % (p, record["record_sha256"][:16]))
    print("evidence: %s" % ("verified read" if good else "%d check(s) failed: %s" % (len(names), "; ".join(names))))
    if good:
        print("evidence: this is a read about a unit and a part. It authorises nothing: the installer still")
        print("evidence: reads 0x12 on the live link, and the release key is still unpinned.")
    return 0 if good else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="check a read-only FM-1 flash evidence bundle")
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check", help="verify a bundle written by tools/fm1_backup.py")
    c.add_argument("bundle", help="the directory fm1_backup.py --out wrote")
    c.add_argument("--second", help="a second, independent read of the same unit")
    c.add_argument("--json", dest="out_json", help="write the evidence record here")
    a = ap.parse_args(argv)
    return check_bundle(a.bundle, a.second, a.out_json)


if __name__ == "__main__":
    sys.exit(main())
