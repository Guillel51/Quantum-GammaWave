#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""0x12 identification and 0x13 read-only reads against a simulated FM-1.

Every rejection the firmware can make is exercised here: an unknown part, no
answer, a truncated reply, a bad CRC, two readings that disagree, a geometry
that does not match the part, no session, a stale session, an out-of-range
address, a zero length, more than one chunk, a read error, a disconnection.
Then the backup tool: two passes, byte comparison, SHA-256, manifest, and the
proof that it sends no frame that could write.

  python3 tests/chip_test.py
"""
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))
import fm1_backup as B  # noqa: E402
import fm1_install as I  # noqa: E402
import fm1_probe as P  # noqa: E402
from install_test import FakeFM1  # noqa: E402

I.DELAY.update(open=0, hs=0.05, retry=0.01)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<66} {'ok' if cond else 'FAIL'}")
    failed += not cond


def link(dev):
    return dev.open(dev.name, dev.name)


def fails(code, fn, what):
    try:
        fn()
    except P.ProbeError as e:
        ok(e.code == code, "%s -> %s" % (what, e.code))
        return
    ok(False, "%s -> no error" % what)


def ident():
    dev = FakeFM1(b"", jedec=0x856014)
    ln = link(dev)
    bad = P.identify(ln, 0x11223344, 0.2)
    ok(bad["jedec"] == 0x856014 and bad["cap"] == 0x100000 and bad["part"] == "P25Q80H",
       "0x12 identifies the part and its geometry")
    ok((bad["page"], bad["sector"], bad["block"]) == (256, 0x1000, 0x10000), "page / sector / block")
    ev = P.probe(ln, 0.2)
    ok(ev["readings"] == 2 and ev["source"] == "device-0x12", "probe: two readings")
    ok(ev["session"] == bad["session"] + 40503 or ev["session"] != bad["session"], "probe: fresh session")


def rejections():
    for fault, code, what in (("unknown", "unknown-part", "unknown JEDEC"),
                              ("truncated", "frame", "truncated reply"),
                              ("crc", "crc", "bad reply CRC"),
                              ("flaky", "mismatch", "two readings disagree"),
                              ("geometry", "geometry", "geometry not the part's"),
                              ("nosession", "no-session", "no session token")):
        dev = FakeFM1(b"", query_fault=fault)
        fails(code, lambda d=dev: P.probe(link(d), 0.2), what)
    dev = FakeFM1(b"", query_fault="silent")
    fails("timeout", lambda: P.probe(link(dev), 0.05), "no answer")
    dev = FakeFM1(b"")
    ln = link(dev)
    dev.connected = False
    fails("lost", lambda: P.probe(ln, 0.2), "device disconnected")
    dev = FakeFM1(b"", jedec=0)
    fails("no-nor-answer", lambda: P.probe(link(dev), 0.2), "the NOR does not answer")


def reads():
    dev = FakeFM1(b"")
    ln = link(dev)
    ev = P.probe(ln, 0.2)
    got = P.read(ln, 0x1234, 256, ev["session"], 0.2)
    ok(got == dev.nor[0x1234:0x1234 + 256], "0x13 returns exactly the NOR's bytes")
    fails("length", lambda: P.read(ln, 0, 0, ev["session"], 0.2), "zero length")
    fails("length", lambda: P.read(ln, 0, 257, ev["session"], 0.2), "more than one chunk")
    fails("range", lambda: P.read(ln, 0x100000 - 8, 16, ev["session"], 0.2), "past the end")
    fails("no-session", lambda: P.read(ln, 0, 16, ev["session"] ^ 1, 0.2), "stale session")
    dev2 = FakeFM1(b"", query_fault="readfail")
    ln2 = link(dev2)
    ev2 = P.probe(ln2, 0.2)
    fails("length-or-read", lambda: P.read(ln2, 0x100, 256, ev2["session"], 0.2), "device read error")
    dev3 = FakeFM1(b"", query_fault="badaddr")
    ln3 = link(dev3)
    ev3 = P.probe(ln3, 0.2)
    fails("address", lambda: P.read(ln3, 0x100, 16, ev3["session"], 0.2), "reply for another address")
    got, errs = P.read_range(ln, 0x1000, 1024, ev["session"], 0.2)
    ok(len(got) == 1024 and not errs and got == dev.nor[0x1000:0x1400], "chunked read range")


def backups():
    out = Path(tempfile.mkdtemp(prefix="qg1-backup-"))
    dev = FakeFM1(b"")
    ln = link(dev)
    man, path, binp = B.backup(ln, out)
    ok(man["status"] == "complete", "backup: complete")
    ok(man["sha256"] == __import__("hashlib").sha256(dev.nor).hexdigest(), "backup: sha256 of the part")
    ok(man["sha256_pass_a"] == man["sha256_pass_b"], "backup: the two passes agree")
    ok(man["size"] == 0x100000 and man["expected_size"] == 0x100000, "backup: exact size")
    ok(set(man["commands_sent"]) <= {0x12, 0x13} and man["command_counts"].get("0x12") == 2
       and "0x30" not in man["command_counts"] and dev.upgrades == 0 and dev.served == 0,
       "backup records the 0x12 pair and sends read frames only (no 0x30, no UPGRADE)")
    ok(man["commands"][:2] == [0x12, 0x12] and man["frames"] == len(man["commands"]),
       "backup manifest keeps the real order: identification first, then every read")
    ok(man["device"]["jedec"] == "0x856014" and man["device"]["page"] == 256, "manifest: identity and geometry")
    ok(json.loads(path.read_text())["status"] == "complete" and binp.exists(), "manifest on disk")

    dev = FakeFM1(b"", query_fault="unstable")
    man, path, _ = B.backup(link(dev), out)
    ok(man["status"] == "inconsistent" and not man["identical"], "backup: passes differ -> inconsistent")
    ok(man["sha256"] == "" and man["files"]["pass_b"], "inconsistent keeps both passes, no final hash")

    dev = FakeFM1(b"", query_fault="readfail_at")
    man, path, _ = B.backup(link(dev), out, max_errors=2)
    ok(man["status"] == "partial" and man["unread_ranges"], "backup: unread ranges -> partial, not complete")
    ok(bool(man["errors"]), "partial backup keeps the errors")


def main():
    ident()
    rejections()
    reads()
    backups()
    print("chip / backup test %s (%d failed)" % ("passed" if not failed else "FAILED", failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
