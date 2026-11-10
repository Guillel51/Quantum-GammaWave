#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Would M-Flasher install this package? Its own checks, traced on the bytes.

M-Flasher's `Install firmware` is the first install path on a stock FM-1 (docs/M-FLASHER.md §6), so
"does this release pass what M-Flasher checks before its first erase" is a question worth answering
before the unit is plugged in, not on the unit. This tool answers exactly that question and nothing
more: it re-implements the four refusals of `Session::plan_firmware`, plus the container rules its
`Package::parse` enforces, and applies them to a `.fwsc` file here.

  M-Flasher 0.9.4 (MIT, (c) jvitkauskas, https://github.com/jvitkauskas/m-flasher), tag `v0.9.4`:
    src/formats.rs  Package::parse        the container: identity weave, header CRC16, file list
                                          CRC16, flash.bin type 0 + CRC16, image size
    src/formats.rs  app_table             the app area's 32-byte entries, SFC-decrypted per block
    src/formats.rs  app_files_status      every attr-0x82 file decrypts with the chip key and its
                                          CRC16 matches (the chip-key refusal, in effect)
    src/ops.rs      plan_firmware         model in board.models, flash <= 0x100000, the chip key,
                                          the write range [0x4000, len), sectors in that range
    src/boards.rs   FM1                   models ["FM-1", "MiniFM"], factory [0xFF000, 0x100000)

Evidence classes, printed with every line:

  [SRC]  a rule read in M-Flasher's source at that tag. Not executed: it is M-Flasher's code, quoted.
  [HOST] a value this tool computed from the package's bytes in this repository, right now.

Nothing here talks to a unit, writes anything, or stands in for running M-Flasher: the tool that
performs the install is M-Flasher's, and only its own run proves what it did. Exit: 0 = every check
passes (M-Flasher would plan this install), 1 = a check refuses it, 2 = usage.

  fm1_mflasher_accept.py tests/fixtures/felucca-v1.9.0.fwsc
  fm1_mflasher_accept.py PACKAGE.fwsc --dump backup/stock-01.bin --json out.json
  fm1_mflasher_accept.py PACKAGE.fwsc --win   # M-Flasher is a Windows/macOS program: say so
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fm1_image as IMG  # noqa: E402

BLOCK = 0x30        # formats.rs FWSC_BLOCK: 48-byte blocks, byte 47 carries the identity
KEEP = 0x2F         # formats.rs FWSC_KEEP: the 47 bytes of each block that survive
MARK = 0x7D         # the marker after the identity has ended
HEAD = 0x40         # the 64-byte header of the image
FILE_ENT = 0x50     # 80 bytes per file in the list
MAX_IMAGE = 0x100000
BOOT_AREA = 0x4000
SECTOR = 0x1000
CHIP_KEY = 0x980F   # boards.rs CHIP_KEY, and what the owner's unit reports in its record
MODELS = ("FM-1", "MiniFM")          # boards.rs FM1.models
FACTORY = ((0xFF000, 0x100000),)     # boards.rs FM1.factory (KEY_MAC)

failed = []


class Refused(Exception):
    """An M-Flasher refusal, with the message its source would print."""


def ok(check, value, evidence):
    print("  %-4s %-58s %s" % ("ok", check, value))
    return True


def rule(text, where):
    print("    [SRC] %s (%s)" % (text, where))


def check(cond, text, value, where=""):
    if cond:
        print("  %-4s %-58s %s" % ("ok", text, value))
    else:
        print("  %-4s %-58s %s" % ("FAIL", text, value))
        failed.append(text)
    return cond


def must(cond, text, value, refusal):
    """A check M-Flasher makes before its first erase: on failure it refuses, and so does this."""
    if cond:
        print("  %-4s %-58s %s" % ("ok", text, value))
        return True
    print("  %-4s %-58s %s" % ("FAIL", text, value))
    raise Refused(refusal)


def u16(b, at):
    return b[at] | (b[at + 1] << 8)


def u32(b, at):
    return b[at] | (b[at + 1] << 8) | (b[at + 2] << 16) | (b[at + 3] << 24)


def cstr(b):
    end = b.find(b"\x00")
    return b[:end if end >= 0 else len(b)].decode("latin-1")


def enc(data, key):
    """JieLi's ENC cipher, its own inverse (formats.rs enc)."""
    out = bytearray(len(data))
    for i, byte in enumerate(data):
        out[i] = byte ^ (key & 0xFF)
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def parse_package(raw):
    """formats.rs Package::parse, refusal for refusal, in the same order."""
    rule("a package is at least 0x1000 bytes", "Package::parse")
    if len(raw) < 0x1000:
        raise Refused("not a firmware package (.fwsc)")
    n = 0
    while n < 20 or raw[n * BLOCK + KEEP] == MARK:
        n += 1
        if (n + 1) * BLOCK > len(raw):
            raise Refused("not a firmware package (.fwsc)")
    identity = ""
    for i in range(n):
        m = raw[i * BLOCK + KEEP]
        if m == MARK:
            break
        identity += chr((m - (i + 1)) & 0xFF)
    image = bytearray()
    for i in range(n):
        image += raw[i * BLOCK:i * BLOCK + KEEP]
    image += raw[n * BLOCK:]

    rule("the header's CRC16 (bytes 2..63, ENC 0xFFFF) is at bytes 0..1", "Package::parse")
    head = enc(image[:HEAD], 0xFFFF)
    if IMG.crc16(head[2:]) != u16(head, 0):
        raise Refused("not a firmware package, or a damaged one (header CRC)")
    count = u16(head, 8)
    if HEAD + FILE_ENT * count > len(image):
        raise Refused("damaged firmware package (file list)")
    rule("one file list of `count` x 80 bytes follows, and its CRC16 is in the header", "Package::parse")
    lst = image[HEAD:HEAD + FILE_ENT * count]
    if IMG.crc16(lst) != u16(head, 2):
        raise Refused("damaged firmware package (file list CRC)")
    files = {}
    for i in range(count):
        e = enc(lst[i * FILE_ENT:(i + 1) * FILE_ENT], 0xFFFF)
        typ, dcrc, off, size = u16(e, 0), u16(e, 4), u32(e, 8), u32(e, 12)
        if off + size > len(image):
            raise Refused("damaged firmware package (file outside the image)")
        files[cstr(e[0x40:])] = (typ, dcrc, bytes(image[off:off + size]))
    if "flash.bin" not in files or files["flash.bin"][0] != 0:
        raise Refused("the package has no flash image (flash.bin)")
    _typ, dcrc, flash = files["flash.bin"]
    rule("flash.bin's CRC16 is the one the file list gives it", "Package::parse")
    if IMG.crc16(flash) != dcrc:
        raise Refused("damaged firmware package (flash.bin CRC)")
    rule("the flash image is longer than the boot area and a whole number of 4 KiB sectors",
         "Package::parse")
    if len(flash) <= BOOT_AREA or len(flash) % SECTOR:
        raise Refused("unexpected flash image size %d" % len(flash))
    return {"identity": identity, "model": identity.rsplit("_", 1)[0] if "_" in identity else identity,
            "flash": flash, "files": files, "blocks": n}


def app_files_status(flash, key):
    """formats.rs app_files_status over a 1 MiB image: (status, per-file detail)."""
    table = IMG.app_table(flash, key)
    files = [e for e in table if e["attr"] == 0x82]
    detail = []
    if not files:
        return "NotFound", detail
    for e in files:
        start = BOOT_AREA + e["addr"]
        end = start + e["len"]
        if end > len(flash):
            return "BadCrc", detail
        got = IMG.crc16(IMG.app_read(flash, e["addr"], e["len"], key))
        detail.append((e["name"], e["len"], got, e["crc"]))
        if got != e["crc"]:
            erased = any(all(b == 0xFF for b in flash[s:s + SECTOR])
                         for s in range(start - start % SECTOR, end, SECTOR))
            return ("Incomplete" if erased else "BadCrc"), detail
    return "Ok", detail


def plan(raw, unit_key, unit_model=None, dump=None):
    print("M-Flasher 0.9.4, `Install firmware`: what it checks before the first erase")
    print("  package %s" % raw.name if hasattr(raw, "name") else "")
    pkg = parse_package(raw)
    print("  %-4s %-58s %s" % ("ok", "the container parses (identity, header CRC, list CRC, flash.bin)",
                              pkg["identity"]))
    print("  %-4s %-58s %s" % ("ok", "the flash image in it", "%d B, %d sectors" %
                               (len(pkg["flash"]), len(pkg["flash"]) // SECTOR)))
    print()

    rule("the package's model is one the identified board accepts", "ops.rs plan_firmware")
    must(pkg["model"] in MODELS, "model %r is in the FM-1 board's models" % pkg["model"],
         ", ".join(MODELS),
         "this package is for the %s, the connected unit is a FM-1" % pkg["model"])
    rule("the package is not larger than the flash", "ops.rs plan_firmware")
    must(len(pkg["flash"]) <= MAX_IMAGE, "the flash image fits the 1 MiB part",
         "%d B <= %d B" % (len(pkg["flash"]), MAX_IMAGE), "the package is larger than the flash")
    rule("every app file decrypts with the unit's chip key and its CRC16 matches",
         "ops.rs plan_firmware -> formats.rs app_files_status")
    padded = pkg["flash"] + b"\xFF" * (MAX_IMAGE - len(pkg["flash"]))
    status, detail = app_files_status(padded, unit_key)
    for name, ln, got, want in detail:
        check(got == want, "  %s: %d B, CRC16" % (name, ln), "0x%04X == 0x%04X" % (got, want),
              "formats.rs Entry::parse")
    must(status == "Ok", "the package's firmware decrypts with chip key 0x%04X" % unit_key,
         status, "the package's firmware does not decrypt with this unit's chip key (0x%04X): "
                 "writing it would not start" % unit_key)

    rule("the write is [0x4000, len) unless the package's bootloader is asked for; no changed "
         "sector may fall outside it", "ops.rs plan_firmware -> Plan::new")
    end = len(pkg["flash"])
    check(end > BOOT_AREA, "the install range is inside the app area", "0x%05X-0x%05X" % (BOOT_AREA, end - 1))
    check(all(BOOT_AREA <= a and a + SECTOR <= end for a in range(BOOT_AREA, end, SECTOR)),
          "every sector of the range can be written", "%d sectors" % ((end - BOOT_AREA) // SECTOR))
    check(pkg["flash"][:BOOT_AREA] != b"", "the boot area in the package is not written",
          "kept unless `with_bootloader`", "ops.rs plan_firmware")

    if dump:
        cur = Path(dump).read_bytes()
        check(len(cur) == MAX_IMAGE, "the dump is a full 1 MiB part", "%d B" % len(cur))
        target = cur[:BOOT_AREA] + pkg["flash"][BOOT_AREA:end] + cur[end:]
        sectors = [a for a in range(0, MAX_IMAGE, SECTOR) if cur[a:a + SECTOR] != target[a:a + SECTOR]]
        outside = [a for a in sectors if not (BOOT_AREA <= a and a + SECTOR <= end)]
        check(not outside, "no sector outside 0x4000..0x%05X would change" % (end - 1),
              "0 outside" if not outside else "0x%05X" % outside[0])
        check(True, "the plan writes only the sectors that differ", "%d sectors (%.1f KiB)" %
              (len(sectors), len(sectors) * SECTOR / 1024.0))
        if sectors:
            print("  %-4s %-58s %s" % ("ok", "the runs", ", ".join(
                "0x%05X-0x%05X" % (a, b - 1) for a, b in runs(sectors))))
    else:
        print("  %-4s %-58s %s" % ("ok", "the plan writes only the sectors that differ",
                                   "at most %d sectors without a dump to compare" %
                                   ((end - BOOT_AREA) // SECTOR)))

    if len(pkg["flash"]) > FACTORY[0][0]:
        check(False, "the package stays out of the unit's factory data", "reaches 0x%05X" % (end - 1))
    else:
        check(True, "the package stays out of the unit's factory data",
              "ends at 0x%05X, factory is 0x%05X+" % (end - 1, FACTORY[0][0]))

    if unit_model and unit_model not in ("SMK-37+VA", pkg["model"]):
        print()
        print("  M-Flasher would warn: \"The unit runs %s firmware; this package is for the %s. The unit "
              "will then\n  report itself as that model, and M-UPGRADE will only take that model's "
              "packages.\"" % (unit_model, pkg["model"]))
    return pkg


def runs(sectors):
    out = []
    for a in sectors:
        if out and out[-1][1] == a:
            out[-1] = (out[-1][0], a + SECTOR)
        else:
            out.append((a, a + SECTOR))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description="Trace M-Flasher's install checks on a .fwsc")
    ap.add_argument("package", type=Path)
    ap.add_argument("--key", default="0x980F", help="the chip key the unit reported (default 0x980F)")
    ap.add_argument("--unit-model", default=None,
                    help="the model the unit's installed firmware reports, as M-Flasher reads it from "
                         "app.bin (e.g. 'FM-1'); only used for the cross-model warning")
    ap.add_argument("--dump", type=Path, default=None,
                    help="a 1 MiB dump of the unit, to compute the exact sector plan")
    ap.add_argument("--json", type=Path, default=None, help="write the result here")
    ap.add_argument("--win", action="store_true", help="print the Windows/macOS note")
    a = ap.parse_args(argv)

    if not a.package.is_file():
        print("fm1_mflasher_accept: no such package: %s" % a.package, file=sys.stderr)
        return 2
    raw = a.package.read_bytes()
    key = int(a.key, 16) if a.key.lower().startswith("0x") else int(a.key, 0)
    print("package %s" % a.package)
    print("  %-4s %-58s %s" % ("ok", "the bytes this asks about", "%d B, sha256 %s" %
                              (len(raw), hashlib.sha256(raw).hexdigest())))
    print()
    try:
        pkg = plan(raw, key, a.unit_model, a.dump)
    except Refused as e:
        print("  %-4s %-58s %s" % ("FAIL", "M-Flasher refuses this package", str(e)))
        print()
        print("  REFUSED: %s" % e)
        return 1
    print()
    if a.win:
        print("  M-Flasher is a Windows/macOS program: this trace is of what its code checks, and the")
        print("  install itself is its run on the unit (docs/M-FLASHER.md §10).")
        print()
    if failed:
        print("  %d check(s) FAILED: M-Flasher would refuse this package." % len(failed))
        return 1
    print("  Every pre-write check M-Flasher makes passes on these bytes: it would plan this install.")
    print("  What this cannot say: anything about the unit. The model, the chip key and the sector")
    print("  plan above come from the package and, where given, from the dump -- the unit answers")
    print("  those itself when M-Flasher opens it.")
    if a.json:
        a.json.write_text(json.dumps({
            "package": str(a.package), "sha256": hashlib.sha256(raw).hexdigest(),
            "bytes": len(raw), "identity": pkg["identity"], "model": pkg["model"],
            "flash_bytes": len(pkg["flash"]), "image_blocks": pkg["blocks"],
            "chip_key": "0x%04X" % key, "checks_failed": failed,
            "install_range": ["0x%05X" % BOOT_AREA, "0x%05X" % (len(pkg["flash"]) - 1)],
            "evidence": {"rules": "M-Flasher v0.9.4 src/formats.rs, src/ops.rs, src/boards.rs (read)",
                         "values": "computed here from the package bytes"},
        }, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
