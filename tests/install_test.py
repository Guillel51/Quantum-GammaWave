#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_install.py against a simulated FM-1 (no hardware, no mido).
The fake device is the one in web/test_web.mjs: identity on the handshake,
then "device asks, host answers" reads of the logical image. Run from the repo root:
  python3 tests/install_test.py
Also checks logical_image/product_of against web/fm1pkg.js (needs node and
build/felucca.fwsc, skipped otherwise)."""
import hashlib
import io
import json
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1_probe as P  # noqa: E402

I.DELAY.update(open=0, start=0.01, reply=0, loader=0.01, reboot=0.01, retry=0.02, poll=0.05, hs=0.1,
               idle_check=0.4, idle_write=0.4, wait_loader=2, wait_reboot=2)
failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<64} {'ok' if cond else 'FAIL'}")
    failed += not cond


# ---------------------------------------------------------- simulated FM-1 ---

class FakeLink:
    def __init__(self, dev, gen):
        self.dev, self.gen, self.q, self.closed = dev, gen, queue.Queue(), False

    @property
    def lost(self):
        return self.gen != self.dev.gen or not self.dev.connected

    def send(self, pkt):
        if self.lost:
            return False
        self.dev.sent.append(bytes(pkt))
        self.dev.rx(bytes(pkt))
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        self.closed = True


class FakeFM1:
    """a MIDI backend with one FM-1 on it"""

    def __init__(self, image, identity="FM-1_015", name="FM-1", unplug_after=None, after_write="FM-1_965",
                 stall_after=None, bad_addr=None, ack_lost=False, ack_lost_stage1=False,
                 jedec=0x856014, query_fault=None):
        self.image, self.unplug_after, self.after_write = image, unplug_after, after_write
        self.stall_after, self.bad_addr, self.ack_lost = stall_after, bad_addr, ack_lost
        self.ack_lost_stage1 = ack_lost_stage1
        self.jedec, self.query_fault, self.queries, self.session = jedec, query_fault, 0, 0
        self.nor = bytes(range(256)) * 4096            # 1 MiB of readable NOR for 0x13
        self.served = self.bad = self.upgrades = 0
        self.sent, self.links, self.gen, self.lock = [], [], 0, threading.Lock()
        self.boot(identity, name)

    def boot(self, identity, name):
        with self.lock:
            self.gen += 1
            self.identity, self.name, self.connected = identity, name, True
            self.waiting, self.queue = None, []

    def input_names(self):
        return [self.name] if self.connected else []

    output_names = input_names

    def open(self, in_name, out_name):
        if in_name != self.name or not self.connected:
            raise IOError("no such port")
        link = FakeLink(self, self.gen)
        self.links.append(link)
        return link

    def tx(self, pkt):
        for link in self.links:
            if link.gen == self.gen and not link.closed:
                link.q.put(bytes(pkt))

    def rx(self, d):
        if d[:1] == b"\xF0" and len(d) > 8:            # 0x12 / 0x13: identification and read-only flash
            u = I.unpack7(d[1:-1])
            if len(u) >= 7 and u[0] == 0 and u[1] == 0x59 and u[2] in (0x12, 0x13):
                self.answer_query(u[2], u[6:6 + (u[3] | u[4] << 8 | u[5] << 16)])
                return
        if d == I.HS_QUERY:
            t = self.identity.encode()
            self.tx(b"\xF0" + I.pack7(bytes([0, 0x59, 0x11, 0, 0, 0]) + t + bytes(28 - len(t))) + b"\xF7")
        elif d == I.UPGRADE:
            self.upgrades += 1
            first = self.bad_addr if self.bad_addr is not None else 0
            self.queue = ([(first + k * 512, 512) for k in range(6)] + [(I.FINISH_WRITE, 8)]
                          if self.identity.startswith("ota-")
                          else [(0, 64), (0x40, 160), (0x1000, 512), (I.FINISH_CHECK, 8)])
            self.next()
        elif self.waiting:
            u = I.unpack7(d[1:-1])
            addr, n = self.waiting
            fin = addr >= I.FINISH_CHECK
            want = b"success\0" if fin else self.image[addr:addr + n]
            if u[14:14 + len(want)] != want or len(u) != 15 + len(want):
                self.bad += 1
            self.waiting = None
            self.served += 1
            if self.unplug_after and self.served >= self.unplug_after:
                self.connected = False
                return
            if self.stall_after and self.served >= self.stall_after:
                return
            if addr == I.FINISH_CHECK:
                if not self.ack_lost_stage1:
                    threading.Timer(0.05, self.boot, ("ota-FM-1_965", "Felucca Update")).start()
            elif addr == I.FINISH_WRITE:
                if not self.ack_lost:
                    threading.Timer(0.05, self.boot, (self.after_write, "Felucca")).start()
            else:
                self.next()

    def next(self):
        if not self.queue:
            return
        self.waiting = addr, n = self.queue.pop(0)
        u = bytearray([0, 0x59, 0x30, 0, 0, 0, 0]) + addr.to_bytes(4, "little") + bytes([n & 0xFF, n >> 8, 0])
        u.append(~sum(u[6:14]) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")

    def _reply(self, cmd, body):
        u = bytearray([0, 0x59, cmd, len(body) & 0xFF, (len(body) >> 8) & 0xFF, 0]) + bytearray(body)
        u.append(~sum(body) & 0xFF)
        self.tx(b"\xF0" + I.pack7(u) + b"\xF7")

    def answer_query(self, cmd, body):
        """the app's 0x12/0x13 answers, with the faults the tests inject"""
        self.queries += 1
        f = self.query_fault
        if f == "silent" or not self.connected:
            return
        if cmd == 0x12:
            nonce = int.from_bytes(body[1:5], "little") if len(body) >= 5 else 0
            jedec, cap = self.jedec, 0x100000
            if f == "truncated":
                self._reply(0x12, bytes([1, 0]) + bytes(8))
                return
            if f == "crc":
                b = bytearray(24)
                b[0], b[21] = 1, 0xAA
                self._reply(0x12, bytes(b))
                return
            if f == "unknown":
                jedec = 0x123456
            if f == "flaky":
                jedec = 0x856014 if self.queries % 2 else 0xB36014
            if f == "geometry":
                cap = 0x200000
            st = 1 if jedec == 0 else (2 if jedec not in P.PARTS else 0)
            b = bytearray(24)
            b[0], b[1] = 1, st
            b[2], b[3], b[4] = (jedec >> 16) & 0xFF, (jedec >> 8) & 0xFF, jedec & 0xFF
            if st == 0:
                page, sector, block = P.PARTS[jedec][2:]
                b[5:9] = cap.to_bytes(4, "little")
                b[9:11] = page.to_bytes(2, "little")
                b[11:13] = sector.to_bytes(2, "little")
                b[13:17] = block.to_bytes(4, "little")
                if f != "nosession":
                    self.session = ((nonce * 2654435761 + self.queries * 40503) & 0xFFFFFFFF) or 1
                    b[17:21] = self.session.to_bytes(4, "little")
            b[21:23] = P.crc16(bytes(b[:21])).to_bytes(2, "little")
            self._reply(0x12, bytes(b))
            return
        addr = int.from_bytes(body[1:5], "little") if len(body) >= 11 else 0
        n = int.from_bytes(body[5:7], "little") if len(body) >= 11 else 0
        sess = int.from_bytes(body[7:11], "little") if len(body) >= 11 else 0
        st = 0
        if f == "readfail" or (f == "readfail_at" and self.queries > 4):
            st = 6
        elif not self.session or sess != self.session:
            st = 4
        elif not n or n > 256:
            st = 6
        elif addr + n > 0x100000:
            st = 5
        h = bytearray([1, st])
        h += (0 if st else n).to_bytes(2, "little")
        h += (0 if st else (addr + (1 if f == "badaddr" else 0))).to_bytes(4, "little")
        h += (0 if st else sess).to_bytes(4, "little")
        h[12:14] = P.crc16(bytes(h[:12])).to_bytes(2, "little")
        data = b""
        if not st:
            data = bytearray(self.nor[addr:addr + n])
            if f == "unstable" and data and self.queries % 3 == 0:
                data[0] ^= 0xFF
        self._reply(0x13, bytes(h) + bytes(data))


