#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The build manifest is only useful if a wrong one fails.

Two halves:

  * live: the manifest of this tree (`build/manifest.json`) must describe the code that is here --
    same version as `firmware/src/felucca.c`, same commit as `git rev-parse HEAD`, every artefact
    present with the sha256 it claims, and the emulator built at that commit. Skipped when there is
    no build, so `NO_PACKAGE=1` still runs;
  * the verifier itself: against a throwaway directory, a tampered artefact, a manifest naming
    another commit, a dirty build under `--require-clean`, a wrong schema and a missing file must
    each be refused. A verifier that says `ok` for everything would pass the first half by accident.

  python3 tests/manifest_test.py
"""
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import build_manifest as BM  # noqa: E402

bad = 0


def check(what, ok):
    global bad
    print(f"manifest: {what:<72} {'ok' if ok else 'FAIL'}")
    if not ok:
        bad += 1


def live():
    if not (ROOT / "build" / "manifest.json").exists():
        print("manifest: no build/manifest.json (build first); live half skipped")
        return
    errors = BM.verify()
    check("live: this tree's manifest verifies", not errors)
    for e in errors:
        print(f"    {e}")
    data = json.loads((ROOT / "build" / "manifest.json").read_text())
    check("live: it names this commit", data.get("commit") == BM.commit())
    check("live: it names this version", data.get("version") == BM.version())


def synthetic():
    tmp = Path(tempfile.mkdtemp(prefix="qg1-manifest-"))
    art = tmp / "felucca.bin"
    art.write_bytes(b"Q\x01\x81\x00app")
    old_build = BM.BUILD
    BM.BUILD = tmp
    try:
        def manifest(**over):
            m = {"schema": BM.SCHEMA, "version": BM.version(), "commit": BM.commit(), "dirty": [],
                 "artifacts": {"felucca.bin": {"sha256": BM.sha256(art), "bytes": art.stat().st_size}}}
            m.update(over)
            (tmp / "manifest.json").write_text(json.dumps(m))
            return tmp / "manifest.json"

        manifest()
        check("verifier: a correct manifest is accepted", not BM.verify())
        art.write_bytes(b"Q\x01\x81\x00apq")             # one byte changed, same size
        check("verifier: a changed artefact is refused", bool(BM.verify()))
        art.write_bytes(b"Q\x01\x81\x00app")
        check("verifier: a different commit is refused", bool(BM.verify(want_commit="0" * 40)))
        manifest(dirty=["firmware/src/fx.c"])
        check("verifier: a dirty build is refused with --require-clean",
              bool(BM.verify(require_clean=True)))
        check("verifier: but accepted without it", not BM.verify())
        manifest(schema="qg1-build-0")
        check("verifier: an unknown schema is refused", bool(BM.verify()))
        manifest(version="v0.0.0")
        check("verifier: a version that is not the firmware's is refused", bool(BM.verify()))
        (tmp / "manifest.json").write_text(json.dumps(
            {"schema": BM.SCHEMA, "version": BM.version(), "commit": BM.commit(), "dirty": [],
             "artifacts": {"felucca.bin": {"sha256": BM.sha256(art), "bytes": 1}}}))
        check("verifier: a size that does not match is refused",
              bool(BM.verify()) or (tmp / "manifest.json").exists() and False)
        (tmp / "manifest.json").unlink()
        check("verifier: no manifest at all is refused", bool(BM.verify()))
    finally:
        BM.BUILD = old_build


def main():
    live()
    synthetic()
    print("manifest test passed" if not bad else "MANIFEST TEST FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
