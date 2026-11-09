#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The M-Flasher acceptance trace, and the refusals that prove it is not a rubber stamp.

The fixture is the signed release in `release/`. A checker that only ever prints "ok" proves nothing,
so every refusal M-Flasher's `plan_firmware`/`Package::parse` makes is reproduced here by mutating that
package: a damaged app file, a damaged header, a damaged flash image, a foreign model and a foreign
chip key. Each one must be refused, and with M-Flasher's own wording.

  python3 tests/mflasher_accept_test.py
"""
import hashlib
import io
import contextlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_mflasher_accept as A  # noqa: E402

PKG = ROOT / "release" / "felucca-v1.9.0.fwsc"
failed = 0


def ok(cond, what):
    global failed
    print("%-76s %s" % (what, "ok" if cond else "FAIL"))
    failed += not cond


def raw_at(n, image_offset):
    """The file offset of an image offset: each identity block costs one byte."""
    if image_offset < n * A.KEEP:
        return (image_offset // A.KEEP) * A.BLOCK + image_offset % A.KEEP
    return n * A.BLOCK + (image_offset - n * A.KEEP)


def rebuild(raw, n, mutate):
    """The package again, with `mutate` applied to the image and every CRC recomputed.

    This is the case the container CRCs cannot catch: a package that is perfectly consistent with
    itself (header, file list, flash.bin) whose firmware is not the one the unit's chip key decodes.
    """
    image = bytearray()
    for i in range(n):
        image += raw[i * A.BLOCK:i * A.BLOCK + A.KEEP]
    image += raw[n * A.BLOCK:]
    head = bytearray(A.enc(bytes(image[:A.HEAD]), 0xFFFF))
    count = A.u16(head, 8)
    list_at = A.HEAD
    entries = []
    for i in range(count):
        e = bytearray(A.enc(bytes(image[list_at + i * A.FILE_ENT:list_at + (i + 1) * A.FILE_ENT]), 0xFFFF))
        entries.append(e)
    flash_e = next(e for e in entries if A.cstr(e[0x40:]) == "flash.bin")
    off, size = A.u32(flash_e, 8), A.u32(flash_e, 12)
    mutate(image, off, size)
    flash_e[4:6] = A.IMG.crc16(bytes(image[off:off + size])).to_bytes(2, "little")
    for i, e in enumerate(entries):
        image[list_at + i * A.FILE_ENT:list_at + (i + 1) * A.FILE_ENT] = A.enc(bytes(e), 0xFFFF)
    lst = bytes(image[list_at:list_at + A.FILE_ENT * count])
    head[2:4] = A.IMG.crc16(lst).to_bytes(2, "little")
    head[0:2] = A.IMG.crc16(bytes(head[2:])) .to_bytes(2, "little")
    image[0:A.HEAD] = A.enc(bytes(head), 0xFFFF)
    out = bytearray()
    for i in range(n):
        out += image[i * A.KEEP:(i + 1) * A.KEEP]
        out.append(raw[i * A.BLOCK + A.KEEP])          # the identity marker, as it was
    out += image[n * A.KEEP:]
    return bytes(out)


def run(raw, key=0x980F):
    """(exit code, output) of the trace over these bytes."""
    out = io.StringIO()
    tmp = ROOT / "build" / "_accept_test.fwsc"
    tmp.parent.mkdir(exist_ok=True)
    tmp.write_bytes(raw)
    with contextlib.redirect_stdout(out):
        try:
            code = A.main([str(tmp), "--key", "0x%04X" % key])
        finally:
            tmp.unlink()
            A.failed.clear()
    return code, out.getvalue()


raw = PKG.read_bytes()
with contextlib.redirect_stdout(io.StringIO()):
    pkg = A.parse_package(raw)
n = pkg["blocks"]

code, out = run(raw)
ok(code == 0 and "Every pre-write check M-Flasher makes passes" in out,
   "the release passes every check M-Flasher makes before its first erase")
ok(pkg["identity"] == "FM-1_982", "the container's identity is %r" % pkg["identity"])
ok(pkg["model"] == "FM-1", "its model, as M-Flasher reads it, is %r" % pkg["model"])
ok(len(pkg["flash"]) == 0x93000, "the flash image is 0x93000 B (%d sectors)" % (len(pkg["flash"]) // A.SECTOR))
ok(hashlib.sha256(raw).hexdigest() ==
   "80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2",
   "the bytes traced are the release's (sha256 80ad995caf9147f2...)")

# 1. an app file whose bytes were damaged, with every container CRC recomputed: the chip-key refusal
app = next(e for e in A.IMG.app_table(pkg["flash"] + b"\xFF" * (A.MAX_IMAGE - len(pkg["flash"])),
                                      A.CHIP_KEY) if e["name"] == "app.bin")


def damage_app(image, off, size):
    image[A.BOOT_AREA + app["addr"] + 0x1000] ^= 0xFF


code, out = run(rebuild(raw, n, damage_app))
ok(code == 1 and "decrypt with this unit's chip key" in out,
   "a consistent package with a damaged app.bin is refused: the chip key/CRC check")

# 2. the header's CRC16: one byte of it
bad_head = bytearray(raw)
bad_head[raw_at(n, 2)] ^= 0x01
code, out = run(bytes(bad_head))
ok(code == 1 and "header CRC" in out, "a damaged package header is refused (header CRC)")

# 3. flash.bin's own CRC16: a byte of the image outside the app table
bad_flash = bytearray(raw)
bad_flash[raw_at(n, len(pkg["flash"]) - A.SECTOR)] ^= 0x01
code, out = run(bytes(bad_flash))
ok(code == 1 and "flash.bin CRC" in out, "a damaged flash image is refused (flash.bin CRC)")

# 4. a foreign model: the identity weave, one character
foreign = bytearray(raw)
foreign[0 * A.BLOCK + A.KEEP] = (ord("S") + 1) & 0xFF  # 'F' -> 'S': SM-1_982
code, out = run(bytes(foreign))
ok(code == 1 and "this package is for the SM-1" in out,
   "a package for another model is refused, naming it")

# 5. a foreign chip key: the same package under a key it was not made for
code, out = run(raw, key=0xFFFF)
ok(code == 1 and "does not decrypt with this unit's chip key (0xFFFF)" in out,
   "the package is refused under a chip key it was not built for (0xFFFF)")

# 6. a smaller flash image: cut to a non-sector size
small = bytearray(raw)
cut = len(pkg["flash"]) - 0x800          # no longer a whole number of 4 KiB sectors
del small[raw_at(n, cut):]
code, out = run(bytes(small))
ok(code == 1 and ("unexpected flash image size" in out or "file list" in out
                  or "damaged firmware package" in out),
   "a package whose image is not a whole number of sectors is refused")

print()
print("m-flasher acceptance: %d check(s), %d FAILED" % (8 + 6, failed))
sys.exit(1 if failed else 0)