# ----------------------------------------------------------------- helpers ---

def package(product="FM-1_965", marker=True, size=0x2000):
    raw = bytearray((i * 7) & 0xFF for i in range(size + I.BLOCKS))
    for i in range(I.BLOCKS):
        raw[i * I.BLK + I.KEEP] = (ord(product[i]) + i + 1) & 0xFF if i < len(product) else 0x7D
    raw[0x1800:0x1800 + 16] = I.LOADER_MARK if marker else bytes(16)
    return bytes(raw)


TMP = Path(tempfile.mkdtemp(prefix="felucca-install-"))


def pkgfile(name, raw, version="3.0.0", minimum="2.0.0", sign=True, product=None):
    p = TMP / name
    p.write_bytes(raw)
    (TMP / (name + ".sha256")).write_text(hashlib.sha256(raw).hexdigest() + "\n")   # (QG-1) hashes are mandatory
    return str(p)


def cli(args, dev, answer=True):
    # (QG-1) no typed JEDEC any more: the installer reads the chip with 0x12 (the fake answers it).
    # tests/preflight_test.py covers every refusal.
    args = list(args)
    out, err = io.StringIO(), io.StringIO()
    old, sys.stderr = sys.stderr, err
    try:
        rc = I.main(args, backend=dev, out=out, ask=lambda _p: answer)
    except SystemExit as e:                    # argparse: an unknown flag ends the CLI, not the test run
        rc = int(e.code or 0)
    finally:
        sys.stderr = old
    return rc, out.getvalue(), err.getvalue()


