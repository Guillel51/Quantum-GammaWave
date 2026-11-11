#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The M-Flasher import path: an image built like the stock one, and the refusals that matter.

The fixture is a synthetic 1 MiB part laid out the way a real FM-1 is: an ENC'd boot header and file
table with the bootloader at 0xA0, an ENC'd app table at 0x4000 whose app.bin carries `FM-1_015`, and
the FM-1 region map. Nothing here is hardware: what the tests prove is that the importer checks the
bytes rather than the JSON, that it refuses a second read that differs, and that the archive it writes
is one tools/fm1_restore.py accepts.

The real stock dump (jvitkauskas/fm1-backup) is not in the repository -- it holds another unit's
private data -- so its half of the proof runs only when FM1_REAL_DUMP points at a copy:

  FM1_REAL_DUMP=~/fm1-dump.bin python3 tests/mflasher_test.py

  python3 tests/mflasher_test.py
"""
import contextlib
import hashlib
import io
import json
import os
import struct
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_flash_evidence as E  # noqa: E402
import fm1_image as IMG  # noqa: E402
import fm1_mflasher as M  # noqa: E402
import fm1_restore as S  # noqa: E402

failed = 0


def ok(cond, what):
    global failed
    print("%-74s %s" % (what, "ok" if cond else "FAIL"))
    failed += not cond


def put16(b, at, v):
    struct.pack_into("<H", b, at, v)


def put32(b, at, v):
    struct.pack_into("<I", b, at, v)


def entry(name, addr, length, crc, attr):
    e = bytearray(32)
    put16(e, 2, crc)
    put32(e, 4, addr)
    put32(e, 8, length)
    e[12] = attr
    e[16:16 + len(name)] = name.encode()
    put16(e, 0, IMG.crc16(e[2:]))
    return bytes(e)


def boot_volume(identity_ok=True):
    """the boot area: header, table, a BANKCB bootloader at 0xA0, isd_config at 0x38D0"""
    img = bytearray(b"\xff" * IMG.CAP)
    header = bytearray(32)
    put16(header, 0, IMG.crc16(header[2:]))
    img[0:32] = IMG.enc(bytes(header), 0xFFFF)
    # a valid BANKCB container with no AC79BLID: that is exactly how the stock one reads
    payload = bytes([0xA5]) * 64
    boot = bytearray(b"\x00" * 80)
    put16(boot, 0, 1)
    put16(boot, 2, len(payload))
    put32(boot, 8, 16)
    put16(boot, 12, IMG.crc16(payload))
    put16(boot, 14, IMG.crc16(bytes(boot[:14])))
    boot[16:16 + len(payload)] = payload
    img[0xA0:0xA0 + len(boot)] = IMG.enc(bytes(boot[:16]), 0xFFFF) + IMG.enc(bytes(boot[16:]), 0xFFFF)
    config = bytes(i & 0xFF for i in range(0x2BB))
    table = [entry("uboot.boot", 0xA0, len(boot), IMG.crc16(bytes(boot)), 0),
             entry("isd_config.ini", 0x38D0, len(config), IMG.crc16(config), 0),
             entry("app_dir_head", 0x4000, 0xFFFFFFFF, 0xFFFF, 0)]
    for i, e in enumerate(table):
        img[0x20 + 32 * i:0x40 + 32 * i] = IMG.enc(e, 0xFFFF)
    img[0x38D0:0x38D0 + len(config)] = config
    return img


def app_area(img, identity="FM-1_015", corrupt=False, layout="fm1"):
    """the app area: its table, and an app.bin holding the identity, ENC'd block by block"""
    text = b"\x00" * 0x100 + identity.encode() + b"\x00" * 64
    app_at = 0x100
    crc = IMG.crc16(text)
    if corrupt:
        text = text[:0x110] + b"X" + text[0x111:]
    regions = ({"VM": 0x93000, "BTIF": 0xE9000, "USR": 0xEA000} if layout == "fm1"
               else {"VM": 0xC1000, "BTIF": 0xC0000, "USRTRIM": 0xC1000})
    plain = bytearray(b"\x00" * 0x1000)
    table = [entry("app_area_head", 0x2000120, len(text), crc, 0x83),
             entry("app.bin", app_at, len(text), crc, 0x82)]
    table += [entry(n, a, 0x1000, 0, 0x12 if n == "VM" else 0x92) for n, a in regions.items()]
    for i, e in enumerate(table):
        plain[32 * i:32 * i + 32] = e
    plain[app_at:app_at + len(text)] = text
    for a in range(0, len(plain), 32):
        img[IMG.BOOT_AREA + a:IMG.BOOT_AREA + a + 32] = IMG.enc(bytes(plain[a:a + 32]), IMG.CHIP_KEY ^ (a >> 2))
    return bytes(img)


