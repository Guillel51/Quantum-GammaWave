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
    (OUT / "manifest.json").write_text(json.dumps({
        "version": version,
        "commit": sha,
        "wasm_sha256": digest,
        "wasm_bytes": len(blob),
    }, separators=(",", ":")) + "\n")
    print(f"emu: packaged {len(blob)} B wasm, {version}, build {sha}, sha256 {digest}")
    print(f"emu: {OUT / 'index.html'} and {OUT / 'manifest.json'}")


if __name__ == "__main__":
    main()
