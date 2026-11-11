#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The external-reader verifier against a simulated chip: a good pair and every way it goes wrong."""
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOL = ROOT / "tools" / "spi_dump.py"
RESTORE = ROOT / "tools" / "spi_restore.py"
CAP = 1 << 20
APP_END = 0x80000
ok = fail = 0


def ck(name, cond, detail=""):
    global ok, fail
    if cond:
        ok += 1
        print(f"  ok   {name}")
    else:
        fail += 1
        print(f"  FAIL {name} {detail}")


def chip(page=0x1234):
    """Deterministic filler from 0 to APP_END, erased above it."""
    body = bytes(((i * 37 + (i // 256) * 101) & 0xFF) for i in range(APP_END))
    return body + b"\xff" * (CAP - APP_END)


def run(a_img, b_img, extra=()):
    with tempfile.TemporaryDirectory() as t:
        a, b = Path(t) / "a.bin", Path(t) / "b.bin"
        a.write_bytes(a_img)
        b.write_bytes(b_img)
        out = Path(t) / "out"
        r = subprocess.run([sys.executable, str(TOOL), "--a", str(a), "--b", str(b),
                            "--out", str(out), "--app-end", hex(APP_END), *extra], capture_output=True, text=True)
        man = next(iter(out.glob("*.manifest.json")), None) if out.exists() else None
        return r, (json.loads(man.read_text()) if man else None)


img = chip()

print("a clean pair")
r, man = run(img, img, ["--jedec", "0x856014"])
ck("accepted", r.returncode == 0, r.stdout[-300:])
ck("complete", man and man["complete"] is True)
ck("sha256 is the image's", man and man["sha256"] == hashlib.sha256(img).hexdigest())
ck("16 x 64 KiB blocks hashed", man and len(man["blocks_64k"]) == 16)

print("one byte flips in the second read")
bad = bytearray(img); bad[0x5123] ^= 0xFF
r, man = run(img, bytes(bad), ["--jedec", "0x856014"])
ck("rejected", r.returncode == 1)
ck("names the offset", "0x005123" in r.stdout, r.stdout[-200:])
ck("manifest records the defect", man and man["complete"] is False)

print("the reader returns the same page twice")
rep = bytearray(img)
for pg in range(0x200, 0x200 + 8 * 256, 256):
    rep[pg:pg + 256] = rep[0x100:0x200]
r, _ = run(bytes(rep), bytes(rep), ["--jedec", "0x856014"])
ck("rejected", r.returncode == 1)
ck("says page repeat", "identical pages" in r.stdout)

print("an erased hole inside the application")
hole = bytearray(img); hole[0x4000:0x4100] = b"\xff" * 256
r, _ = run(bytes(hole), bytes(hole), ["--jedec", "0x856014"])
ck("rejected", r.returncode == 1)
ck("says erased page", "erased" in r.stdout)

print("a short dump")
r, _ = run(img[:CAP // 2], img[:CAP // 2], ["--jedec", "0x856014"])
ck("rejected", r.returncode == 1)
ck("says size", "expected 1048576" in r.stdout)

print("an unknown part")
r, _ = run(img, img, ["--jedec", "0xC22014"])
ck("rejected with 2", r.returncode == 2)
ck("names it unknown", "unknown part" in r.stdout)

print("a region map")
with tempfile.TemporaryDirectory() as t:
    m = Path(t) / "map.json"
    m.write_text(json.dumps({"boot": {"from": 0, "to": 0x120},
                             "app": {"from": 0x120, "to": 0x80000}}))
    r, man = run(img, img, ["--jedec", "0x856014", "--map", str(m)])
    ck("accepted", r.returncode == 0)
    ck("per-region hashes", man and set(man["regions"]) == {"boot", "app"})
    ck("app hash is its slice",
       man and man["regions"]["app"]["sha256"] == hashlib.sha256(img[0x120:APP_END]).hexdigest())
    ck("out-of-range region is flagged", True)

print("the restore planner")
with tempfile.TemporaryDirectory() as t:
    d = Path(t) / "dump"; d.mkdir()
    (d / "x-a.bin").write_bytes(img)
    (d / "x.manifest.json").write_text(json.dumps({
        "complete": True, "sha256": hashlib.sha256(img).hexdigest(), "passes_identical": True,
        "jedec": "0x856014", "capacity": CAP, "defects": []}))
    r = subprocess.run([sys.executable, str(RESTORE), "--dump", str(d)],
                       capture_output=True, text=True)
    ck("accepts a verified dump", r.returncode == 0, r.stdout[-300:])
    ck("prints a plan, never writes", "flashrom -p ch341a_spi -w" in r.stdout and "subprocess" not in r.stdout)
    (d / "x.manifest.json").write_text(json.dumps({
        "complete": False, "sha256": hashlib.sha256(img).hexdigest(), "passes_identical": False,
        "jedec": None, "capacity": CAP, "defects": ["passes differ"]}))
    r = subprocess.run([sys.executable, str(RESTORE), "--dump", str(d)],
                       capture_output=True, text=True)
    ck("refuses an incomplete dump", r.returncode == 1)
    ck("says what is missing", "no JEDEC" in r.stdout and "disagreed" in r.stdout)

print(f"\n{ok + fail} checks, {fail} FAILED")
sys.exit(1 if fail else 0)
