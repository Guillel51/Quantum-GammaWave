#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The published site, checked against itself: one build, or a named mismatch.

`web/make_site.py` refuses to *write* a site whose labels contradict the package's build record, and
CI builds the package and the site from one commit. Neither of them can see what GitHub Pages is
serving an hour later: a deploy that failed, a Pages cache holding the previous artifact, or a
`versions.json` from this run next to an emulator from the last one would all look fine from inside
the checkout. This reads the published origin over HTTPS and holds every artefact to the commit the
page claims:

  versions.json      the newest entry: version label, package file, size, SHA-256, build commit,
                     identity and the emulator record of the emulator served beside it
  the package        that file, downloaded: its size and SHA-256 must be the versions.json ones
  emulator/manifest  its commit must be the package's, its wasm size and hash must be the file's
  emulator/felucca.wasm  downloaded: size and SHA-256 must be the manifest's
  the portal         must print the same version label in its hero
  the installer      must offer that version (the page's own copy of versions.json)

    python3 tools/site_check.py                     # the public site, newest version
    python3 tools/site_check.py --url http://127.0.0.1:8000 --expect-commit fd53c29

`--expect-commit` is the commit the *package* was built from, which is the build record's commit and
not necessarily HEAD: a release stays the release while main moves on. `--expect-sha256` holds the
package to the exact signed bytes, and `--expect-emu-commit` names the emulator build that this run
copied to the site -- the two are different artefacts with different commits, and the check says
which is which instead of pretending they are the same commit.

Exit code 0 only when every line says ok. Anything that cannot be fetched or parsed is a failure, not
a skip: a site whose files cannot be read is not a site that can be trusted.
"""
import argparse
import hashlib
import json
import sys
import urllib.error
import urllib.request

DEFAULT_URL = "https://guillel51.github.io/Quantum-GammaWave/"
TIMEOUT = 60
bad = 0


def ok(what, cond, detail=""):
    global bad
    bad += not cond
    print("site: %-66s %s%s" % (what, "ok" if cond else "FAIL", ("  " + detail) if detail and not cond else ""))


def get(url, binary=False):
    req = urllib.request.Request(url, headers={"Cache-Control": "no-cache", "User-Agent": "qg1-site-check"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        data = r.read()
    return data if binary else data.decode("utf-8", "replace")


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def main(argv=None):
    ap = argparse.ArgumentParser(description="check that the published site serves one build")
    ap.add_argument("--url", default=DEFAULT_URL)
    ap.add_argument("--expect-commit", default=None, help="the commit the published package was built from")
    ap.add_argument("--expect-sha256", default=None, help="the package's SHA-256, when it is a known artefact")
    ap.add_argument("--expect-emu-commit", default=None, help="the commit the served emulator was built from")
    args = ap.parse_args(argv)
    base = args.url.rstrip("/") + "/"

    try:
        versions = json.loads(get(base + "firmware/versions.json"))
    except (urllib.error.URLError, ValueError, OSError) as e:
        print("site: versions.json unreadable: %s" % e)
        return 1
    if not versions:
        print("site: versions.json lists no version")
        return 1
    top = versions[0]
    version, build = top.get("version", ""), top.get("build") or {}
    commit = str(build.get("commit") or "")
    emu_rec = build.get("emulator") or {}
    ok("versions.json names a version and a full commit",
       bool(version) and len(commit) == 40, "%r %r" % (version, commit[:7]))
    if args.expect_commit:
        ok("the published package is the commit asked for",
           commit.startswith(args.expect_commit), "%s vs %s" % (commit[:7], args.expect_commit))
    if args.expect_sha256:
        ok("the published package is the signed artefact",
           str(top.get("sha256")) == args.expect_sha256,
           "%s vs %s" % (str(top.get("sha256"))[:16], args.expect_sha256[:16]))

    pkg_url = base + "firmware/" + str(top.get("file") or "")
    try:
        pkg = get(pkg_url, binary=True)
    except (urllib.error.URLError, OSError) as e:
        ok("the package is downloadable", False, "%s: %s" % (pkg_url, e))
        pkg = b""
    if pkg:
        ok("the package's size is versions.json's", len(pkg) == top.get("size"),
           "%d vs %r" % (len(pkg), top.get("size")))
        ok("the package's SHA-256 is versions.json's", sha256(pkg) == top.get("sha256"),
           sha256(pkg)[:16] + " vs " + str(top.get("sha256"))[:16])

    try:
        emu = json.loads(get(base + "emulator/manifest.json"))
    except (urllib.error.URLError, ValueError, OSError) as e:
        ok("emulator/manifest.json is readable", False, str(e))
        emu = {}
    if emu:
        if args.expect_emu_commit:
            ok("the emulator is the build this run copied up",
               str(emu.get("commit") or "").startswith(args.expect_emu_commit),
               "%r vs %s" % (emu.get("commit"), args.expect_emu_commit))
        else:
            ok("the emulator names the commit it was built from", bool(emu.get("commit")),
               "%r" % (emu.get("commit"),))
        ok("the emulator names the same version", emu.get("version") == build.get("version"),
           "%r vs %r" % (emu.get("version"), build.get("version")))
        if emu_rec:
            ok("versions.json's emulator record is the emulator's wasm",
               emu_rec.get("wasm_sha256") == emu.get("wasm_sha256") and
               emu_rec.get("wasm_bytes") == emu.get("wasm_bytes"),
               "record %r/%r" % (str(emu_rec.get("wasm_sha256"))[:12], emu_rec.get("wasm_bytes")))
        try:
            wasm = get(base + "emulator/felucca.wasm", binary=True)
        except (urllib.error.URLError, OSError) as e:
            ok("the emulator's wasm is downloadable", False, str(e))
            wasm = b""
        if wasm:
            ok("the wasm's size is the manifest's", len(wasm) == emu.get("wasm_bytes"),
               "%d vs %r" % (len(wasm), emu.get("wasm_bytes")))
            ok("the wasm's SHA-256 is the manifest's", sha256(wasm) == emu.get("wasm_sha256"),
               sha256(wasm)[:16] + " vs " + str(emu.get("wasm_sha256"))[:16])

    try:
        portal = get(base)
    except (urllib.error.URLError, OSError) as e:
        ok("the portal is readable", False, str(e))
        portal = ""
    if portal:
        ok("the portal prints the published version", version in portal, version)
        ok("the portal offers the emulator", "/emulator/" in portal)
    try:
        installer = get(base + "webapp/installer/")
    except (urllib.error.URLError, OSError) as e:
        ok("the installer page is readable", False, str(e))
        installer = ""
    if installer:
        ok("the installer offers the published version", version in installer, version)
        ok("the installer page is the one with the Verify Only checklist", "vrep-gate" in installer)

    print("site: %s" % ("all checks passed" if not bad else "%d FAILED" % bad))
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
