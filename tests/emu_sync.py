#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Reproducible check: the browser page must name and load the exact WASM bytes that are packaged.
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WASM = ROOT / "build" / "emu" / "felucca.wasm"
BUILD = ROOT / "build" / "emu"
SRC = ROOT / "web" / "emu" / "index.html"


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
    assert manifest == {"version": version, "wasm_sha256": digest, "wasm_bytes": len(blob)}, manifest
    html = (BUILD / "index.html").read_text()
    assert "@VERSION@" not in html and "@WASM_SHA@" not in html and "@WASM_SIZE@" not in html
    assert f'EXPECTED_WASM_SHA = "{digest}"' in html
    assert f'fetch("felucca.wasm?v={digest}"' in html
    assert 'cache: "no-store"' in html
    assert 'WASM mismatch: page expects' in html
    assert f'<code id="wasm-hash">{digest}</code>' in html
    assert "worklet.js?v=\" + Date.now()" in html
    assert (BUILD / "worklet.js").is_file()
    assert "@WASM_SHA@" in SRC.read_text() and "EXPECTED_WASM_SHA" in SRC.read_text()
    assert "tools/package_emu.py" in (ROOT / "web" / "emu" / "build.sh").read_text()
    print(f"emu sync: {version}, {len(blob)} B, sha256 {digest}, page and manifest agree")


if __name__ == "__main__":
    main()
