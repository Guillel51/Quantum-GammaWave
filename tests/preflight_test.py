#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Host tests for tools/fm1_preflight.py: the read-only gate, no hardware.

The device is the fake FM-1 of tests/install_test.py (identity over the same
handshake the real firmware answers). Every case checks two things: the exit
code the policy promises, and that the tool never started an update.

  python3 tests/preflight_test.py [PACKAGE.fwsc]
"""
import hashlib
import io
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import fm1_install as I
import fm1_preflight as P
import install_test as T

fails = 0


def check(what, ok):
    global fails
    print(f"{what:<72} {'ok' if ok else 'FAIL'}")
    if not ok:
        fails += 1


def run(pkg, dev=None, argv_extra=()):
    out = io.StringIO()
    try:
        rc = P.main([pkg, *argv_extra], backend=dev, out=out)
    except SystemExit as e:          # argparse: an unknown flag ends the CLI, not the test run
        rc = int(e.code or 0)
    return rc, out.getvalue()


def main():
    pkg = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.fwsc"
    if not os.path.exists(pkg):
        print(f"preflight: no package at {pkg} (run ./build.sh first)")
        return 2
    raw = open(pkg, "rb").read()
    image = I.logical_image(raw)
    product = I.product_of(raw)

    tmp = tempfile.mkdtemp(prefix="fm1pf-")
    bare = os.path.join(tmp, "bare.fwsc")
    open(bare, "wb").write(raw)                       # no .sha256 beside it
    good = os.path.join(tmp, "good.fwsc")
    open(good, "wb").write(raw)
    open(good + ".sha256", "w").write(hashlib.sha256(raw).hexdigest() + "  good.fwsc\n")

    dev = T.FakeFM1(image, identity=product, name="Felucca")
    rc, out = run(good, dev)
    check("a package with its published hash and an identified chip reads as ready",
          rc == 0 and "0x856014" in out and "ready" in out)

    rep_path = os.path.join(tmp, "report.json")
    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca"), ["--json", rep_path])
    rep = json.load(open(rep_path))
    check("the report is written and states that nothing was written",
          rep["writes"] is False and rep["package"]["sha256"] == hashlib.sha256(raw).hexdigest() and
          rep["package"]["product"] == product and rep["verdict"] == "ready")
    check("the report records the chip that answered 0x12 and that the gate allowed it",
          rep["chip"]["identified"] is True and rep["chip"]["jedec"] == "0x856014"
          and rep["gate"]["allowed"] is True)

    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca", jedec=0xB36014))
    check("the UCUN part passes the same preflight as the Puya one",
          rc == 0 and "0xB36014" in out and "ready" in out)

    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca", jedec=0x123456))
    check("an undocumented part stays a NO-GO with its reason",
          rc == P.EXIT["chip"] and "no-go" in out)

    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca", query_fault="silent"))
    check("a unit that does not answer 0x12 stays a NO-GO with its reason",
          rc == P.EXIT["chip"] and "0x12" in out)

    d = T.FakeFM1(image, identity=product, name="Felucca")
    run(good, d)
    check("the preflight never starts an update (no UPGRADE frame)", d.upgrades == 0)

    rc, _ = run(bare, T.FakeFM1(image, identity=product, name="Felucca"))
    check("no published hash anywhere: refused", rc == P.EXIT["badpkg"])

    bad = os.path.join(tmp, "bad.fwsc")
    open(bad, "wb").write(raw)
    open(bad + ".sha256", "w").write("0" * 64 + "  bad.fwsc\n")
    rc, _ = run(bad, T.FakeFM1(image, identity=product, name="Felucca"))
    check("a hash that does not match: refused", rc == P.EXIT["badpkg"])

    rc, _ = run(good, T.FakeFM1(image, identity="XY-9_001", name="usb-midi"))
    check("another model: refused", rc == P.EXIT["model"])

    rc, _ = run(good, T.FakeFM1(image, identity="FM-1_905", name="Felucca"))
    check("another version of the same model reaches the gate (a downgrade is a different decision)",
          rc == 0)

    nod = T.FakeFM1(image, identity=product, name="Felucca")
    nod.connected = False
    rc, _ = run(good, nod)
    check("no device answering: refused", rc == P.EXIT["notfound"])

    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca"), ["--allow-unidentified-chip"])
    check("there is no flag that passes the gate without the chip", rc == 2)

    rc, _ = run(good, T.FakeFM1(image, identity="ota-" + product, name="Felucca Update"))
    check("a device left in update mode is reported and still gated", rc == P.EXIT["chip"])

    rc, _ = run("firmware/packages/missing.fwsc", None)
    check("a package that does not exist: refused", rc == P.EXIT["badpkg"])

    # ---- the gate lives inside the installer, not in a program the user must remember
    def inst(pkg, args=(), dev_identity=product, after=product, fault=None):
        d = T.FakeFM1(image, identity=dev_identity, name="Felucca", after_write=after, query_fault=fault)
        try:
            rc = I.main([pkg, "--yes", *args], backend=d, out=io.StringIO())
        except SystemExit as e:
            rc = int(e.code or 0)
        return rc, d

    rc, d = inst(good, fault="silent")
    check("a device that cannot answer 0x12: refused (exit 7)", rc == I.EXIT["chip"])
    check("a device that cannot answer 0x12: no UPGRADE sent", d.upgrades == 0)

    rc, d = inst(good, ("--jedec", "123456"))
    check("a typed JEDEC is not evidence: no such flag, nothing sent", rc == 2 and d.upgrades == 0)

    rc, d = inst(good, ("--allow-unidentified-chip",))
    check("an unidentified chip cannot be allowed: no such flag, nothing sent",
          rc == 2 and d.upgrades == 0)

    os.environ["FM1_LAB"] = "1"
    try:
        rc, d = inst(good, ("--jedec", "856014"))
        rc2, d2 = inst(good, ("--allow-unidentified-chip",))
    finally:
        del os.environ["FM1_LAB"]
    check("FM1_LAB=1 changes nothing: the flags are gone and no frame is sent",
          rc == 2 and d.upgrades == 0 and rc2 == 2 and d2.upgrades == 0)

    # a loader that is not this project's (Felucca's own 919, not ours): the host cannot vouch for the
    # NOR it will erase through, so nothing may be sent
    d6 = T.FakeFM1(image, identity="ota-FM-1_919", name="Felucca Update", after_write=product)
    up6 = I.Updater(d6)
    dev6 = up6.find()
    err = None
    try:
        up6.write(dev6, image, lambda *a: None)
    except I.InstallError as e:
        err = e
    check("a direct Updater.write() through a foreign loader, without evidence, is refused too",
          err is not None and err.code == "chip" and d6.upgrades == 0)

    # this project's own loader is the one exception, and it is the documented one: it identifies the
    # chip itself before it erases (firmware/loader/loader.c ldr_flash_known), so the upgrade starts
    d7 = T.FakeFM1(image, identity="ota-" + product, name="Felucca Update", after_write=product)
    up7 = I.Updater(d7)
    dev7 = up7.find()
    try:
        up7.write(dev7, image, lambda *a: None)
    except I.InstallError:
        pass
    check("this project's loader (" + product + ") may take the image without host evidence",
          d7.upgrades == 1)

    print("preflight: all passed" if not fails else f"preflight: {fails} FAILED")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
