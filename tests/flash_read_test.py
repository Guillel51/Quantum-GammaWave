#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_flash_read.py against a part simulated on a file, plus its hand-off to restore.

This is the first-backup path for a stock unit (docs/FLASH_READER.md): the programmer reads the part
twice, the two images must agree byte for byte, and the manifest has to be one that
tools/fm1_restore.py will accept -- otherwise the "backup" is a file nobody can restore from. So the
positive case runs the real tool, and the negative cases are the ones that matter: a part that changes
between reads, a declared identity that is not in the image, a declared JEDEC the programmer
contradicts, and a manifest that claims the external source without the two-read receipt.

  python3 tests/flash_read_test.py
"""
import argparse
import contextlib
import hashlib
import io
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import fm1_flash_read as R  # noqa: E402
import fm1_probe as P  # noqa: E402
import fm1_restore as S  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print("%-72s %s" % (what, "ok" if cond else "FAIL"))
    failed += not cond


def part(tmp, identity="FM-1_015", jedec_mark="P25Q80H", size=0x100000, fill=None):
    """a 1 MiB part on disk: recognisable boot bytes and the identity the label claims"""
    raw = bytearray((i * 13 + 7) & 0xFF for i in range(size)) if fill is None \
        else bytearray(bytes([fill]) * size)
    raw[0:8] = bytes.fromhex("5aa5f00f1234cdef")
    raw[0x4100:0x4100 + len(identity)] = identity.encode()
    p = Path(tmp) / ("part-%s.bin" % hashlib.sha256(identity.encode() + bytes(raw[:64])).hexdigest()[:8])
    p.write_bytes(bytes(raw))
    return p


def run(*argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
        code = R.main(list(argv))
    return code, buf.getvalue()


def main():
    tmp = Path(tempfile.mkdtemp(prefix="qg1-flash-read-"))
    p = part(tmp)

    # 1. the positive case: two reads of the same part, the identity found in the image
    out = tmp / "read-01"
    code, text = run("--out", str(out), "--programmer", "file:%s" % p, "--jedec", "0x856014", "--yes")
    ok(code == 0 and "complete" in text, "a stable part reads complete")
    man_path = next(out.glob("*.manifest.json"))
    man = json.loads(man_path.read_text())
    ok(man["source"] == "external-spi" and man["programmer"]["ops"] == ["read", "read"],
       "the manifest records the external source and exactly two reads")
    ok(man["commands"] == [] and man["commands_sent"] == [] and man["frames"] == 0,
       "no MIDI frame is claimed for an external read")
    ok(man["device"]["identity"] == "FM-1_015" and "image" in man["device"]["identity_source"],
       "the identity came from the image, not from a declaration")
    ok(man["device"]["jedec"] == "0x856014" and "marking" in man["device"]["jedec_source"],
       "the JEDEC is declared and labelled as such")
    ok(man["sha256"] == hashlib.sha256(p.read_bytes()).hexdigest() and man["size"] == 0x100000,
       "the hash and size are the part's")
    ok((out / (man_path.stem.replace(".manifest", "") + ".sha256")).exists(), "the .sha256 sidecar is written")

    # 2. the archive the restore tool accepts, and its read-back path
    facts = S.validate(man, (out / man["files"]["pass_a"]).read_bytes(), out / man["files"]["pass_a"])
    ok(facts["identity"] == "FM-1_015" and facts["jedec"] == "0x856014",
       "restore validates an external archive and gets the identity from it")

    # 3. the identity on the label that is not in the image: refused, not written
    code, text = run("--out", str(tmp / "read-02"), "--programmer", "file:%s" % p,
                     "--jedec", "0x856014", "--identity", "FM-1_965", "--yes")
    ok(code == 1 and "does not appear in the image" in text and not (tmp / "read-02").exists(),
       "a declared identity absent from the image is refused and nothing is written")

    # 4. two reads that differ: inconsistent, both kept, no restorable hash
    a = bytes(0x100000)
    b = bytearray(a)
    b[0x9000] ^= 0xFF
    man2, _ = R.build_manifest(a, bytes(b), {"type": "file", "path": "x"}, ["read", "read"],
                               0x856014, None)
    ok(man2["status"] == "inconsistent" and not man2["identical"] and man2["sha256"] == "",
       "two reads that differ are inconsistent, with no final hash")

    # 5. a capacity that is not the part's: partial, never complete
    man3, _ = R.build_manifest(bytes(0x80000), bytes(0x80000), {"type": "file", "path": "x"},
                               ["read", "read"], 0x856014, None)
    ok(man3["status"] == "partial" and man3["size"] == 0x80000,
       "a short read is partial, not a smaller chip")

    # 6. an unknown JEDEC: refused before anything is written
    try:
        R.build_manifest(bytes(0x100000), bytes(0x100000), {"type": "file", "path": "x"},
                         ["read", "read"], 0x123456, None)
    except R.ReadError as e:
        ok(e.code == "chip", "an undocumented JEDEC is refused")
    else:
        ok(False, "an undocumented JEDEC was accepted")

    # 7. the programmer's detected name contradicts the declared id
    try:
        R.build_manifest(bytes(0x100000), bytes(0x100000),
                         {"type": "flashrom", "device": "ch341a_spi", "name": "Winbond W25Q16"},
                         ["read", "read"], 0x856014, None)
    except R.ReadError as e:
        ok(e.code == "chip", "a programmer part name that contradicts the declaration is refused")
    else:
        ok(False, "a contradicting part name was accepted")

    # 8. no identity in the image: a complete read, and restore refuses it
    blank = part(tmp, identity="", fill=0x41)
    out3 = tmp / "read-03"
    code, text = run("--out", str(out3), "--programmer", "file:%s" % blank, "--jedec", "0x856014", "--yes")
    man4 = json.loads(next(out3.glob("*.manifest.json")).read_text())
    ok(code == 0 and man4["device"]["identity"] == "" and "refuse this archive" in text,
       "an image with no identity is complete and says restore will refuse it")
    try:
        S.validate(man4, (out3 / man4["files"]["pass_a"]).read_bytes(), out3 / man4["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "identity", "restore refuses an archive that cannot be tied to a unit")
    else:
        ok(False, "restore accepted an archive with no identity")

    # 9. a manifest that claims external without the two-read receipt
    bad = json.loads(man_path.read_text())
    bad["programmer"]["ops"] = ["read", "write"]
    try:
        S.validate(bad, (out / man["files"]["pass_a"]).read_bytes(), out / man["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "readonly", "a programmer operation other than two reads is refused by restore")
    else:
        ok(False, "restore accepted a write operation in the receipt")

    # 10. the evidence checker accepts the external bundle and reports it as not restorable-only
    import fm1_flash_evidence as E
    rec = tmp / "evidence.json"
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        ev_code = E.check_bundle(str(out), None, str(rec))
    doc = json.loads(rec.read_text())
    ok(ev_code == 0 and doc["source"] == "external-spi" and doc["result"]["restorable"] is True,
       "the evidence checker verifies an external bundle and its restorability")
    ok(doc["result"]["authorises_install"] is False, "the external record never authorises an install")

    print("flash read: %d checks, %d FAILED" % (16, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
