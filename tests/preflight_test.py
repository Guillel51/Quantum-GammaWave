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
    rc = P.main([pkg, *argv_extra], backend=dev, out=out)
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
    check("a package with its published hash and a matching device reaches the chip gate",
          rc == P.EXIT["chip"] and "unknown" in out and "no-go" in out)

    rep_path = os.path.join(tmp, "report.json")
    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca"), ["--json", rep_path])
    rep = json.load(open(rep_path))
    check("the report is written and states that nothing was written",
          rep["writes"] is False and rep["package"]["sha256"] == hashlib.sha256(raw).hexdigest() and
          rep["package"]["product"] == product and rep["verdict"] == "no-go")
    check("the report names the chip as unidentified with a reason",
          rep["chip"]["identified"] is False and rep["chip"]["jedec"] is None and rep["chip"]["why"])

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
    check("another version of the same model reaches the chip gate (a downgrade is a different decision)",
          rc == P.EXIT["chip"])

    nod = T.FakeFM1(image, identity=product, name="Felucca")
    nod.connected = False
    rc, _ = run(good, nod)
    check("no device answering: refused", rc == P.EXIT["notfound"])

    rc, out = run(good, T.FakeFM1(image, identity=product, name="Felucca"), ["--allow-unidentified-chip"])
    check("the lab override passes the gate and still writes nothing",
          rc == 0 and "lab-override" in out)

    rc, _ = run(good, T.FakeFM1(image, identity="ota-" + product, name="Felucca Update"))
    check("a device left in update mode is reported and still gated", rc == P.EXIT["chip"])

    rc, _ = run("firmware/packages/missing.fwsc", None)
    check("a package that does not exist: refused", rc == P.EXIT["badpkg"])

    # ---- the gate lives inside the installer, not in a program the user must remember
    def inst(pkg, args=(), dev_identity=product, after=product):
        d = T.FakeFM1(image, identity=dev_identity, name="Felucca", after_write=after)
        rc = I.main([pkg, "--yes", *args], backend=d, out=io.StringIO())
        return rc, d

    rc, d = inst(good)
    check("installer without chip evidence: refused (exit 7)", rc == I.EXIT["chip"])
    check("installer without chip evidence: no UPGRADE reached the device", d.upgrades == 0)

    rc, d = inst(good, ("--jedec", "123456"))
    check("installer with an unknown JEDEC: refused, nothing sent", rc == I.EXIT["chip"] and d.upgrades == 0)

    rc, d = inst(good, ("--allow-unidentified-chip",))
    check("lab override without FM1_LAB=1: usage error, nothing sent", rc == 2 and d.upgrades == 0)

    rc, d = inst(good, ("--jedec", "856014"))
    check("installer with a documented JEDEC read on this unit: installs", rc == 0 and d.upgrades >= 1)

    os.environ["FM1_LAB"] = "1"
    try:
        rc, d = inst(good, ("--allow-unidentified-chip",))
    finally:
        del os.environ["FM1_LAB"]
    check("lab override with FM1_LAB=1: runs (harness only)", rc == 0 and d.upgrades >= 1)

    d6 = T.FakeFM1(image, identity="ota-" + product, name="Felucca Update", after_write=product)
    up6 = I.Updater(d6)
    dev6 = up6.find()
    err = None
    try:
        up6.write(dev6, image, lambda *a: None)
    except I.InstallError as e:
        err = e
    check("a direct Updater.write() without evidence is refused too",
          err is not None and err.code == "chip" and d6.upgrades == 0)

    print("preflight: all passed" if not fails else f"preflight: {fails} FAILED")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
