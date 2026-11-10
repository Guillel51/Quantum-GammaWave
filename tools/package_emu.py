#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Package the already-built emulator WASM into the browser page and its manifest.
# This is deliberately separate from emcc: it can be run to verify/package an existing
# build without pretending that a stale binary was recompiled.
import datetime
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "web" / "emu" / "index.html"
WASM = ROOT / "build" / "emu" / "felucca.wasm"
OUT = ROOT / "build" / "emu"


def commit():
    """The commit of the sources this WASM was built from, so two builds of one version differ.

    The tree comes first, not the environment. On CI the checkout *is* GITHUB_SHA, so both agree; but the
    stable channel's emulator is built in a worktree at the signed release's commit, and there GITHUB_SHA is
    the deploy's commit. Reading it first labelled a WASM built from 03d3f80 as bbca837 -- the page named a
    commit its bytes did not come from, while versions.json said 03d3f80 (measured on the live site,
    2026-11-10). git answers for the worktree it is run in; the environment is the fallback for a checkout
    without a repository."""
    try:
        value = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short=12", "HEAD"],
                               capture_output=True, text=True, check=True).stdout.strip()
        if value:
            return value
    except (OSError, subprocess.CalledProcessError):
        pass
    for env in ("GIT_COMMIT", "GITHUB_SHA"):
        value = os.environ.get(env, "").strip()
        if value:
            return value[:12]
    return "unknown"


def emcc_version():
    """Which compiler made this WASM; `unknown` when emcc is not on PATH (re-packaging only)."""
    try:
        out = subprocess.run(["emcc", "--version"], capture_output=True, text=True, timeout=30).stdout
        return out.splitlines()[0] if out else "unknown"
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def emu_dirty():
    """Uncommitted files outside build/, as the firmware's manifest records them."""
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain", "--untracked-files=normal"],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return []
    return [ln[3:].strip().strip('"') for ln in out.splitlines()
            if ln[3:].strip().strip('"') and not ln[3:].strip().strip('"').startswith("build/")]


def main():
    text = (ROOT / "firmware" / "src" / "felucca.c").read_text()
    m = re.search(r'^#define FELUCCA_VERSION "([^"]+)"', text, re.M)
    if not m:
        raise SystemExit("package_emu: FELUCCA_VERSION not found")
    version = m.group(1)
    # the identity the WASM answers with is the same single constant the package is built with
    # (tools/build.py PRODUCT, docs/IDENTITY.md), read here instead of copied
    m = re.search(r'^PRODUCT = "([^"]+)"', (ROOT / "tools" / "build.py").read_text(), re.M)
    if not m:
        raise SystemExit("package_emu: PRODUCT not found in tools/build.py")
    product = m.group(1)
    if not WASM.exists():
        raise SystemExit("package_emu: build/emu/felucca.wasm not found; compile it first")
    blob = WASM.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    sha = commit()
    preview = " (preview)" if os.environ.get("EMU_PREVIEW", "1") != "0" else ""
    # (3.3) THE BUILD'S DATE, IN THE PAGE AND IN THE MANIFEST. The footer already names the version, the
    # commit and the wasm's SHA-256; a date is what tells a reader whether the page in front of them is the
    # build of tonight or the one from last week. SOURCE_DATE_EPOCH makes it reproducible when a caller
    # needs two builds of one tree to be identical files.
    _epoch = os.environ.get("SOURCE_DATE_EPOCH")
    built = datetime.datetime.fromtimestamp(int(_epoch), datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ") \
        if _epoch and _epoch.isdigit() else datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    html = SRC.read_text()
    for key, value in {
        "@VERSION@": version,
        "@PREVIEW@": preview,
        "@COMMIT@": sha,
        "@WASM_SHA@": digest,
        "@WASM_SIZE@": str(len(blob)),
        "@BUILT@": built,
    }.items():
        html = html.replace(key, value)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "index.html").write_text(html)
    # (v1.9.0) The manifest has to say enough for tools/build_manifest.py to refuse a stale one: the
    # commit above, and the knobs this particular WASM was compiled with (EMU_OPT / EMU_CFLAGS change
    # the code, so two builds of one commit are not interchangeable silently).
    dirty = emu_dirty()
    (OUT / "manifest.json").write_text(json.dumps({
        "schema": "qg1-emu-1",
        "version": version,
        "commit": sha,
        "wasm_sha256": digest,
        "wasm_bytes": len(blob),
        "identity": product,
        "config": {"EMU_OPT": os.environ.get("EMU_OPT", "-O2"),
                   "EMU_CFLAGS": os.environ.get("EMU_CFLAGS", ""),
                   "EMU_PREVIEW": os.environ.get("EMU_PREVIEW", "1")},
        "built": built,
        "emcc": emcc_version(),
        "dirty": dirty,
    }, indent=2, sort_keys=True) + "\n")
    print(f"emu: packaged {len(blob)} B wasm, {version}, build {sha}, sha256 {digest}")
    print(f"emu: {OUT / 'index.html'} and {OUT / 'manifest.json'}")


if __name__ == "__main__":
    main()