# ------------------------------------------------------------------- tests ---

def wire():
    data = bytes(range(256)) * 3
    ok(I.unpack7(I.pack7(data))[:len(data)] == data, "pack7 / unpack7 round trip")
    pkt = I.response(0x12345, b"\x01\x02\x03", fl=5)
    u = I.unpack7(pkt[1:-1])
    ok(u[:3] == b"\x00\x59\x30" and int.from_bytes(u[7:11], "little") == 0x12345 and u[14:17] == b"\x01\x02\x03"
       and ~sum(u[6:17]) & 0xFF == u[17], "response: header, address, data, checksum")
    raw = package("FM-1_906")
    ok(I.product_of(raw) == "FM-1_906" and len(I.logical_image(raw)) == len(raw) - 20, "product_of / logical_image (synthetic)")


def installs():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 0 and dev.bad == 0 and dev.upgrades == 2 and dev.served == 11 and "done: the FM-1 runs FM-1_965" in out
       and "100%" in out, f"install: running -> loader -> Felucca ({dev.served} reads)")

    dev = FakeFM1(image, identity="ota-FM-1_965", name="Felucca Update")
    rc, out, err = cli([p, "--yes"], dev)
    # the loader cannot answer 0x12; it checks the chip itself (loader.c), so a cut-off install resumes
    ok(rc == 0 and dev.upgrades == 1 and dev.bad == 0,
       "install: device already in update mode -> the write is finished")

    dev = FakeFM1(image)
    rc, out, err = cli([p], dev, answer=False)
    ok(rc == 1 and dev.upgrades == 0 and "cancelled" in out, "install: answer no -> nothing sent but the handshake")

    dev = FakeFM1(image, identity="FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and "running" in out and dev.upgrades == 0, "--info: identity of the connected FM-1")

    dev = FakeFM1(image, identity="ota-FM-1_965", name="Felucca Update")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "update loader" in out, "--info: device in update mode")

    dev = FakeFM1(image, identity="\x1b[2J\x1b]0;x\x07FM-1_905", name="Felucca")
    rc, out, err = cli(["--info"], dev)
    ok(rc == 0 and "FM-1_905" in out and not any(c in out for c in "\x1b\x07"),
       "--info: control bytes in the device's identity are not printed")