def stock_image(identity="FM-1_015", corrupt=False, layout="fm1"):
    return app_area(boot_volume(), identity, corrupt, layout)


def record(img, identity="FM-1_015", tool="m-flasher 0.9.1", flash="0x856014", key="0x980F", app="OK"):
    return {
        "tool": tool, "model": "FM-1", "bootloader": "stock (M-VAVE)",
        "firmware": "FM-1 %s" % identity[-3:], "app": app, "chip key": key, "flash": flash,
        "unit id": "0123456789abcdef", "loader": "M-Flasher 9.1, CPU at 360 MHz",
        "sha256": hashlib.sha256(img).hexdigest(), "saved": "20261106-120000",
    }


def pair(tmp, name, img, **kw):
    binp = Path(tmp) / ("%s.bin" % name)
    binp.write_bytes(img)
    jp = Path(tmp) / ("%s.json" % name)
    jp.write_text(json.dumps(record(img, **kw)), encoding="utf-8")
    return binp, jp


def run(*argv):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf), contextlib.redirect_stderr(buf):
        code = M.main(list(argv))
    return code, buf.getvalue()


def main():
    tmp = Path(tempfile.mkdtemp(prefix="qg1-mflasher-"))
    img = stock_image()

    # 1. the image decoder reads the same facts the fixture was built with
    info = IMG.analyse(img)
    ok(info["identity"] == "FM-1_015" and info["app_status"] == "ok" and info["fm1_layout"],
       "the decoder gets the identity, the app CRC and the layout out of the image")
    ok(info["app_files"][0]["name"] == "app.bin" and info["bootloader"]["kind"] == "stock (M-VAVE)",
       "app.bin verifies and the stock bootloader is recognised")
    try:
        IMG.analyse(img[:0x80000])
    except IMG.ImageError as e:
        ok(e.code == "size", "a short image is refused as a size, not decoded")
    else:
        ok(False, "a short image was decoded")

    # 2. one read and no chip CRC32: written, marked unverifiable, and restore refuses it
    b1, j1 = pair(tmp, "one", img)
    code, text = run("import", "--bin", str(b1), "--out", str(tmp / "one-out"))
    ok(code == 1 and "unverifiable" in text, "a single read with no chip CRC32 is not complete")
    man = json.loads(next((tmp / "one-out").glob("*.manifest.json")).read_text())
    ok(man["status"] == "unverifiable" and man["passes"] == 1 and man["device"]["identity"] == "FM-1_015",
       "the archive records one pass and the identity the image decodes to")
    try:
        S.validate(man, img, tmp / "one-out" / man["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "incomplete", "restore refuses an unverifiable import")
    else:
        ok(False, "restore accepted an unverifiable import")

    # 3. the chip's own CRC32 is the corroboration M-Flasher prints
    good = "0x%08X" % (zlib.crc32(img) & 0xFFFFFFFF)
    code, text = run("import", "--bin", str(b1), "--crc32", good, "--out", str(tmp / "crc-out"))
    ok(code == 0 and "complete" in text, "the chip's CRC32 over the whole part completes the archive")
    man = json.loads(next((tmp / "crc-out").glob("*.manifest.json")).read_text())
    ok(man["verified_by"].startswith("the chip's own CRC32"), "the record says what verified it")
    facts = S.validate(man, img, tmp / "crc-out" / man["files"]["pass_a"])
    ok(facts["identity"] == "FM-1_015" and facts["jedec"] == "0x856014",
       "restore accepts it and takes the identity from the decoded app")
    ok(man["programmer"]["flash_writes"] == 0 and man["commands_sent"] == []
       and man["programmer"]["ops"] == ["read"], "nothing written, nothing sent over MIDI")

    # 4. a wrong CRC32 is a wrong read, not a warning
    code, text = run("import", "--bin", str(b1), "--crc32", "0x00000000", "--out", str(tmp / "bad-crc"))
    ok(code == 1 and "not what the chip holds" in text, "a chip CRC32 that disagrees is refused")

    # 5. two independent backups that agree, and two that do not
    code, text = run("import", "--bin", str(b1), "--second", str(b1), "--out", str(tmp / "two-out"))
    ok(code == 0 and "two independent backups" in text, "two identical backups complete the archive")
    man2 = json.loads(next((tmp / "two-out").glob("*.manifest.json")).read_text())
    ok(man2["passes"] == 2 and man2["programmer"]["ops"] == ["read", "read"],
       "the record says two reads")
    other = bytearray(img)
    other[0x9000] ^= 0xFF
    b3, _ = pair(tmp, "two", bytes(other))
    code, text = run("import", "--bin", str(b1), "--second", str(b3), "--out", str(tmp / "diff-out"))
    ok(code == 1 and "differ" in text, "two backups that differ are refused")

    # 6. the .json is checked against the bytes, never believed
    b4, j4 = pair(tmp, "swapped", img)
    bogus = json.loads(j4.read_text())
    bogus["sha256"] = "0" * 64
    j4.write_text(json.dumps(bogus), encoding="utf-8")
    code, text = run("import", "--bin", str(b4), "--out", str(tmp / "swap-out"))
    ok(code == 1 and "SHA-256" in text, "a .json whose hash is not the file's is refused")
    b5, j5 = pair(tmp, "notmflasher", img)
    other_tool = json.loads(j5.read_text())
    other_tool["tool"] = "some-other-tool 1.0"
    j5.write_text(json.dumps(other_tool), encoding="utf-8")
    code, text = run("import", "--bin", str(b5), "--out", str(tmp / "tool-out"))
    ok(code == 1 and "not written by M-Flasher" in text, "a record from another tool is refused")

    # 7. the part decides what may be written, and the layout decides what unit it is
    b6, j6 = pair(tmp, "ucun", img, flash="0xB36014")
    code, text = run("import", "--bin", str(b6), "--crc32", good, "--out", str(tmp / "ucun-out"))
    ok(code == 0, "a UCUN part (newer FM-1s) imports")
    man = json.loads(next((tmp / "ucun-out").glob("*.manifest.json")).read_text())
    ok(man["write_authorised"] is True and man["device"]["jedec"] == "0xB36014",
       "a UCUN part is write-authorised, as fm1_gate and the web installer say")
    b7, j7 = pair(tmp, "unknown", img, flash="0x123456")
    code, text = run("import", "--bin", str(b7), "--out", str(tmp / "unk-out"))
    ok(code == 1 and "does not " in text and "document" in text, "an undocumented JEDEC is refused")
    b8, j8 = pair(tmp, "key", img, key="0x1234")
    code, text = run("import", "--bin", str(b8), "--out", str(tmp / "key-out"))
    ok(code == 1 and "chip key" in text, "a unit whose chip key this project does not know is refused")
    b9, _ = pair(tmp, "smk", stock_image(layout="smk"), app="OK")
    code, text = run("import", "--bin", str(b9), "--out", str(tmp / "smk-out"))
    ok(code == 1, "an SMK-37 layout is not accepted as an FM-1 backup")

    # 8. a firmware that does not verify is not a clean backup of a working unit
    b10, j10 = pair(tmp, "badapp", stock_image(corrupt=True))
    code, text = run("import", "--bin", str(b10), "--out", str(tmp / "badapp-out"))
    ok(code == 1 and ("does not verify" in text or "bad-crc" in text),
       "an app.bin whose CRC fails is refused")

    # 9. restore re-derives the identity: a manifest cannot rename a unit
    man = json.loads(next((tmp / "crc-out").glob("*.manifest.json")).read_text())
    renamed = dict(man, device=dict(man["device"], identity="FM-1_999"))
    try:
        S.validate(renamed, img, tmp / "crc-out" / man["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "identity", "a manifest that renames the unit is refused")
    else:
        ok(False, "a renamed identity passed validation")
    written = json.loads(json.dumps(man))
    written["programmer"]["flash_writes"] = 4096
    try:
        S.validate(written, img, tmp / "crc-out" / man["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "readonly", "a record that admits a flash write is refused")
    else:
        ok(False, "a record with a flash write passed validation")
    sent = json.loads(json.dumps(man))
    sent["commands_sent"] = [0x30]
    try:
        S.validate(sent, img, tmp / "crc-out" / man["files"]["pass_a"])
    except S.RestoreError as e:
        ok(e.code == "readonly", "an import that claims a MIDI frame is refused")
    else:
        ok(False, "a record with MIDI frames passed validation")

    # 10. the evidence checker reads the same bundle and agrees
    code, _ = run("import", "--bin", str(b1), "--crc32", good, "--out", str(tmp / "ev-out"))
    rec_path = tmp / "ev-out" / "evidence.json"
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = E.check_bundle(tmp / "ev-out", None, rec_path)
    rec = json.loads(rec_path.read_text())
    ok(rc == 0 and rec["result"]["status"] == "verified-read" and rec["result"]["restorable"] is True,
       "the evidence checker calls it a verified read of a restorable archive")
    ok(rec["result"]["authorises_install"] is False, "and still does not authorise an install")

    # 11. the real stock dump, when a copy is available: the same facts M-Flasher reports
    real = os.environ.get("FM1_REAL_DUMP")
    if real and Path(real).is_file():
        raw = Path(real).read_bytes()
        got = IMG.analyse(raw)
        ok(got["identity"] == "FM-1_015" and got["app_files"][0]["crc16"] == "0xBD93"
           and got["app_files"][0]["len"] == 580892,
           "the real stock dump decodes to FM-1_015, app.bin 580892 B, CRC16 0xBD93")
        ok(got["fm1_layout"] and got["bootloader"]["kind"] == "stock (M-VAVE)",
           "and it is the stock bootloader on the FM-1 layout")
        if hashlib.sha256(raw).hexdigest() == "3d5315f924147ded9f43e1da0d160e434ab0aa03fdc3ad55e888b64b87591d72":
            b11, j11 = pair(tmp, "real", raw)
            code, text = run("import", "--bin", str(b11),
                             "--crc32", "0x%08X" % (zlib.crc32(raw) & 0xFFFFFFFF),
                             "--out", str(tmp / "real-out"))
            ok(code == 0 and "FM-1_015" in text, "the public stock dump imports as a complete archive")
            man = json.loads(next((tmp / "real-out").glob("*.manifest.json")).read_text())
            ok(S.validate(man, raw, tmp / "real-out" / man["files"]["pass_a"])["identity"] == "FM-1_015",
               "and restore accepts it")
    else:
        print("%-74s %s" % ("the real stock dump", "skipped (set FM1_REAL_DUMP)"))

    print("\n%s" % ("mflasher tests passed" if not failed else "%d FAILED" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
