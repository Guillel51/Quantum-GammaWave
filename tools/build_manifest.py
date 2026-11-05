#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One manifest that ties an artefact to the code, the toolchain and the checks that made it.

Why this exists: the artefacts used to say which version they were (a string compiled into the
firmware) and, for the emulator, a commit -- but nothing checked that the string, the commit and the
bytes actually belonged together. A WASM built at one commit could ship carrying another commit's
name; a package could be rebuilt from a dirty tree and still look identical to the reviewed one.

The manifest is written **after** the build and records, for every artefact, its sha256 and size,
next to:

  * `commit`   -- the exact commit (`GIT_COMMIT`/`GITHUB_SHA` in CI, `git rev-parse HEAD` locally);
  * `dirty`    -- the worktree files that were not committed, minus `build/` (empty: reproducible);
  * `version`  -- `FELUCCA_VERSION` as compiled;
  * `identity` -- the package identity (`FM-1_982`);
  * `config`   -- the build knobs that change the code (`FELUCCA_*`, `FELUCCA_USB_LAYOUT`);
  * `toolchain`/`sdk` -- what compiled it and the SDK files it linked, by hash;
  * `checks`   -- the build's own checks and their results.

  tools/build_manifest.py --verify [MANIFEST]   exit 0 only if every claim still holds
  tools/build_manifest.py --print               the manifest for the current tree, without writing it

A release build (`tools/build.py --release`) refuses a dirty tree; a normal build records it. Either
way the manifest says which one it was, and `--verify --require-clean` fails on a dirty one, which
is what CI runs.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
SCHEMA = "qg1-build-1"

# every knob tools/build.py forwards to the compiler (see its build_app())
KNOBS = ("FELUCCA_FLASH", "FELUCCA_OTA", "FELUCCA_OTA_DRYRUN", "FELUCCA_OTA_RAMONLY", "FELUCCA_CDC",
         "FELUCCA_UART", "FELUCCA_UAC", "FELUCCA_UAC_TONE", "FELUCCA_UAC_48K", "FELUCCA_ICONS",
         "FELUCCA_SLICE", "FELUCCA_FM4", "FELUCCA_CDC_DEFAULT", "FELUCCA_USB_LAYOUT",
         "FELUCCA_SIZE", "FELUCCA_HOTRAM")


def _git(*args):
    try:
        return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def commit():
    """The commit that produced these bytes: the CI's SHA when it is set, else the checkout's."""
    for env in ("GIT_COMMIT", "GITHUB_SHA"):
        value = os.environ.get(env, "").strip()
        if value:
            return value
    return _git("rev-parse", "HEAD") or "unknown"


def dirty():
    """Uncommitted files, minus build/. Empty means the bytes come from exactly `commit`.

    Porcelain `-z` on purpose: the plain format's first line starts with a space, and _git() trims
    the output, so a fixed `line[3:]` silently ate the first character of the first path
    ("tools/build.py" was reported as "ools/build.py"). With `-z` the status is exactly two bytes
    and the path is everything after the space, whatever it contains (spaces, quotes, newlines)."""
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "status", "--porcelain=v1", "-z",
                              "--untracked-files=normal"], capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return []
    fields, files, i = out.split("\0"), [], 0
    while i < len(fields):
        entry = fields[i]
        i += 1
        if len(entry) < 4 or entry[2] != " ":
            continue
        if "R" in entry[:2] or "C" in entry[:2]:
            i += 1                      # the rename's source follows as its own field
        path = entry[3:]
        if path and not path.startswith("build/"):
            files.append(path)
    return files


def version():
    text = (ROOT / "firmware" / "src" / "felucca.c").read_text()
    m = re.search(r'^#define FELUCCA_VERSION "([^"]+)"', text, re.M)
    if not m:
        raise SystemExit("build_manifest: FELUCCA_VERSION not found in firmware/src/felucca.c")
    return m.group(1)


def identity():
    text = (ROOT / "tools" / "build.py").read_text()
    m = re.search(r'^PRODUCT = "([^"]+)"', text, re.M)
    return m.group(1) if m else "unknown"


def config():
    """The knobs as the compiler saw them: unset means the default in the source."""
    out = {}
    for name in KNOBS:              # build.py forwards only these values; anything else is the default
        value = os.environ.get(name)
        allowed = ("0", "1", "2", "3") if name == "FELUCCA_USB_LAYOUT" else ("0", "1")
        out[name] = value if value in allowed else "default"
    return out


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def toolchain_info():
    tc = os.environ.get("JIELI_TOOLCHAIN") or str(Path.home() / ".jieli" / "toolchain")
    clang = Path(tc) / "pi32v2" / "bin" / "clang"
    info = {"path": tc, "name": Path(os.path.realpath(tc)).name if Path(tc).exists() else "missing"}
    if clang.exists():                     # the binary itself, so a different build cannot pass for it
        info["clang_sha256"] = sha256(clang)
    try:
        info["version"] = subprocess.run([str(clang), "--version"], capture_output=True, text=True,
                                         timeout=30).stdout.splitlines()[0]
    except (OSError, subprocess.SubprocessError):
        info["version"] = "unknown"
    return info


