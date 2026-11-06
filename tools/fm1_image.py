#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Read the JieLi structures out of a 1 MiB FM-1 flash image, without the unit.

An archive is only as good as what can be proved from its bytes. This module decodes what the flash
itself says -- the boot area's file table, the bootloader container, the app area's table, the CRC16
of every file in it, the flash layout the firmware declares, and the `FM-1_0NN` identity the firmware
carries inside `app.bin` -- so a backup can be tied to a unit and a firmware without trusting a JSON.

The reason this matters: the stock image ships its identity *encrypted* inside `app.bin`. A search for
`FM-1_` in the raw bytes finds nothing on a real unit (`jvitkauskas/fm1-backup`, SHA-256
3d5315f924147ded9f43e1da0d160e434ab0aa03fdc3ad55e888b64b87591d72: 0 hits), so identity by plain
search is wrong on exactly the case a first backup targets. Decoding the app area is the only way to
get it.

The formats are JieLi's, as M-Flasher documents and implements them (MIT, (c) jvitkauskas,
https://github.com/jvitkauskas/m-flasher, `src/formats.rs`): CRC16/XMODEM, the ENC cipher, the 32-byte
file entries, the BANKCB bootloader container. Reimplemented here in Python for the evidence tools;
checked against that public dump, where it reproduces the same facts (bootloader at 0xA0, 14384 B;
`app.bin` 580892 B, CRC16 0xBD93; `FM-1_015`).

  fm1_image.py backup/stock.bin
  fm1_image.py backup/stock.bin --json

Exit: 0 = a structure this project can read, 1 = it cannot, 2 = usage.
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

CAP = 0x100000
SECTOR = 0x1000
BOOT_AREA = 0x4000
BOOT_AT = 0xA0
CHIP_KEY = 0x980F
IDENT = re.compile(rb"FM-1_(\d{3})")
# The model names the firmware can report, as M-Flasher reads them out of the decoded app.
MODELS = (b"FM-1", b"MiniFM", b"SMK-37 Pro", b"SMK-37 Elite", b"SMK-37+VA")
# The FM-1 layouts: stock and Baud Girl keep VM/BTIF/USR; 016 replaces USR with SEQ64 and moves them.
FM1_LAYOUTS = (
    {"VM": 0x93000, "USR": 0xEA000},
    {"VM": 0xBA000, "SEQ64": 0xDA000},
)


class ImageError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code


