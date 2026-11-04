#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Reproducible check: the browser page must name and load the exact WASM bytes that are packaged,
# and must identify the build by version, commit and WASM SHA-256.
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WASM = ROOT / "build" / "emu" / "felucca.wasm"
BUILD = ROOT / "build" / "emu"
SRC = ROOT / "web" / "emu" / "index.html"


def expected_commit():
    for env in ("GIT_COMMIT", "GITHUB_SHA"):
        value = os.environ.get(env, "").strip()
        if value:
            return value[:12]
    try:
        return subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short=12", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def main():
    src = (ROOT / "firmware" / "src" / "felucca.c").read_text()
    m = re.search(r'^#define FELUCCA_VERSION "([^"]+)"', src, re.M)
    assert m, "FELUCCA_VERSION missing"
    version = m.group(1)
    assert WASM.is_file(), "build/emu/felucca.wasm missing"
    blob = WASM.read_bytes()
    digest = hashlib.sha256(blob).hexdigest()
    assert version.encode() in blob, "the WASM does not contain the firmware version string"
    manifest = json.loads((BUILD / "manifest.json").read_text())
    sha = manifest.get("commit")
    assert sha and re.fullmatch(r"[0-9a-f]{7,40}|unknown", sha), manifest
    head = expected_commit()
    if head:
        assert sha == head, f"manifest commit {sha} is not this checkout's HEAD {head}"
    assert manifest == {"version": version, "commit": sha, "wasm_sha256": digest, "wasm_bytes": len(blob)}, manifest
    html = (BUILD / "index.html").read_text()
    for key in ("@VERSION@", "@COMMIT@", "@WASM_SHA@", "@WASM_SIZE@"):
        assert key not in html, f"unsubstituted placeholder {key}"
    assert f'EXPECTED_WASM_SHA = "{digest}"' in html
    assert f'BUILD_COMMIT = "{sha}"' in html
    assert f'fetch("felucca.wasm?v={digest}"' in html
    assert 'cache: "no-store"' in html
    assert 'WASM mismatch: page expects' in html
    assert f'<code id="wasm-hash">{digest}</code>' in html
    assert f'<code id="build-commit">{sha}</code>' in html
    assert f'<code id="build-commit-header">{sha}</code>' in html
    assert "worklet.js?v=\" + Date.now()" in html
    assert (BUILD / "worklet.js").is_file()
    assert "@WASM_SHA@" in SRC.read_text() and "EXPECTED_WASM_SHA" in SRC.read_text()
    assert "@COMMIT@" in SRC.read_text() and "BUILD_COMMIT" in SRC.read_text()
    assert "tools/package_emu.py" in (ROOT / "web" / "emu" / "build.sh").read_text()
    print(f"emu sync: {version}, build {sha}, {len(blob)} B, sha256 {digest}, page and manifest agree")


if __name__ == "__main__":
    main()
