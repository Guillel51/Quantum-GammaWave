#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Read-only preflight for installing a Felucca package on an FM-1.

This tool never writes, never starts the update and never sends UPGRADE: it only
reads the package, talks to a port long enough to ask for the identity, and then
prints a verdict. It exists so that "can I install this?" is answered by a
program with an explicit policy instead of by a human reading a log.

What it can establish here:

  * the package bytes and their SHA-256 (integrity, not authenticity: the hash
    comes from the same site as the package unless it is given by --sha256),
  * the package identity and that the device reports the same model,
  * the device mode (running firmware or update loader),
  * that the device does NOT report the flash chip it was built on.

What it cannot establish, and therefore refuses to authorise:

  * the JEDEC of the NOR actually fitted. The identity packet (cmd 0x11) carries
    the model string only (34 bytes, text in d[6:33]); the loader identifies a
    package by the device's chip key, not by JEDEC, and there is no host-initiated
    flash read in the protocol. Without that, a write cannot be proven safe for
    the chip in front of it. See docs/FM1_INSTALL_SAFETY.md, alternatives A-D.

The chip is read the same way the installer reads it: 0x12 on the running app,
twice, in this session. A unit that does not answer it (any stock V15) or a part
outside the write allowlist is a NO-GO, and there is no flag that changes that.

Exit codes: 0 the gate would allow this install (nothing was written), 2 bad
package, 3 device not found, 6 wrong model, 7 the physical flash chip is not
identified or may not be written, 8 the device identity is inconsistent.

  fm1_preflight.py PACKAGE.fwsc [--sha256 HEX] [--port NAME] [--json FILE]
  fm1_preflight.py --info
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import fm1_gate as GATE      # the same decision the installers ask before a write
import fm1_install as I      # the wire format, the ports and the package rules live there

EXIT = {"badpkg": 2, "notfound": 3, "model": 6, "chip": 7, "identity": 8}

# The documented 1 MiB parts. A device that reports one of these over JEDEC is
# accepted; the installer cannot read it, so this list is used by the policy only.
KNOWN_JEDEC = {
    0x856014: "Puya P25Q80H (1 MiB)",
    0xB36014: "UCUN 1 MiB",
}

MAX_PACKAGE = 0x100000        # the part is 1 MiB and the image is the whole flash


class Refused(Exception):
    def __init__(self, code, reason, detail=""):
        super().__init__(reason)
        self.code, self.reason, self.detail = code, reason, detail


def package_report(path, sha=None):
    """Everything the host can prove about the bytes, without touching a device."""
    try:
        with open(path, "rb") as f:
            raw = f.read()
    except OSError as e:
        raise Refused("badpkg", "package_unreadable", f"{path}: {e.strerror}")
    if len(raw) > MAX_PACKAGE:
        raise Refused("badpkg", "package_too_large", f"{len(raw)} B > {MAX_PACKAGE} B")
    sha256, _ = I.check_sha256(raw, path, sha)
    product = I.product_of(raw)
    if not I.re.match(r"[^_]+_\d+", product):
        raise Refused("badpkg", "bad_identity", f"identity {product!r}")
    official = sha256 == I.STOCK_V15_SHA256
    if I.LOADER_MARK not in raw and not official:
        raise Refused("badpkg", "no_loader_marker",
                      "no Felucca update loader in this package (only Felucca packages and the "
                      "unmodified official V15 are installable)")
    image = I.logical_image(raw)
    return {
        "path": os.path.abspath(path),
        "bytes": len(raw),
        "sha256": sha256,
        "product": product,
        "model": I.model_of(product),
        "loader": "Felucca" if not official else "official V15",
        "logical_bytes": len(image),
    }


def chip_report(jedec, why=None):
    """The physical NOR as 0x12 answered it, or why it did not."""
    if jedec is None:
        return {
            "jedec": None,
            "part": None,
            "identified": False,
            "known": False,
            "why": why or "0x12 did not answer: this unit's firmware cannot identify its NOR",
        }
    known = jedec in KNOWN_JEDEC
    return {
        "jedec": f"0x{jedec:06X}",
        "part": KNOWN_JEDEC.get(jedec),
        "identified": True,
        "known": known,
        "why": None if known else "not in the documented 1 MiB allowlist",
    }


