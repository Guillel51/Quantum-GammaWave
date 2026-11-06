#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_flash_evidence.py against bundles a simulated FM-1 actually produced.

The point of the checker is that a bundle is not taken on trust: the image on disk must hash to what
the manifest says, the geometry must be the identified part's, and the frame log must contain nothing
but 0x11/0x12/0x13. So the positive case here is a real `fm1_backup.backup()` run against the
simulated unit (the same one tests/chip_test.py uses), and every negative case is that bundle with one
thing changed -- a byte of the image, a frame in the log, the JEDEC id, the status, a second read that
disagrees.

  python3 tests/flash_evidence_test.py
"""
import json
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import fm1_backup as B  # noqa: E402
import fm1_install as I  # noqa: E402
import fm1_probe as P  # noqa: E402
import fm1_flash_evidence as E  # noqa: E402
from install_test import FakeFM1  # noqa: E402

I.DELAY.update(open=0, hs=0.05, retry=0.01)
failed = 0


def ok(cond, what):
    global failed
    print("%-70s %s" % (what, "ok" if cond else "FAIL"))
    failed += not cond


def bundle(tmp, name="read-01", dev=None):
    """a real read-only backup of the simulated unit, in its own directory

    `B.run()` gets the identity from the MIDI device info and folds it into the probe evidence; a test
    that calls `backup()` directly has to do the same, or the manifest would carry an empty identity and
    the checker is right to reject it.
    """
    out = Path(tmp) / name
    dev = dev or FakeFM1(b"")
    real = P.probe
    ident = {"identity": dev.identity, "product": "FM-1", "firmware": "V15"}

    def with_identity(ln, *a, **kw):
        ev = real(ln, *a, **kw)
        ev.update(ident)
        return ev

    P.probe = with_identity
    try:
        man, path, binp = B.backup(dev.open(dev.name, dev.name), out)
    finally:
        P.probe = real
    return out, man, path, binp


def run(bundle_dir, second=None, out_json=None):
    """the checker's verdict, without its printing getting in the way of the assertions"""
    import contextlib
    import io
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        code = E.check_bundle(str(bundle_dir), str(second) if second else None, str(out_json) if out_json else None)
    return code, buf.getvalue()


def main():
    tmp = Path(tempfile.mkdtemp(prefix="qg1-evidence-"))

    # 1. the positive case: a bundle the tool wrote, checked by the checker
    good, man, man_path, binp = bundle(tmp)
    rec = Path(tmp) / "evidence.json"
    code, text = run(good, out_json=rec)
    ok(code == 0, "a complete read-only bundle verifies")
    ok("every logged frame is 0x11, 0x12 or 0x13" in text, "the frame log is checked, not assumed")
    ok(rec.is_file(), "the evidence record is written")
    doc = json.loads(rec.read_text())
    ok(doc["format"] == "qg1-flash-evidence-1" and doc["result"]["status"] == "verified-read",
       "the record says verified-read")
    ok(doc["result"]["authorises_install"] is False, "the record never authorises an install")
    ok(doc["result"]["write_capable_part"] is True, "a P25Q80H read is marked write-capable (the part, not the unit)")
    ok(doc["image"]["sha256"] == man["sha256"] and doc["image"]["size"] == 0x100000,
       "the record carries the image's size and hash")
    body = {k: v for k, v in doc.items() if k != "record_sha256"}
    import hashlib
    ok(doc["record_sha256"] == hashlib.sha256(json.dumps(body, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
       "the record's own hash covers the record")
    ok(len(doc["limits"]) >= 3 and any("restore" in s for s in doc["limits"]),
       "the record states what a read does not prove")

    # 2. one byte of the image changed: the hash no longer matches
    bad = Path(tmp) / "tampered"
    shutil.copytree(good, bad)
    img = next(bad.glob("*.bin"))
    raw = bytearray(img.read_bytes())
    raw[0x4000] ^= 0xFF
    img.write_bytes(bytes(raw))
    code, text = run(bad)
    ok(code == 1 and "the image's SHA-256 is the manifest's" in text, "a changed image byte fails the check")

    # 3. a write frame in the log: the run was not read-only, whatever it says
    bad = Path(tmp) / "wrote"
    shutil.copytree(good, bad)
    m = json.loads(next(bad.glob("*.manifest.json")).read_text())
    m["commands"] = m["commands"] + [0x30]
    next(bad.glob("*.manifest.json")).write_text(json.dumps(m))
    code, text = run(bad)
    ok(code == 1 and "every logged frame is 0x11, 0x12 or 0x13" in text, "a 0x30 in the log fails the check")

    # 4. an unknown part: the geometry cannot be trusted, so the bundle is not evidence
    bad = Path(tmp) / "unknown-part"
    shutil.copytree(good, bad)
    m = json.loads(next(bad.glob("*.manifest.json")).read_text())
    m["device"]["jedec"] = "0x123456"
    next(bad.glob("*.manifest.json")).write_text(json.dumps(m))
    code, text = run(bad)
    ok(code == 1 and "is a part this project documents" in text, "an unknown JEDEC id fails the check")

    # 5. a partial read is not a backup
    bad = Path(tmp) / "partial"
    shutil.copytree(good, bad)
    m = json.loads(next(bad.glob("*.manifest.json")).read_text())
    m["status"] = "partial"
    m["unread_ranges"] = [[0x80000, 0x1000]]
    next(bad.glob("*.manifest.json")).write_text(json.dumps(m))
    code, text = run(bad)
    ok(code == 1 and "the result is complete" in text and "no range was left unread" in text,
       "a partial read fails the check")

    # 6. two independent reads that disagree
    second, man2, _, _ = bundle(tmp, "read-02", FakeFM1(b"", query_fault="unstable"))
    code, text = run(good, second=second)
    ok(code == 1 and "the two reads have the same SHA-256" in text, "a second read that disagrees fails the check")

    # 7. two reads that agree
    second_ok, _, _, _ = bundle(tmp, "read-03")
    code, text = run(good, second=second_ok)
    ok(code == 0 and "the second read's image is byte-identical to the first" in text,
       "two independent reads that agree verify")

    # 8. a read-only part is a verified read and still not a write authorisation
    ro = FakeFM1(b"", jedec=0xB36014)
    ro_dir, ro_man, _, _ = bundle(tmp, "read-ro", ro)
    rec2 = Path(tmp) / "evidence-ro.json"
    code, text = run(ro_dir, out_json=rec2)
    doc2 = json.loads(rec2.read_text())
    ok(code == 0 and doc2["result"]["write_capable_part"] is False and doc2["result"]["authorises_install"] is False,
       "a read-only part verifies as a read and is marked not write-capable")

    print("flash evidence: %d checks, %d FAILED" % (8 + 12, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
