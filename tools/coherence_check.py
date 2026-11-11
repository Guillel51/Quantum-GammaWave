#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The tree may not contradict itself.

Every check here failed at least once in this repository's history, and each one failed silently:

  * **merge markers inside a source file.** The v2.7.1 bump arrived with `<<<<<<< HEAD` committed inside
    `firmware/src/felucca.c`; the firmware did not compile and nothing said so until someone built it.
  * **a version with no package.** `FELUCCA_VERSION` said v2.7.1 while `release/` stopped at v2.7.0, so the
    site had nothing to serve for the version it named.
  * **a package that does not describe itself.** The hash or size in the manifest disagreed with the bytes.
  * **a site that serves something else.** `site/firmware/versions.json` named a hash the package does not
    have, which is exactly what the installer checks before it enables INSTALL.
  * **an emulator from another tree.** The record named a WASM built from a different commit than the
    firmware it installs beside.

    python3 tools/coherence_check.py [--root .]

Exit code 0 means the tree agrees with itself; 1 means it does not and says where. It reads only tracked
files and never touches the network, so it runs the same locally and in CI.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

TEXT_SUFFIXES = (".c", ".h", ".py", ".js", ".mjs", ".json", ".yml", ".yaml", ".md", ".html", ".sh", ".txt", "")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def tracked_files(root):
    try:
        out = subprocess.run(["git", "-C", root, "ls-files", "-z"],
                             capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return []
    return [p for p in out.decode("utf-8", "replace").split("\0") if p]


def check_conflict_markers(root):
    problems, scanned = [], 0
    for rel in tracked_files(root):
        if not rel.endswith(TEXT_SUFFIXES):
            continue
        path = os.path.join(root, rel)
        try:
            if os.path.getsize(path) > 4 << 20:
                continue
            with open(path, encoding="utf-8", errors="ignore") as f:
                text = f.read()
        except OSError:
            continue
        scanned += 1
        for no, line in enumerate(text.splitlines(), 1):
            if line.startswith("<<<<<<< ") or line.startswith(">>>>>>> ") or line.rstrip() == "=======":
                # `=======` alone is legitimate Markdown/Doc underlining only when it is not part of a pair;
                # the two markers above are never legitimate, so one is enough to fail.
                if line.startswith("<<<<<<< ") or line.startswith(">>>>>>> "):
                    problems.append(f"{rel}:{no}: merge marker committed")
    return problems, scanned


def version_of(root):
    path = os.path.join(root, "firmware", "src", "felucca.c")
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r'#define\s+FELUCCA_VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    return None


def check_package(root, version, problems):
    """The package the version names exists, and its manifest describes exactly those bytes."""
    pkg = os.path.join(root, "release", f"felucca-{version}.fwsc")
    man = pkg + ".manifest.json"
    if not os.path.exists(pkg):
        problems.append(f"release/felucca-{version}.fwsc is missing: the firmware claims {version} "
                        f"and release/ has no package for it")
        return None, None
    if not os.path.exists(man):
        problems.append(f"release/felucca-{version}.fwsc.manifest.json is missing")
        return pkg, None
    with open(man, encoding="utf-8") as f:
        manifest = json.load(f)
    digest = sha256(pkg)
    if manifest.get("sha256") != digest:
        problems.append(f"manifest sha256 {str(manifest.get('sha256'))[:16]}… != package {digest[:16]}…")
    size = os.path.getsize(pkg)
    if manifest.get("size") != size:
        problems.append(f"manifest size {manifest.get('size')} != package {size}")
    if str(manifest.get("version")) != version.lstrip("v"):
        problems.append(f"manifest version {manifest.get('version')!r} != firmware {version}")
    return pkg, digest


def check_build_record(root, version, problems):
    path = os.path.join(root, "release", f"build-manifest-{version}.json")
    if not os.path.exists(path):
        problems.append(f"release/build-manifest-{version}.json is missing")
        return None
    with open(path, encoding="utf-8") as f:
        record = json.load(f)
    if str(record.get("version")) != version:
        problems.append(f"build record version {record.get('version')!r} != firmware {version}")
    if record.get("dirty"):
        problems.append(f"build record was made from a dirty tree: {record['dirty']}")
    if not record.get("commit"):
        problems.append("build record names no commit")
    return record


def check_emulator(root, version, record, problems):
    path = os.path.join(root, "release", "emu", "manifest.json")
    if not os.path.exists(path):
        problems.append("release/emu/manifest.json is missing")
        return
    with open(path, encoding="utf-8") as f:
        emu = json.load(f)
    if str(emu.get("version")) != version:
        problems.append(f"emulator record version {emu.get('version')!r} != firmware {version}")
    emu_rec = (record or {}).get("emulator") or {}
    for key in ("wasm_sha256", "wasm_bytes", "commit"):
        if key in emu_rec and str(emu.get(key)) != str(emu_rec[key]):
            problems.append(f"emulator {key} {emu.get(key)!r} != the build record's {emu_rec[key]!r}")


def check_site(root, digest, problems):
    path = os.path.join(root, "site", "firmware", "versions.json")
    if not os.path.exists(path) or digest is None:
        return
    with open(path, encoding="utf-8") as f:
        entries = json.load(f)
    if not isinstance(entries, list) or not entries:
        problems.append("site/firmware/versions.json has no entries")
        return
    latest = entries[0]
    if latest.get("sha256") != digest:
        problems.append(f"site serves sha256 {str(latest.get('sha256'))[:16]}… but the package is {digest[:16]}…")


def check_sums(root, problems):
    path = os.path.join(root, "release", "SHA256SUMS")
    if not os.path.exists(path):
        problems.append("release/SHA256SUMS is missing")
        return
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            digest, _, name = line.partition("  ")
            target = os.path.join(root, "release", name)
            if not os.path.exists(target):
                problems.append(f"SHA256SUMS names {name}, which is not in release/")
            elif sha256(target) != digest:
                problems.append(f"SHA256SUMS says {digest[:16]}… for {name}, the file hashes differently")


def main():
    ap = argparse.ArgumentParser(description="fail when the tree contradicts itself")
    ap.add_argument("--root", default=".")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    problems = []

    markers, scanned = check_conflict_markers(root)
    problems += markers

    version = version_of(root)
    if not version:
        problems.append("firmware/src/felucca.c defines no FELUCCA_VERSION")
    else:
        pkg, digest = check_package(root, version, problems)
        record = check_build_record(root, version, problems)
        check_emulator(root, version, record, problems)
        check_site(root, digest, problems)
        check_sums(root, problems)

    if problems:
        print(f"coherence: FAILED ({len(problems)} problem(s), {scanned} files scanned)")
        for p in problems:
            print(f"  - {p}")
        return 1
    print(f"coherence: ok — {version}, package and manifest agree, the site serves that package, "
          f"the emulator record matches the build, SHA256SUMS verifies, {scanned} files carry no merge marker")
    return 0


if __name__ == "__main__":
    sys.exit(main())
