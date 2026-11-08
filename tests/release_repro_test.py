#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The signed package, reproduced from the tree it claims.

`tools/fm1_sign.py verify` proves the bytes were signed by the pinned key, and the publish job proves
the record names a commit whose firmware has not changed since. Neither answers the question this test
asks: **does this source tree still produce those exact bytes?** A release whose package cannot be
rebuilt is a package nobody can audit, so the answer is checked here against a fresh `./build.sh`.

Three states, and only one of them can pass:

  * the committed package does not match its own record (hash or size) -> FAIL, always;
  * the record names an older tree (firmware/tools/web/emu changed since) -> SKIP with a notice: that
    is a pending release, not a reproducibility regression, and the `publish` job already reports it;
  * the tree and the record are the same source -> the freshly built `build/felucca.fwsc` must be
    byte-identical to `release/*.fwsc` (and `build/loader/ota.bin` to the loader it records), or FAIL,
    naming the first differing offset. A tree that builds something else is the defect.

Without `build/felucca.fwsc` (the host-only CI job, `NO_PACKAGE=1`) there is nothing to compare against
and the test says so instead of passing silently.

  python3 tests/release_repro_test.py
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
failed = 0
skipped = 0


def ok(cond, what, detail=""):
    global failed
    print(f"{what:<70} {'ok' if cond else 'FAIL'}")
    if not cond:
        failed += 1
        if detail:
            print(f"    {detail}")
    return cond


def skip(what, why):
    global skipped
    skipped += 1
    print(f"{what:<70} skip")
    print(f"    {why}")
    if os.environ.get("GITHUB_ACTIONS"):
        print(f"::notice title=release reproducibility::{why}")   # an annotation, not a silent skip


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)


def main():
    rec_path = ROOT / "release" / "build-manifest.json"
    pkgs = sorted((ROOT / "release").glob("*.fwsc"))
    if not rec_path.exists() or not pkgs:
        ok(False, "there is a committed release to reproduce", f"{rec_path.name}: missing")
        return 1
    rec = json.loads(rec_path.read_text())
    pkg = pkgs[0]

    art = rec.get("artifacts", {}).get("felucca.fwsc", {})
    sha, size = sha256(pkg), pkg.stat().st_size
    ok(rec.get("dirty") == [], "the record says it was built from a clean tree", f"dirty: {rec.get('dirty')}")
    ok(art.get("sha256") == sha, "the committed package is the one the record hashes",
       f"record {art.get('sha256')}, file {sha}")
    ok(art.get("bytes") == size, "the committed package is the size the record states",
       f"record {art.get('bytes')} B, file {size} B")
    if failed:
        return 1                                     # the rest compares against something untrue

    commit = str(rec.get("commit", ""))
    head = git("rev-parse", "HEAD").stdout.strip()
    ok(len(commit) == 40 and git("cat-file", "-e", f"{commit}^{{commit}}").returncode == 0,
       "the record names a commit this checkout has", commit)
    if failed:
        return 1

    # the package is only meant to come out of the tree the record names: everything that reaches the
    # bytes must be identical to HEAD's, or this check would demand a signature for code not released
    changed = git("diff", "--name-only", f"{commit}..HEAD", "--", "firmware", "tools", "web/emu").stdout.split()
    if changed:
        skip("the signed package reproduces from this tree",
             f"the record names {commit[:7]}; {len(changed)} firmware/tools path(s) changed since "
             f"({', '.join(changed[:3])}{', …' if len(changed) > 3 else ''}): a pending release, which the "
             "publish job reports, not a reproducibility failure")
        print(f"\nrelease reproducibility: 1 skipped, {failed} failed")
        return failed

    built = ROOT / "build" / "felucca.fwsc"
    if not built.exists():
        skip("the signed package reproduces from this tree",
             "no build/felucca.fwsc: run ./build.sh first (the host-only job has no target package)")
        print(f"\nrelease reproducibility: 1 skipped, {failed} failed")
        return failed

    build_rec_path = ROOT / "build" / "manifest.json"
    if build_rec_path.exists():
        built_at = json.loads(build_rec_path.read_text()).get("commit", "")
        ok(built_at[:12] == head[:12],
           "the package here was built at HEAD, not at another commit",
           f"build/manifest.json: {built_at[:12]}, HEAD: {head[:12]}")
        if failed:
            return 1

    fresh, signed = built.read_bytes(), pkg.read_bytes()
    ok(fresh == signed, "a fresh build is byte-identical to the signed package",
       f"{len(fresh)} B sha {hashlib.sha256(fresh).hexdigest()[:16]} != "
       f"{len(signed)} B sha {hashlib.sha256(signed).hexdigest()[:16]}")
    if fresh != signed:
        off = next((i for i, (a, b) in enumerate(zip(fresh, signed)) if a != b), min(len(fresh), len(signed)))
        print(f"    first difference at byte {off} (0x{off:X})")

    ota = ROOT / "build" / "loader" / "ota.bin"
    ota_sha = rec.get("artifacts", {}).get("loader/ota.bin", {}).get("sha256")
    if ota.exists() and ota_sha:
        ok(sha256(ota) == ota_sha, "the loader built here is the one the record names",
           f"record {ota_sha[:16]}, file {sha256(ota)[:16]}")

    print(f"\nrelease reproducibility: {failed} failed")
    return failed


if __name__ == "__main__":
    sys.exit(1 if main() else 0)