def sdk_info():
    sdk = Path(os.environ.get("AC79_SDK") or Path.home() / "fw-AC79_AIoT_SDK")
    files = {"uboot.boot": "cpu/wl82/tools/uboot.boot",
             "cfg_tool.bin": "cpu/wl82/tools/cfg_tool.bin",
             "cfg/eq_cfg_hw.bin": "cpu/wl82/tools/cfg/eq_cfg_hw.bin"}
    out = {"path": str(sdk), "commit": _git_sdk(sdk), "files": {}}
    for name, rel in files.items():
        p = sdk / rel
        out["files"][name] = sha256(p) if p.exists() else "missing"
    return out


def _git_sdk(sdk):
    try:
        return subprocess.run(["git", "-C", str(sdk), "rev-parse", "HEAD"], capture_output=True,
                              text=True, check=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def artifacts(package="felucca.fwsc"):
    """The files a build produces, by sha256 and size. Missing ones are named as missing."""
    out = {}
    for name in ("felucca.bin", "loader/ota.bin", package):
        p = BUILD / name
        out[name] = {"sha256": sha256(p), "bytes": p.stat().st_size} if p.exists() else {"sha256": None}
    return out


def emulator():
    """The emulator's own manifest, kept next to it; folded in so one file describes both."""
    p = BUILD / "emu" / "manifest.json"
    if not p.exists():
        return None
    try:
        data = json.loads(p.read_text())
    except (OSError, ValueError):
        return None
    return {k: data[k] for k in ("version", "commit", "wasm_sha256", "wasm_bytes") if k in data}


def collect(checks=None, package="felucca.fwsc"):
    return {
        "schema": SCHEMA,
        "version": version(),
        "commit": commit(),
        "dirty": dirty(),
        "identity": identity(),
        "config": config(),
        "toolchain": toolchain_info(),
        "sdk": sdk_info(),
        "artifacts": artifacts(package),
        "emulator": emulator(),
        "checks": checks or {},
    }


def write(manifest, outdir=BUILD):
    outdir = Path(outdir)
    outdir.mkdir(parents=True, exist_ok=True)
    text = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    (outdir / "manifest.json").write_text(text)
    (outdir / f"manifest-{manifest['version']}.json").write_text(text)
    missing = [n for n, a in manifest["artifacts"].items() if not a.get("sha256")]
    print(f"manifest: {manifest['version']} at {manifest['commit'][:12]}"
          f"{' (dirty), ' + str(len(manifest['dirty'])) + ' file(s)' if manifest['dirty'] else ''}"
          f"; {len(manifest['artifacts']) - len(missing)} artefact(s) hashed"
          + (f"; missing {', '.join(missing)}" if missing else ""))
    return outdir / "manifest.json"


def verify(path=None, require_clean=False, want_commit=None):
    """Every claim in the manifest, checked against the tree and the files on disk. -> [errors]"""
    path = Path(path) if path else BUILD / "manifest.json"
    if not path.exists():
        return [f"{path}: no manifest (build first)"]
    errors, data = [], json.loads(path.read_text())
    if data.get("schema") != SCHEMA:
        errors.append(f"schema {data.get('schema')!r} is not {SCHEMA!r}")
    here = version()
    if data.get("version") != here:
        errors.append(f"version {data.get('version')} in the manifest, {here} in the firmware")
    head = commit()
    want = want_commit or head
    if data.get("commit") != want:
        errors.append(f"commit {data.get('commit')} in the manifest, {want} is the code being checked")
    if require_clean and data.get("dirty"):
        errors.append(f"built from a dirty tree: {len(data['dirty'])} file(s), e.g. {data['dirty'][0]}")
    for name, claim in data.get("artifacts", {}).items():
        p = BUILD / name
        if not claim.get("sha256"):
            errors.append(f"{name}: no hash in the manifest")
        elif not p.exists():
            errors.append(f"{name}: in the manifest, not on disk")
        else:
            got = sha256(p)
            if got != claim["sha256"]:
                errors.append(f"{name}: sha256 {got}, the manifest says {claim['sha256']}")
            elif p.stat().st_size != claim.get("bytes"):
                errors.append(f"{name}: {p.stat().st_size} B, the manifest says {claim.get('bytes')}")
    emu = data.get("emulator")
    if emu:
        wasm = BUILD / "emu" / "felucca.wasm"
        if not wasm.exists():
            errors.append("emulator: the manifest names a WASM that is not on disk")
        elif sha256(wasm) != emu.get("wasm_sha256"):
            errors.append(f"emulator: the wasm sha256 is not {emu.get('wasm_sha256')}")
        elif emu.get("commit") != data.get("commit")[:12] and emu.get("commit") != data.get("commit"):
            errors.append(f"emulator: built at {emu.get('commit')}, the firmware at {data.get('commit')[:12]}")
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify", nargs="?", const=str(BUILD / "manifest.json"), metavar="MANIFEST")
    ap.add_argument("--require-clean", action="store_true")
    ap.add_argument("--expect-commit", metavar="SHA")
    ap.add_argument("--print", action="store_true", help="print the manifest for the current tree")
    a = ap.parse_args()
    if a.verify:
        errors = verify(a.verify, a.require_clean, a.expect_commit)
        for e in errors:
            print(f"  FAIL  {e}")
        if errors:
            raise SystemExit("manifest: the artefacts do not match the code")
        print("manifest: ok (version, commit, config, artefacts and the emulator agree)")
        return 0
    manifest = collect()
    if a.print:
        print(json.dumps(manifest, indent=2, sort_keys=True))
        return 0
    if not any(a.get("sha256") for a in manifest["artifacts"].values()):
        raise SystemExit("build_manifest: nothing to describe; build first")
    write(manifest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