def preflight(package, sha=None, backend=None, port=None):
    """-> the report dict; raises Refused when the policy says no."""
    report = {"tool": "fm1_preflight", "version": 1, "writes": False}
    report["package"] = package_report(package, sha)
    model = report["package"]["model"]

    up = I.Updater(backend, port) if backend is not None else I.Updater(None, port)
    dev = up.find()
    if not dev:
        raise Refused("notfound", "device_not_found", "no FM-1 port answered the handshake")
    jedec, why = None, None
    try:
        ident = dev.id
        report["device"] = {
            "identity": ident.text,
            "model": I.model_of(ident.text),
            "mode": "loader" if ident.loader else "running",
            "port": dev.name,
        }
        if not ident.loader:                 # the loader cannot identify its NOR; the app can
            try:
                jedec = I.PROBE.probe(dev.link)["jedec"]
            except I.PROBE.ProbeError as e:
                why = f"0x12 did not identify the NOR ({e.code}): {e}"
        else:
            why = "the unit is in update mode: the loader cannot identify its NOR"
    finally:
        dev.link.close()

    if report["device"]["model"] != model:
        raise Refused("model", "model_mismatch",
                      f"device {report['device']['identity']} vs package {report['package']['product']}")
    if not report["device"]["identity"] or report["device"]["identity"].count("_") != 1:
        raise Refused("identity", "inconsistent_identity", report["device"]["identity"])

    report["chip"] = chip_report(jedec, why)
    gate = GATE.authorise(report["package"]["product"], report["device"]["identity"], jedec)
    report["gate"] = gate
    if gate["allowed"]:
        report["verdict"] = "ready"
        report["reasons"] = []
    else:
        report["verdict"] = "no-go"
        report["reasons"] = [f"{gate['reason']}: {gate['detail']}" if gate["detail"] else gate["reason"]]
    return report


def main(argv=None, backend=None, out=sys.stdout):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package", nargs="?", help="Felucca package (.fwsc)")
    ap.add_argument("--sha256", metavar="HEX", help="expected SHA-256 (else PACKAGE.sha256 / SHA256SUMS)")
    ap.add_argument("--port", metavar="NAME", help="MIDI port to use (part of its name)")
    ap.add_argument("--json", metavar="FILE", help="write the report here (a record of what was checked)")
    ap.add_argument("--info", action="store_true", help="print the identity of the connected FM-1 and exit")
    a = ap.parse_args(argv)
    if bool(a.info) == bool(a.package):
        ap.error("give a PACKAGE.fwsc or --info")
    try:
        if backend is None:
            try:
                backend = I.MidoBackend()
            except ImportError:
                raise Refused("badpkg", "no_midi", "needs mido and python-rtmidi (pip install mido python-rtmidi)")
        if a.info:
            up = I.Updater(backend, a.port)
            dev = up.find()
            if not dev:
                raise Refused("notfound", "device_not_found", "no FM-1 port answered the handshake")
            dev.link.close()
            print(f"{dev.id.text}  [{'loader' if dev.id.loader else 'running'}]  port: {dev.name}", file=out)
            print("chip: unknown (the identity packet does not carry the JEDEC)", file=out)
            return 0
        rep = preflight(a.package, a.sha256, backend, a.port)
    except Refused as e:
        print(f"refused: {e.reason}: {e.detail}", file=sys.stderr)
        return EXIT.get(e.code, 1)
    except I.InstallError as e:
        print(f"refused: {e}", file=sys.stderr)
        return I.EXIT.get(e.code, 1)
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(rep, f, indent=2, sort_keys=True)
            f.write("\n")
    p, d, c = rep["package"], rep["device"], rep["chip"]
    print(f"package: {p['product']}  {p['bytes']} B  sha256 {p['sha256']}", file=out)
    print(f"device:  {d['identity']}  [{d['mode']}]  port: {d['port']}", file=out)
    print(f"chip:    {c['jedec'] or 'unknown'}"
          + (f"  ({c['part']})" if c["part"] else ""), file=out)
    print(f"verdict: {rep['verdict']}", file=out)
    for r in rep["reasons"]:
        print(f"  - {r}", file=out)
    if rep["verdict"] == "no-go":
        return EXIT["chip"]
    return 0


if __name__ == "__main__":
    sys.exit(main())