def errors():
    raw = package()
    image = I.logical_image(raw)
    p = pkgfile("ok.fwsc", raw)

    rc, out, err = cli([p, "--yes"], FakeFM1(image, name="IAC Driver Bus 1"))
    ok(rc == 3 and "not found" in err and "IAC Driver" in err, "no FM-1 port -> exit 3, lists the MIDI inputs")
    dev = FakeFM1(image)
    rc, out, err = cli([p, "--yes", "--port", "Felucca"], dev)
    ok(rc == 3 and dev.upgrades == 0, "--port that matches nothing -> exit 3")
    dev = FakeFM1(image, name="My Interface")
    rc, out, err = cli(["--info", "--port", "my int"], dev)
    ok(rc == 0 and "FM-1_015" in out, "--port picks a port the default match skips")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, unplug_after=2))
    ok(rc == 4 and "disconnected" in err and "nothing was written" in err and time.monotonic() - t0 < 3,
       "unplugged in step 1 -> exit 4 'lost', nothing written")
    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_965", name="Felucca Update", unplug_after=3))
    ok(rc == 4 and "disconnected" in err and time.monotonic() - t0 < 3,
       "unplugged in update mode -> exit 4 'lost'")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_965", name="Felucca Update", stall_after=3))
    ok(rc == 4, "a loader that stops answering -> exit 4 'stopped'")
    rc, out, err = cli([p, "--yes"], FakeFM1(image, identity="ota-FM-1_965", name="Felucca Update", bad_addr=len(image)))
    ok(rc != 0 and "outside the package" in err, "a loader asking outside the package is refused")

    rc, out, err = cli([p, "--yes"], FakeFM1(image, after_write="FM-1_015"))
    ok(rc == 6 and "reports FM-1_015" in err, "another identity after the restart -> exit 6")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, ack_lost_stage1=True))
    ok(rc == 5 and "loader did not appear" in err and "done:" not in out and time.monotonic() - t0 < 5,
       "step-1 ACK lost -> no loader, exit 5, never done")

    t0 = time.monotonic()
    rc, out, err = cli([p, "--yes"], FakeFM1(image, ack_lost=True))
    ok(rc == 5 and "did not come back" in err and "done:" not in out and time.monotonic() - t0 < 4,
       "final ACK lost -> no success, exit 5 after the reboot wait")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([p, "--yes"], dev)
    ok(rc == 6 and dev.upgrades == 0, "another model -> exit 6, nothing sent")

    class Stuck(FakeFM1):          # the loader never shows up
        def boot(self, identity, name):
            super().boot(identity, name)
            if identity.startswith("ota-"):
                self.connected = False
    rc, out, err = cli([p, "--yes"], Stuck(image))
    ok(rc == 5 and "loader did not appear" in err, "no loader after step 1 -> exit 5")

    plain = pkgfile("plain.fwsc", package(marker=False))
    dev = FakeFM1(image)
    rc, out, err = cli([plain, "--yes"], dev)
    ok(rc == 2 and "no Felucca update loader" in err and dev.sent == [], "package without the loader marker -> exit 2, no MIDI")
    dev = FakeFM1(I.logical_image(package(marker=False)))
    rc, out, err = cli([plain, "--yes", "--force"], dev)
    ok(rc == 2 and "unrecognized arguments" in err and dev.sent == [],
       "... and there is no --force to override it: the flag is not accepted, no MIDI sent")
    rc, out, err = cli([pkgfile("short.fwsc", b"\0" * 100), "--yes"], FakeFM1(image))
    ok(rc == 2 and "too short" in err, "not a package -> exit 2")
    rc, out, err = cli([str(TMP / "missing.fwsc"), "--yes"], FakeFM1(image))
    ok(rc == 2, "missing file -> exit 2")


def integrity():
    """(QG-1) SHA-256 mandatory: --sha256, a .sha256 / SHA256SUMS next to the package; --verify-only writes nothing"""
    raw = package()
    image = I.logical_image(raw)
    h = hashlib.sha256(raw).hexdigest()
    d = TMP / "sums"
    d.mkdir(exist_ok=True)
    p = d / "q.fwsc"
    p.write_bytes(raw)
    plain = d / "plain.fwsc"
    plain.write_bytes(raw)
    dev = FakeFM1(image)
    rc, out, err = cli([str(plain), "--yes"], dev)
    ok(rc == 2 and "no SHA-256 published" in err and dev.sent == [],
       "no SHA-256 published -> refused (exit 2), no MIDI")
    dev = FakeFM1(image)
    rc, out, err = cli([str(p), "--yes", "--sha256", h], dev)
    ok(rc == 0 and f"sha256:  {h}  (verified)" in out, "--sha256 that matches: installs, says verified")
    dev = FakeFM1(image)
    rc, out, err = cli([str(p), "--yes", "--sha256", "0" * 64], dev)
    ok(rc == 2 and "does not match" in err and dev.sent == [], "--sha256 that does not match -> exit 2, no MIDI")
    (d / "SHA256SUMS").write_text(f"{'1' * 64}  other.fwsc\n{'f' * 64}  q.fwsc\n")
    dev = FakeFM1(image)
    rc, out, err = cli([str(p), "--yes"], dev)
    ok(rc == 2 and "SHA256SUMS" in err and dev.sent == [], "SHA256SUMS next to it, wrong line for it -> exit 2, no MIDI")
    (d / "SHA256SUMS").write_text(f"{h} *q.fwsc\n")
    dev = FakeFM1(image)
    rc, out, err = cli([str(p), "--yes"], dev)
    ok(rc == 0 and "(verified)" in out, "SHA256SUMS that matches: installs")
    (d / "SHA256SUMS").unlink()
    (d / "q.fwsc.sha256").write_text(h + "\n")
    dev = FakeFM1(image)
    rc, out, err = cli([str(p), "--verify-only"], dev)
    ok(rc == 0 and "nothing was written" in out and dev.upgrades == 0 and "(verified)" in out,
       "--verify-only: package and device checked, no update started")
    dev = FakeFM1(image, identity="XY-9_001", name="usb-midi")
    rc, out, err = cli([str(p), "--verify-only"], dev)
    ok(rc == 6 and dev.upgrades == 0, "--verify-only on another model -> exit 6")