def crc16(data, crc=0):
    """CRC16/XMODEM, which every table and file in the image carries."""
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def enc(data, key):
    """JieLi's ENC cipher. It is its own inverse: the same call encodes and decodes."""
    out = bytearray(len(data))
    k = key & 0xFFFF
    for i, b in enumerate(data):
        out[i] = b ^ (k & 0xFF)
        k = ((k << 1) ^ (0x1021 if k & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def entry(raw):
    """A 32-byte file table entry, or None when its own CRC says it is not one."""
    if len(raw) < 32:
        return None
    head, data_crc, addr, size = struct.unpack_from("<HHII", raw, 0)
    if crc16(raw[2:]) != head:
        return None
    name = raw[16:32].split(b"\0")[0].decode("latin-1", "replace")
    return {"name": name, "addr": addr, "len": size, "crc": data_crc, "attr": raw[12]}


def boot_table(image):
    """The boot area's entries: uboot.boot, isd_config.ini, app_dir_head, key_mac.

    Empty when the flash header does not check out, which is how a damaged boot area reads.
    """
    if len(image) < BOOT_AREA:
        return []
    header = enc(image[:32], 0xFFFF)
    if crc16(header[2:]) != struct.unpack_from("<H", header, 0)[0]:
        return []
    out = []
    for a in range(0x20, BOOT_AT, 0x20):
        e = entry(enc(image[a:a + 32], 0xFFFF))
        if e is None:
            break
        out.append(e)
    return out


def app_read(image, off, n, key=CHIP_KEY):
    """n plain bytes at `off` of the app area at 0x4000, decrypted block by block.

    Each 32-byte block is ENC'd with the chip key XOR (block offset >> 2), so a read that starts
    inside a block has to decode the whole block first.
    """
    out = bytearray()
    start = max(0, off - off % 32)
    end = off + n
    if start + BOOT_AREA >= len(image) and off + n > len(image) - BOOT_AREA:
        raise ImageError("size", "the read 0x%X+%d runs past the image" % (off, n))
    a = start
    while a < end:
        block = image[BOOT_AREA + a:BOOT_AREA + a + 32]
        if len(block) < 32:
            break
        out += enc(block, key ^ ((a >> 2) & 0xFFFF))
        a += 32
    return bytes(out[off % 32:off % 32 + n])


def app_table(image, key=CHIP_KEY):
    """The app area's file table: app.bin, cfg_tool.bin and the region descriptors."""
    out = []
    for i in range(16):
        e = entry(app_read(image, 32 * i, 32, key))
        if e is None:
            break
        out.append(e)
    return out


def bankcb(boot):
    """The payload of a valid BANKCB bootloader container, or None."""
    if len(boot) < 16:
        return None
    bank, size, offset, pcrc, hcrc = (
        struct.unpack_from("<H", boot, 0)[0], struct.unpack_from("<H", boot, 2)[0],
        struct.unpack_from("<I", boot, 8)[0], struct.unpack_from("<H", boot, 12)[0],
        struct.unpack_from("<H", boot, 14)[0])
    if bank != 1 or crc16(boot[:14]) != hcrc or offset + size > len(boot):
        return None
    payload = boot[offset:offset + size]
    return payload if crc16(payload) == pcrc else None


def stored_bootloader(image):
    """The uboot.boot container the boot table points at, if it is there and its CRC matches."""
    table = boot_table(image)
    if not table:
        return None
    e = table[0]
    if e["name"] != "uboot.boot" or e["addr"] != BOOT_AT:
        return None
    raw = image[BOOT_AT:BOOT_AT + e["len"]]
    if len(raw) != e["len"]:
        return None
    boot = enc(raw[:16], 0xFFFF) + enc(raw[16:], 0xFFFF)
    return boot if crc16(boot) == e["crc"] else None


def bootloader(image):
    """What the stored bootloader is: m-bootloader/SMK-37 tag or the stock M-VAVE one."""
    boot = stored_bootloader(image)
    if boot is None:
        return {"kind": "damaged or missing", "tag": "", "version": ""}
    payload = bankcb(boot)
    if payload and len(payload) >= 0x70 and payload[0x40:0x48] == b"AC79BLID":
        tag = payload[0x48:0x50].split(b"\0")[0].decode("latin-1", "replace")
        version = payload[0x50:0x70].split(b"\0")[0].decode("latin-1", "replace")
        name = "m-bootloader" if version[:1].isdigit() and "." in version else "earlier open bootloader"
        return {"kind": name, "tag": tag, "version": version}
    return {"kind": "stock (M-VAVE)", "tag": "", "version": ""}


def files_status(image, table, key=CHIP_KEY):
    """-> (status, [files]) where status is ok / bad-crc / incomplete / none.

    A damaged file that has a fully erased 4 KiB sector inside was interrupted mid-write, which is a
    different fact from a corrupted one, and both are different from "no firmware installed".
    """
    files = [e for e in table if e["attr"] == 0x82]
    if not files:
        return "none", []
    for e in files:
        data = app_read(image, e["addr"], e["len"], key)
        if len(data) != e["len"] or crc16(data) != e["crc"]:
            start = BOOT_AREA + e["addr"]
            s = start - start % SECTOR
            while s < start + e["len"]:
                if image[s:s + SECTOR].count(0xFF) == SECTOR:
                    return "incomplete", files
                s += SECTOR
            return "bad-crc", files
    return "ok", files


def _identities(text):
    """[('FM-1','015'), ...] for every MODEL_NNN the decoded app carries."""
    out = []
    for model in MODELS:
        pat = re.compile(re.escape(model) + rb"_(\d{3})")
        for m in pat.finditer(text):
            out.append((model.decode(), m.group(1).decode(), m.start()))
    return out


def analyse(image, key=CHIP_KEY):
    """Everything the image says about the unit, or ImageError with what could not be read."""
    if len(image) != CAP:
        raise ImageError("size", "the image is %d bytes; a full FM-1 part is %d (1 MiB)" % (len(image), CAP))
    table = boot_table(image)
    if not table:
        raise ImageError("header", "the flash header does not check out: not a readable boot area")
    names = [e["name"] for e in table]
    if "uboot.boot" not in names or "isd_config.ini" not in names:
        raise ImageError("table", "the boot table has no uboot.boot/isd_config.ini: %s" % (names or "empty"))
    boot = bootloader(image)
    app = app_table(image, key)
    if not app:
        raise ImageError("app-table", "the app area has no readable file table")
    status, files = files_status(image, app, key)
    regions = {e["name"]: e["addr"] for e in app if e["attr"] in (0x12, 0x92)}
    layout = next((n for n, want in enumerate(FM1_LAYOUTS, 1)
                   if all(regions.get(k) == v for k, v in want.items())), 0)
    identity, firmware, idents = "", "", []
    app_file = next((e for e in app if e["name"] == "app.bin"), None)
    if app_file and status != "none":
        text = app_read(image, app_file["addr"], app_file["len"], key)
        idents = _identities(text)
        if idents:
            model, version, _ = idents[0]
            identity, firmware = "%s_%s" % (model, version), "%s %s" % (model, version)
    return {
        "image_size": len(image),
        "flash_header": "ok",
        "boot_table": table,
        "bootloader": boot,
        "app_table": app,
        "app_status": status,
        "app_files": [{"name": e["name"], "len": e["len"], "crc16": "0x%04X" % e["crc"]} for e in files],
        "regions": regions,
        "fm1_layout": bool(layout),
        "layout_evidence": ("the firmware declares the FM-1 layout #%d" % layout) if layout else
                           "the firmware declares no known FM-1 layout",
        "identity": identity,
        "identities": [{"identity": "%s_%s" % (m, v), "offset": o} for m, v, o in idents],
        "firmware": firmware,
        "chip_key": "0x%04X" % key,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="decode the JieLi structures of an FM-1 flash image")
    ap.add_argument("image", help="a 1 MiB dump")
    ap.add_argument("--chip-key", default="0x980F", help="the unit's chip key (default 0x980F)")
    ap.add_argument("--json", action="store_true", help="print the whole analysis as JSON")
    a = ap.parse_args(argv)
    try:
        key = int(str(a.chip_key), 16)
    except ValueError:
        print("--chip-key must be a hex number such as 0x980F", file=sys.stderr)
        return 2
    raw = Path(a.image).read_bytes()
    try:
        info = analyse(raw, key)
    except ImageError as e:
        print("unreadable (%s): %s" % (e.code, e), file=sys.stderr)
        return 1
    if a.json:
        print(json.dumps(info, indent=2, sort_keys=True))
        return 0
    b = info["bootloader"]
    print("%s: %s" % (a.image, info["identity"] or "no FM-1_0NN in the decoded app"))
    print("  bootloader  %s %s" % (b["kind"], ("(" + b["tag"] + " " + b["version"] + ")").strip() if b["tag"] else ""))
    print("  app         %s%s" % (info["app_status"],
                                  "" if info["app_status"] != "ok" else
                                  ", " + ", ".join("%s %d B CRC %s" % (f["name"], f["len"], f["crc16"])
                                                   for f in info["app_files"])))
    print("  layout      %s" % info["layout_evidence"])
    print("  regions     %s" % (", ".join("%s@0x%05X" % (k, v) for k, v in sorted(info["regions"].items())) or "none"))
    print("  firmware    %s" % (info["firmware"] or "unknown"))
    if info["app_status"] != "ok":
        print("  note        the app does not verify: this image is not a clean stock backup", file=sys.stderr)
        return 1
    if not info["identity"]:
        print("  note        no identity in the decoded app: this archive cannot be tied to a unit",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
