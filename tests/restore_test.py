#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""tools/fm1_restore.py against synthetic archives and a simulated NOR.

Checks the archive validation (complete, both passes, size, hash, chip, read-only frame log), the
refusal to write, the chip/geometry check and the byte-by-byte comparison. No hardware, no mido:
the simulated unit answers the same 0x12/0x13 frames fm1_probe.py sends.

  python3 tests/restore_test.py
"""
import hashlib
import json
import queue
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_probe as P  # noqa: E402
import fm1_restore as R  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print(f"{what:<76} {'ok' if cond else 'FAIL'}")
    failed += not cond


class FakeNor:
    """answers 0x12 (identification) and 0x13 (read) from a byte array"""

    def __init__(self, data, jedec=0x856014, geometry=(0x100000, 256, 0x1000, 0x10000)):
        self.data, self.jedec, self.geometry = data, jedec, geometry
        self.session = 0
        self.q = queue.Queue()
        self.sent = []
        self.closed = False
        self.lost = False

    def _ident(self, nonce):
        self.session += 1
        cap, page, sector, block = self.geometry
        body = bytes([P.PROTO, 0]) + self.jedec.to_bytes(3, "big")
        body += struct.pack("<IHH I", cap, page, sector, block)
        body += struct.pack("<I", self.session)
        body += struct.pack("<H", P.crc16(body)) + b"\0"
        return P.frame(0x12, body)

    def _read(self, body):
        addr, n, session = struct.unpack_from("<IHI", body, 1)
        if session != self.session or addr + n > len(self.data):
            reply = bytes([P.PROTO, 5]) + struct.pack("<H", 0) + struct.pack("<I", addr) + struct.pack("<I", session)
        else:
            reply = bytes([P.PROTO, 0]) + struct.pack("<H", n) + struct.pack("<I", addr) + \
                    struct.pack("<I", session)
        reply += struct.pack("<H", P.crc16(reply))
        if reply[1] == 0:
            reply += bytes(self.data[addr:addr + n])
        return P.frame(0x13, reply)

    def send(self, pkt):
        self.sent.append(bytes(pkt))
        cmd, body = P.decode(pkt)
        if cmd == 0x12:
            assert body[0] == P.PROTO
            self.q.put(self._ident(body[1:5]))
        elif cmd == 0x13:
            self.q.put(self._read(body))
        else:
            raise AssertionError(f"the tool sent 0x{cmd:02X}, which is not read-only")
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


def archive(tmp, data, jedec="0x856014", **over):
    raw = bytes(data)
    sha = hashlib.sha256(raw).hexdigest()
    man = {
        "format": R.FORMAT, "tool": "fm1_backup 1.0", "created": "2026-11-04T00:00:00Z",
        "device": {"identity": "FM-1_015", "product": "FM-1", "firmware": "v1.8.7",
                   "jedec": jedec, "part": "P25Q80H", "capacity": 0x100000, "page": 256,
                   "sector": 0x1000, "block": 0x10000, "identification": "0x12, two readings",
                   "nonce": "deadbeef"},
        "passes": 2, "chunk": 256, "size": len(raw), "expected_size": 0x100000,
        "sha256_pass_a": sha, "sha256_pass_b": sha, "sha256": sha, "identical": True,
        "status": "complete", "unread_ranges": [], "errors": [],
        "commands": [0x11, 0x12, 0x12], "command_counts": {"0x11": 1, "0x12": 2},
        "frames": 3, "commands_sent": [0x11, 0x12],
        "files": {"pass_a": "FM-1_015.bin", "pass_b": None}, "seconds": 1.0,
        "restore": "not implemented and not verified",
    }
    man.update(over)
    d = Path(tmp)
    (d / "FM-1_015.bin").write_bytes(raw)
    p = d / "FM-1_015.manifest.json"
    p.write_text(json.dumps(man, indent=2), encoding="utf-8")
    return p


def refuses(fn, code):
    try:
        fn()
        return False, "no error"
    except R.RestoreError as e:
        return e.code == code, e.code


def main():
    image = bytes((i * 37 + (i >> 5)) & 0xFF for i in range(0x100000))
    with tempfile.TemporaryDirectory() as tmp:
        man_path = archive(tmp, image)
        man = json.loads(man_path.read_text())
        raw = (Path(tmp) / "FM-1_015.bin").read_bytes()

        facts = R.validate(man, raw, Path(tmp) / "FM-1_015.bin")
        ok(facts["size"] == 0x100000 and facts["jedec"] == "0x856014", "a complete two-pass archive validates")

        # a directory holding exactly one manifest works, a bare image does not
        found = R.find_backup(Path(tmp))
        ok(found[0] == man_path and found[1].name == "FM-1_015.bin", "a directory with one manifest is found")
        bare = R.find_backup(Path(tmp) / "FM-1_015.bin")
        ok(bare[0] is None, "a bare .bin resolves to no manifest (nothing to verify it against)")
        got, why = refuses(lambda: R.run(type("A", (), {"backup": str(Path(tmp) / "FM-1_015.bin"),
                                                          "write": False, "compare": False,
                                                          "device": False, "port": None})()), "archive")
        ok(got, "a bare .bin is refused by run(): an image alone cannot be verified (code archive)")

        for name, mutate, code in [
            ("one byte changed", lambda m, r: (m, r[:100] + bytes([r[100] ^ 1]) + r[101:]), "hash"),
            ("the file is truncated", lambda m, r: (m, r[:-4096]), "size"),
            ("the manifest says partial", lambda m, r: ({**m, "status": "partial"}, r), "incomplete"),
            ("the passes are not identical", lambda m, r: ({**m, "identical": False}, r), "incomplete"),
            ("one pass only", lambda m, r: ({**m, "passes": 1}, r), "incomplete"),
            ("a write frame in the log", lambda m, r: ({**m, "commands_sent": [0x11, 0x30]}, r), "readonly"),
            ("an unknown chip", lambda m, r: ({**m, "device": {**m["device"], "jedec": "0x123456"}}, r), "chip"),
            ("a 512 KiB geometry", lambda m, r: ({**m, "device": {**m["device"], "capacity": 0x80000}}, r), "geometry"),
            ("the wrong format", lambda m, r: ({**m, "format": "qg1-backup-9"}, r), "format"),
        ]:
            m2, r2 = mutate(json.loads(json.dumps(man)), raw)
            got, why = refuses(lambda: R.validate(m2, r2, None), code)
            ok(got, f"refused: {name} ({why})")

        # --write never reaches the device
        class NoMidi:
            def __getattr__(self, name):
                raise AssertionError("--write must not open MIDI")
        saved = R.open_device
        R.open_device = NoMidi
        try:
            got, why = refuses(lambda: R.run(type("A", (), {"backup": str(man_path), "write": True,
                                                            "compare": False, "device": False, "port": None})()),
                               "write")
            ok(got, "--write is refused before any MIDI frame (code write)")
        finally:
            R.open_device = saved

        # the device: a matching unit, a chip mismatch, and the comparison
        link = FakeNor(image)
        ev = P.probe(link)
        R.check_device(facts, ev)
        ok(True, "the unit's 0x12 evidence matches the archive (chip, capacity, geometry)")
        ok(len(link.sent) == 2 and all(P.decode(p)[0] == 0x12 for p in link.sent),
           "identifying the unit sends only 0x12, twice")

        diffs = R.compare(link, raw, ev["session"])
        ok(diffs == [], "compare: a unit holding the backup reports no differences")

        other = bytearray(image)
        other[0x100:0x110] = bytes(16)          # a changed sector
        other[0x90000] ^= 0xFF                  # a single changed byte
        link2 = FakeNor(bytes(other))
        ev2 = P.probe(link2)
        diffs = R.compare(link2, raw, ev2["session"])
        ok(diffs == [[0x100, 0x110], [0x90000, 0x90001]], f"compare: the two changed ranges are reported ({diffs})")

        try:
            R.check_device(facts, P.probe(FakeNor(image, jedec=0xB36014)))
            ok(False, "a unit with another chip must not match the archive")
        except R.RestoreError as e:
            ok(e.code == "chip", "a unit with another chip is refused (code chip)")
        # a part whose geometry contradicts its JEDEC never reaches check_device: 0x12 refuses it first
        try:
            R.check_device(facts, P.probe(FakeNor(image, geometry=(0x100000, 512, 0x1000, 0x10000))))
            ok(False, "a unit with another page size must not match the archive")
        except P.ProbeError as e:
            ok(e.code == "geometry", "a unit with another geometry is refused at 0x12 (code geometry)")
        except R.RestoreError as e:
            ok(e.code == "geometry", "a unit with another geometry is refused (code geometry)")

        # --- the write path: the part is erased, programmed and read back, sector by sector ---
        nor_path = Path(tmp) / "part.nor"
        ok(R.main(["--backup", str(man_path), "--write", "--programmer", f"file:{nor_path}"]) == 0,
           "write: a blank part is erased, programmed and reads back like the backup (exit 0)")
        ok(nor_path.read_bytes() == raw, "write: the simulated part holds the image byte for byte")

        class Stuck(R.NorFile):
            """a part whose first byte of every page will not program: the readback must catch it"""

            def program(self, addr, chunk):
                super().program(addr, chunk)
                self.data[addr] |= 0x01

        got, why = refuses(lambda: R.write_nor(raw, Stuck(Path(tmp) / "stuck.nor"), 0x1000, 256), "program")
        ok(got, f"write: a bit that does not program is caught by the sector readback ({why})")

        ok(R.main(["--backup", str(man_path), "--write", "--programmer", "nope:x"]) == 2,
           "write: an unknown programmer is refused with exit 2")

        # an external SPI programmer: a stand-in flashrom, so the command line and the readback compare run
        bindir = Path(tmp) / "bin"
        bindir.mkdir()
        chip = Path(tmp) / "chip.bin"
        chip.write_bytes(b"\xff" * 0x100000)
        fake = bindir / "flashrom"
        fake.write_text("#!/bin/sh\nwhile [ $# -gt 0 ]; do\n"
                        f"  case \"$1\" in -w) cp \"$2\" {chip} ;; -r) cp {chip} \"$2\" ;; esac\n"
                        "  shift\ndone\n")
        fake.chmod(0o755)
        saved_flashrom = R.FLASHROM
        R.FLASHROM = str(fake)
        try:
            ok(R.main(["--backup", str(man_path), "--write", "--programmer", "flashrom:ch341a_spi"]) == 0,
               "write: the flashrom backend writes, reads the part back and compares it (exit 0)")
            ok(chip.read_bytes() == raw, "write: the stand-in part holds the image after the flashrom run")

            # a programmer that reports success but leaves a byte wrong must not pass
            fake.write_text("#!/bin/sh\nwhile [ $# -gt 0 ]; do\n"
                            f"  case \"$1\" in -w) cp \"$2\" {chip} ;; "
                            "-r) cp " + str(chip) + " \"$2\"; printf '\\377' | dd of=\"$2\" bs=1 seek=0 conv=notrunc 2>/dev/null ;; esac\n"
                            "  shift\ndone\n")
            ok(R.main(["--backup", str(man_path), "--write", "--programmer", "flashrom:ch341a_spi"]) == 8,
               "write: a readback that differs exits 8 even when flashrom claimed success")
        finally:
            R.FLASHROM = saved_flashrom

        bad = bindir / "flashrom_bad"
        bad.write_text("#!/bin/sh\necho 'No EEPROM/flash device found.' >&2\nexit 3\n")
        bad.chmod(0o755)
        R.FLASHROM = str(bad)
        try:
            ok(R.main(["--backup", str(man_path), "--write", "--programmer", "flashrom:ch341a_spi"]) == 2,
               "write: a programmer that fails exits 2 instead of reporting a restore")
        finally:
            R.FLASHROM = saved_flashrom

        # exit codes through main(): a good archive is 0, a write is 5, an archive problem is 1
        ok(R.main(["--backup", str(man_path)]) == 0, "main: a usable archive exits 0")
        ok(R.main(["--backup", str(man_path), "--write"]) == 5, "main: --write without a programmer exits 5")
        truncated = Path(tmp) / "cut.bin"
        truncated.write_bytes(raw[:0x80000])
        cut_man = json.loads(json.dumps(man))
        cut_man["files"]["pass_a"] = "cut.bin"
        (Path(tmp) / "cut.manifest.json").write_text(json.dumps(cut_man))
        ok(R.main(["--backup", str(Path(tmp) / "cut.manifest.json")]) == 1, "main: a truncated image exits 1")

    print(f"restore: {failed} failed")
    return failed != 0


if __name__ == "__main__":
    sys.exit(main())
