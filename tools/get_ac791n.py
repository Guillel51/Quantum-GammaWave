#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The AC791N toolchain and the three SDK files, in one reproducible step.

    tools/get_ac791n.py                 # ~/.jieli/toolchain and ~/fw-AC79_AIoT_SDK
    tools/get_ac791n.py --check         # what is installed, what is missing (writes nothing)
    tools/get_ac791n.py --sdk /path --toolchain /path

The toolchain is JieLi's Linux toolchain for pi32v2 (clang 4.0.1), fetched by tools/get_toolchain.sh
with its SHA-256 pinned in tools/toolchain.sha256. The SDK is not needed: tools/build.py links against
three blobs of the SPL only, and it pins their SHA-256 (`SDK_SHA256`, version AC79NN_SDK_V1.2.1). They
are fetched from the tag's raw files, checked before they are written, and the tree is ~15 KiB instead
of a 1.1 GB clone. Nothing here needs a vendor account.

After this, `./build.sh` produces build/felucca.bin and the package, plus build/felucca.syms,
build/felucca.map and the per-engine table from tools/engine_size.py (XIP and .data+.bss of every
engine, measured on that build).
"""
import argparse
import hashlib
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build                                    # noqa: E402  (SDK_SHA256: the same pins the build checks)

SDK_REF = "AC79NN_SDK_V1.2.1_2023-12-13"
SDK_RAW = "https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/" + SDK_REF + "/cpu/wl82/tools/"
TOOLCHAIN_SHA = ROOT / "tools" / "toolchain.sha256"


def pinned_toolchain():
    for line in TOOLCHAIN_SHA.read_text().splitlines():
        line = line.strip()
        if len(line) == 64 and all(c in "0123456789abcdef" for c in line):
            last = line
    return last                                     # the file's last bare hash is the reviewed one


def toolchain_ok(dest):
    return (dest / "toolchain" / "pi32v2" / "bin" / "clang").exists()


def get_toolchain(dest, force):
    if toolchain_ok(dest) and not force:
        print("toolchain: %s already installed (its clang is there)" % (dest / "toolchain"))
        return True
    if not TOOLCHAIN_SHA.exists():
        print("toolchain: %s is missing, so there is nothing to check the archive against" % TOOLCHAIN_SHA,
              file=sys.stderr)
        return False
    print("toolchain: tools/get_toolchain.sh (sha256 %s)" % pinned_toolchain()[:16] + "...")
    r = subprocess.run(["sh", str(ROOT / "tools" / "get_toolchain.sh"), str(dest)])
    return r.returncode == 0 and toolchain_ok(dest)


def sdk_path(dest, rel):
    return dest / "cpu" / "wl82" / "tools" / rel


def sdk_ok(dest):
    for rel, sha in build.SDK_SHA256.items():
        p = sdk_path(dest, rel)
        if not p.exists() or hashlib.sha256(p.read_bytes()).hexdigest() != sha:
            return False
    return True


def get_sdk(dest, force):
    if sdk_ok(dest) and not force:
        print("sdk: the three pinned blobs are in %s" % (dest / "cpu" / "wl82" / "tools"))
        return True
    for rel, sha in build.SDK_SHA256.items():
        p = sdk_path(dest, rel)
        if p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == sha and not force:
            print("sdk: %s already matches" % rel)
            continue
        p.parent.mkdir(parents=True, exist_ok=True)
        url = SDK_RAW + rel
        print("sdk: %s <- %s" % (rel, url))
        try:
            with urllib.request.urlopen(url, timeout=60) as r:
                data = r.read()
        except Exception as e:                      # network, a moved tag, a proxy: say which
            print("sdk: %s could not be fetched (%s)" % (rel, e), file=sys.stderr)
            return False
        got = hashlib.sha256(data).hexdigest()
        if got != sha:                              # never write bytes nobody reviewed
            print("sdk: %s is sha256 %s, the build pins %s; not written" % (rel, got, sha), file=sys.stderr)
            return False
        p.write_bytes(data)
        print("sdk: %s %d B, sha256 verified" % (rel, len(data)))
    return sdk_ok(dest)


def check(tc, sdk):
    print("toolchain: %s" % ("ok, " + str(tc / "toolchain") if toolchain_ok(tc) else "MISSING in " + str(tc)))
    print("sdk:       %s" % ("ok, the three pinned blobs" if sdk_ok(sdk) else "MISSING in " + str(sdk)))
    if toolchain_ok(tc) and sdk_ok(sdk):
        print("next:  ulimit -n 9000; ./build.sh")
        print("       (it prints the per-engine XIP / .data+.bss table from build/felucca.syms)")
    return 0 if toolchain_ok(tc) and sdk_ok(sdk) else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--toolchain", type=Path, default=Path.home() / ".jieli", help="where DEST/toolchain goes")
    ap.add_argument("--sdk", type=Path, default=Path.home() / "fw-AC79_AIoT_SDK", help="where the SDK tree goes")
    ap.add_argument("--check", action="store_true", help="report only, write nothing")
    ap.add_argument("--force", action="store_true", help="fetch again even if what is there matches")
    a = ap.parse_args()
    if a.check:
        return check(a.toolchain, a.sdk)
    ok = get_toolchain(a.toolchain, a.force) and get_sdk(a.sdk, a.force)
    print()
    print("export JIELI_TOOLCHAIN=%s" % (a.toolchain / "toolchain"))
    print("export AC79_SDK=%s" % a.sdk)
    print()
    r = check(a.toolchain, a.sdk)
    if not shutil.which("python3"):
        print("note: python3 is what runs the build (tools/build.py)", file=sys.stderr)
    return 0 if ok and r == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
