#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Package the already-built emulator WASM into the browser page and its manifest.
# This is deliberately separate from emcc: it can be run to verify/package an existing
# build without pretending that a stale binary was recompiled.
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
    """The build's commit: CI's SHA, or the local checkout's, so two builds of one version differ."""
    for env in ("GIT_COMMIT", "GITHUB_SHA"):
        value = os.environ.get(env, "").strip()
        if value:
            return value[:12]
    try:
        return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short=12", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip() or "unknown"
    except (OSError, subprocess.CalledProcessError):
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
    if not WASM.exists():
        raise SystemExit("package_emu: build/emu/felucca.wasm not found; compile it first")
    blob = WASM.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    sha = commit()
    preview = " (preview)" if os.environ.get("EMU_PREVIEW", "1") != "0" else ""
    html = SRC.read_text()
    for key, value in {
        "@VERSION@": version,
        "@PREVIEW@": preview,
        "@COMMIT@": sha,
        "@WASM_SHA@": digest,
        "@WASM_SIZE@": str(len(blob)),
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
        "identity": "FM-1_982",
        "config": {"EMU_OPT": os.environ.get("EMU_OPT", "-O2"),
                   "EMU_CFLAGS": os.environ.get("EMU_CFLAGS", ""),
                   "EMU_PREVIEW": os.environ.get("EMU_PREVIEW", "1")},
        "emcc": emcc_version(),
        "dirty": dirty,
    }, indent=2, sort_keys=True) + "\n")
    print(f"emu: packaged {len(blob)} B wasm, {version}, build {sha}, sha256 {digest}")
    print(f"emu: {OUT / 'index.html'} and {OUT / 'manifest.json'}")


if __name__ == "__main__":
    main()
