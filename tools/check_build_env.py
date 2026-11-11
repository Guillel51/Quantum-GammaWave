#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Report what the reproducible build needs and what this host actually has. It never fails on a
# missing tool: a host without the SDK can still run every host test, and pretending otherwise is
# how a README turns into a green check that proves nothing. Exit status is 1 only with --strict.
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(*cmd):
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    return (p.stdout + p.stderr).strip().splitlines()[0] if p.returncode == 0 and (p.stdout or p.stderr) else None


def main():
    strict = "--strict" in sys.argv[1:]
    # --firmware: the caller builds the firmware only, so emcc is not an input there. The target
    # workflow's firmware job has no emsdk step, and demanding one made that job red for a tool it
    # never uses.
    firmware_only = "--firmware" in sys.argv[1:]
    rows, missing = [], []

    tool = Path(os.environ.get("JIELI_TOOLCHAIN") or Path.home() / ".jieli" / "toolchain")
    clang = tool / "pi32v2" / "bin" / "clang"
    if clang.is_file():
        rows.append(("firmware toolchain", f"{tool}", run(str(clang), "--version") or "clang (no --version)"))
    else:
        rows.append(("firmware toolchain", str(tool), "MISSING: tools/get_toolchain.sh, then export JIELI_TOOLCHAIN"))
        missing.append("firmware toolchain")

    sdk = Path(os.environ.get("AC79_SDK") or Path.home() / "fw-AC79_AIoT_SDK")
    boot = sdk / "cpu" / "wl82" / "tools" / "uboot.boot"
    if boot.is_file():
        rows.append(("AC79 SDK", str(sdk), "uboot.boot present"))
    else:
        rows.append(("AC79 SDK", str(sdk), "MISSING: clone gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK, export AC79_SDK"))
        missing.append("AC79 SDK")

    emcc = shutil.which("emcc")
    if emcc:
        rows.append(("emscripten", emcc, run(emcc, "--version") or "emcc (no --version)"))
    else:
        rows.append(("emscripten", "not on PATH", "MISSING: emsdk install 6.0.11 && source emsdk_env.sh"))
        missing.append("emscripten")

    rows.append(("python", sys.executable, sys.version.split()[0]))
    rows.append(("node", shutil.which("node") or "not on PATH", run("node", "--version") or ""))

    for name, where, detail in rows:
        print(f"{name:<20} {where}\n{'':<20} {detail}")

    pkg = ROOT / "build" / "felucca.fwsc"
    if pkg.is_file():
        print(f"\npackage present: build/felucca.fwsc, {pkg.stat().st_size} B (rebuild it before trusting it)")
    else:
        print("\nno package in build/: nothing has been compiled here yet")

    blocking = [m for m in missing if not (firmware_only and m == "emscripten")]
    if missing:
        print(f"\nmissing for a full build: {', '.join(missing)}")
        if firmware_only and "emscripten" in missing and not blocking:
            print("the firmware build does not need emscripten; only the emulator does")
        print("see docs/BUILD_AC791N.md; host tests do not need these")
    if blocking:
        return 1 if strict else 0
    print("\nall build tools this target needs are present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