def against_js():
    clean = ROOT / "build/felucca.fwsc"
    if not shutil.which("node") or not clean.exists():
        print("logical image vs fm1pkg.js: skipped (needs node and build/felucca.fwsc)")
        return
    js = ("import { logicalImage, productOf } from %r; import { readFileSync } from 'node:fs';"
          "const p = readFileSync(process.argv[1]); process.stderr.write(productOf(p));"
          "process.stdout.write(logicalImage(p));") % str(ROOT / "web/fm1pkg.js")
    r = subprocess.run(["node", "--input-type=module", "-e", js, str(clean)], capture_output=True, check=True)
    raw = clean.read_bytes()
    ok(I.logical_image(raw) == r.stdout and I.product_of(raw) == r.stderr.decode(),
       f"logical_image / product_of == fm1pkg.js ({clean.name}, {I.product_of(raw)})")
    ok(I.LOADER_MARK in raw, f"{clean.name} carries the Felucca loader marker")


def official():
    """#32: back to the official V15 from Felucca, without --force (only the unmodified file)"""
    v15 = ROOT / "FM-1_v15.fwsc"
    if not v15.exists():
        print("official V15 restore: skipped (needs FM-1_v15.fwsc in the repo root)")
        return
    raw = v15.read_bytes()
    dev = FakeFM1(I.logical_image(raw), identity="FM-1_965", name="Felucca", after_write="FM-1_015")
    rc, out, err = cli([str(v15), "--yes", "--sha256", hashlib.sha256(raw).hexdigest()], dev)
    ok(rc == 0 and dev.bad == 0 and "FM-1_015" in out and dev.identity == "FM-1_015",
       "official V15 (FM-1.fwsc) from Felucca: installed without --force, back as FM-1_015")
    bad = bytearray(raw)
    bad[-1] ^= 1
    dev = FakeFM1(I.logical_image(bytes(bad)), identity="FM-1_965", name="Felucca")
    rc, out, err = cli([pkgfile("v15mod.fwsc", bytes(bad)), "--yes"], dev)
    ok(rc == 2 and "official V15" in err and dev.sent == [], "a modified V15 is still refused (no MIDI)")


def pairing():
    """#(QG-1 v1.8.7): an output only pairs with an input of the same name.
    The old fallback took the only output on the bus whatever it was called, so a package
    could be served to a device that never answered the handshake."""
    ok(I.pair_output("FM-1", ["FM-1"]) == "FM-1", "pairing: the exact name wins")
    ok(I.pair_output("FM-1 2", ["FM-1"]) == "FM-1", "pairing: Windows' numbered port matches its base")
    ok(I.pair_output("FM-1", ["Some Synth"]) is None, "pairing: one unrelated output is not the FM-1")
    ok(I.pair_output("FM-1", ["FM-1", "Some Synth"]) == "FM-1", "pairing: the named output among several")
    ok(I.pair_output("FM-1", []) is None, "pairing: no outputs, no pair")


def main():
    pairing()
    wire()
    integrity()
    installs()
    errors()
    against_js()
    official()
    print(f"INSTALL TESTS FAILED ({failed})" if failed else "install tests passed")
    return 1 if failed else 0


if __name__ == "__main__":
    _rc = main()
    shutil.rmtree(TMP, ignore_errors=True)
    sys.exit(_rc)
