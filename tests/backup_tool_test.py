#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_backup.py against a simulated FM-1 (no hardware, no mido).

The simulated device speaks the same archive protocol as the firmware
(web/fm1backup.js: F0 7D 46 4C, LIST / GET / PUT). Every case below is a property the
tool must hold: two independent reads that agree, a device that changes under the reader,
an unplugged cable, an archive that was altered after it was written, a selective restore
that must say it is partial, and a write to the wrong model or the wrong object geometry.

  python3 tests/backup_tool_test.py
"""
import hashlib
import io
import json
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_backup as B  # noqa: E402
from fm1_install import pack7  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<70} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ------------------------------------------------------------ fake device ---

def frame(cmd, payload):
    return [0xF0, *B.HEADER[1:], cmd, *payload, 0xF7]


class FakeLink:
    def __init__(self, dev, watch, log):
        self.dev, self.q, self.closed = dev, [], False

    @property
    def lost(self):
        return self.dev.gone

    def drain(self):
        self.q.clear()

    def close(self):
        self.closed = True

    def read_reply(self, cmd, timeout):
        for i, d in enumerate(self.q):
            if d[4] == cmd:
                del self.q[i]
                return d[5:-1]
        return None

    def send(self, pkt):
        if self.dev.gone:
            return False
        pkt = bytes(pkt)
        if pkt[:4] == bytes(B.HEADER) and pkt[-1] == 0xF7:
            self.dev.on_backup(pkt[4], list(pkt[5:-1]), self)
            return True
        if pkt == B.HS_QUERY:
            self.q.append(self.dev.identity_frame())
            return True
        return False


class FakeFM1:
    """an FM-1 whose archive objects are dicts id -> bytes"""

    def __init__(self, identity="FM-1_900", objects=None, gone=False):
        self.identity, self.gone, self.reads, self.writes = identity, gone, 0, []
        self.objects = objects or {i: bytes([i]) * (64 + i) for i in B.IDS}
        self.hold = None      # (id, size, crc, buf): a PUT in progress
        self.mutate = None    # (object id, new bytes) applied before the second LIST
        self.lists = 0
        self.gone_after = None
        self.fail_crc = False

    # handshake -------------------------------------------------------------
    def identity_frame(self):
        text = self.identity.encode()[:27]
        body = [0, 0x59, 0x11, 0, 0, 0] + list(text) + [0] * (27 - len(text)) + [0]
        return [0xF0, *pack7(bytes(body)), 0xF7]

    # archive protocol ------------------------------------------------------
    def on_backup(self, cmd, args, link):
        if self.gone:
            return
        if cmd == B.LIST:
            self.lists += 1
            if self.mutate and self.lists == 2:      # the device moved between the two passes
                i, data = self.mutate
                self.objects[i] = data
                self.mutate = None
            payload = [1, 0, len(self.objects)]
            for i in sorted(self.objects):
                d = self.objects[i]
                payload += [i, *B.u32(len(d)), *B.u32(zlib.crc32(d) & 0xFFFFFFFF)]
            link.q.append(frame(B.LIST, payload))
            return
        if cmd == B.GET:
            oid, off = args[0], B.r32(args, 1)
            size = args[6] | args[7] << 7
            self.reads += 1
            if self.gone_after and self.reads > self.gone_after:
                self.gone = True
                return
            d = self.objects.get(oid)
            if d is None:
                link.q.append(frame(B.GET, [oid, 1, *B.u32(off), 0, 0]))
                return
            chunk = d[off:off + size]
            link.q.append(frame(B.GET, [oid, 0, *B.u32(off), len(chunk) & 127,
                                        (len(chunk) >> 7) & 127, *B.pack(chunk)]))
            return
        if cmd == B.PUT:
            what = args[0]
            oid = args[1]
            if what == 0:
                size, crc = B.r32(args, 2), B.r32(args, 7)
                self.hold = [oid, size, crc, bytearray()]
                link.q.append(frame(B.PUT, [0, oid, 0]))
            elif what == 1 and self.hold:
                off = B.r32(args, 2)
                packed = args[7:]
                # each group is one mask byte plus up to seven 7-bit bytes
                size = (len(packed) // 8) * 7 + (len(packed) % 8 - 1 if len(packed) % 8 else 0)
                data = B.unpack(packed, size)
                self.hold[3][off:off + len(data)] = data
                link.q.append(frame(B.PUT, [0, oid, 0]))
            elif what == 2 and self.hold:
                oid0, size, crc, buf = self.hold
                good = len(buf) == size and (zlib.crc32(bytes(buf)) & 0xFFFFFFFF) == crc
                if good and not self.fail_crc:
                    self.objects[oid0] = bytes(buf)
                    self.writes.append(oid0)
                    link.q.append(frame(B.PUT, [0, oid0, 0]))
                else:
                    link.q.append(frame(B.PUT, [0, oid0, 2]))
                self.hold = None
            elif what == 3:
                self.hold = None
                link.q.append(frame(B.PUT, [0, oid, 0]))
            return


class FakeBackend:
    def __init__(self, dev):
        self.dev = dev
        self.names = ["FM-1"]

    def input_names(self):
        return list(self.names)

    def output_names(self):
        return list(self.names)

    def open_backup(self, name, out, watch, log):
        return FakeLink(self.dev, watch, log)


def run(args, dev, out=None):
    import contextlib
    out = out or io.StringIO()
    log = io.StringIO()
    err = io.StringIO()
    B.DEFAULT_DIR = Path(tempfile.mkdtemp(prefix="qg1-bk-"))
    with contextlib.redirect_stderr(err):
        rc = B.main(args, backend=FakeBackend(dev), out=out, log_stream=log)
    return rc, out.getvalue() + err.getvalue(), log.getvalue()


# ------------------------------------------------------------------ cases ---

def backup_complete(dev, outdir):
    rc, txt, log = run(["backup", "--out", str(outdir)], dev)
    files = list(Path(outdir).glob("*.json"))
    ok(rc == 0 and "complete" in txt and len(files) == 1, "backup: two agreeing reads -> complete archive")
    if not files:
        return None, rc, txt
    doc = json.loads(files[0].read_text())
    ok(doc["qg1"]["result"] == "complete" and doc["qg1"]["passes"] == 2,
       "backup: manifest records the two passes and the result")
    ok(doc["qg1"]["pass_sha256"][0] == doc["qg1"]["pass_sha256"][1],
       "backup: the two passes have the same digest")
    ok(doc["qg1"]["file_sha256"] == B.archive_digest(doc), "backup: file SHA-256 matches its manifest")
    ok(all(o["sha256"] == hashlib.sha256(o["bytes"]).hexdigest() for o in B.load_archive(files[0])["objects"]),
       "backup: every object's SHA-256 verifies")
    ok(sorted(doc["qg1"]["capacity"]["ids"]) == sorted(B.IDS), "backup: every object the device exposes is in it")
    return files[0], rc, txt


def backup_inconsistent():
    dev = FakeFM1()
    dev.mutate = (3, bytes([0xAA]) * 200)
    outdir = Path(tempfile.mkdtemp())
    rc, txt, log = run(["backup", "--out", str(outdir)], dev)
    ok(rc == 5 and "changed between" in (txt + log), "backup: a device that changes between reads is refused")
    ok(not list(outdir.glob("*.json")), "backup: nothing is written for an inconsistent read")


def backup_lost():
    dev = FakeFM1()
    dev.gone_after = 4
    outdir = Path(tempfile.mkdtemp())
    rc, txt, log = run(["backup", "--out", str(outdir)], dev)
    ok(rc == 4, "backup: a cable pulled mid-read is reported as lost (not as an empty archive)")
    ok(not list(outdir.glob("*.json")), "backup: nothing is written after a disconnect")


def backup_private(tmp):
    dev = FakeFM1()
    repo = ROOT / "artifacts"
    path = repo / "should-not-exist.json"
    rc, txt, log = run(["backup", "--out", str(path)], dev)
    ok(rc == 2 and "inside a git repository" in (txt + log),
       "backup: refuses to write a personal archive inside the repo")
    ok(not path.exists(), "backup: the refused archive was not created")


def restore_cases(archive, dev):
    rc, txt, log = run(["restore", str(archive), "--yes"], dev)
    ok(rc == 0 and "complete" in txt, "restore: a verified archive restores completely")
    ok(sorted(set(dev.writes)) == sorted(B.IDS), "restore: every object was written")
    rc, txt, log = run(["restore", str(archive), "--yes", "--only", "3"], dev)
    ok(rc == 0 and "partial (selective)" in txt and "not a full recovery" in txt,
       "restore: a selective restore says it is partial")
    ok(dev.writes[-1] == 3, "restore: --only writes only that object")


def restore_altered(archive):
    doc = json.loads(Path(archive).read_text())
    raw = bytearray(doc["objects"][4]["data"].encode())
    raw[3] = ord("A") if raw[3] != ord("A") else ord("B")
    doc["objects"][4]["data"] = raw.decode()
    bad = Path(tempfile.mkdtemp()) / "altered.json"
    bad.write_text(json.dumps(doc))
    dev = FakeFM1()
    rc, txt, log = run(["restore", str(bad), "--yes"], dev)
    ok(rc == 2 and dev.writes == [], "restore: an altered archive is refused before the first write")

    doc = json.loads(Path(archive).read_text())
    doc["objects"][0]["crc"] ^= 1
    bad.write_text(json.dumps(doc))
    rc, txt, log = run(["restore", str(bad), "--yes"], dev)
    ok(rc == 2 and dev.writes == [], "restore: a CRC that does not match is refused before writing")


def restore_mismatch(archive):
    dev = FakeFM1(identity="ota-FM-1_015")
    rc, txt, log = run(["restore", str(archive), "--yes"], dev)
    ok(rc != 0 and dev.writes == [], "restore: refuses an FM-1 in update mode")
    dev = FakeFM1(identity="FM-1_015")
    rc, txt, log = run(["restore", str(archive), "--yes"], dev)
    ok(rc == 0, "restore: the same model is accepted")
    doc = json.loads(Path(archive).read_text())
    doc["qg1"]["device"]["model"] = "FM-9"
    doc["qg1"]["file_sha256"] = B.archive_digest(doc)
    other = Path(tempfile.mkdtemp()) / "other.json"
    other.write_text(json.dumps(doc))
    dev = FakeFM1()
    rc, txt, log = run(["restore", str(other), "--yes"], dev)
    ok(rc == 6 and dev.writes == [], "restore: a different model is refused before writing")


def verify_cases(archive):
    rc, txt, log = run(["verify", str(archive)], FakeFM1())
    ok(rc == 0 and "complete" in txt, "verify: a good archive verifies")
    doc = json.loads(Path(archive).read_text())
    q = doc.pop("qg1")
    bad = Path(tempfile.mkdtemp()) / "nomanifest.json"
    bad.write_text(json.dumps(doc))
    rc, txt, log = run(["verify", str(bad)], FakeFM1())
    ok(rc == 5 and "manifest" in (txt + log), "verify: an archive with no manifest is unverifiable")
    doc["qg1"] = q
    doc["qg1"]["file_sha256"] = "0" * 64
    bad.write_text(json.dumps(doc))
    rc, txt, log = run(["verify", str(bad)], FakeFM1())
    ok(rc == 2, "verify: a file whose hash does not match its manifest is refused")


def main():
    tmp = Path(tempfile.mkdtemp())
    dev = FakeFM1()
    archive, rc, txt = backup_complete(dev, tmp)
    backup_inconsistent()
    backup_lost()
    backup_private(tmp)
    if archive:
        restore_cases(archive, FakeFM1())
        restore_altered(archive)
        restore_mismatch(archive)
        verify_cases(archive)
    print(f"BACKUP TOOL TESTS FAILED ({failed})" if failed else "backup tool tests passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
