#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Pruebas de la barrera de publicación 3.x (tools/release_gate.py)."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GATE = ROOT / "tools" / "release_gate.py"
fail = 0


def run(version, approved=None, env_approved=None):
    cmd = [sys.executable, str(GATE), version]
    if approved is not None:
        cmd.append(approved)
    env = None
    if env_approved is not None:
        env = {"QG_RELEASE_APPROVED": env_approved}
    p = subprocess.run(cmd, capture_output=True, text=True, env=env)
    return p.returncode, p.stdout + p.stderr


def check(name, cond, detail=""):
    global fail
    print(f"{name:62s} {'ok' if cond else 'FAIL'} {detail}")
    if not cond:
        fail += 1


rc, out = run("v2.9.7")
check("2.9.7 no necesita aprobación", rc == 0, out.strip())
rc, out = run("v3.0.0")
check("3.0.0 sin aprobación se bloquea", rc == 1 and "QG_RELEASE_APPROVED" in out, out.strip())
rc, out = run("v3.0.0", "true")
check("3.0.0 con true se autoriza", rc == 0, out.strip())
rc, out = run("3.0.0-rc.1", "1")
check("3.0.0-rc.1 con 1 se autoriza", rc == 0, out.strip())
rc, out = run("v3.0.0", "false")
check("3.0.0 con false se bloquea", rc == 1, out.strip())
rc, out = run("v3.0.0", env_approved="true")
check("3.0.0 con la variable de entorno true se autoriza", rc == 0, out.strip())
rc, out = run("v10.2.0")
check("v10.2.0 también exige aprobación", rc == 1, out.strip())
rc, out = run("banana")
check("una versión malformada no se publica", rc == 1, out.strip())

print()
if fail:
    print(f"release gate: {fail} check(s) failed")
    sys.exit(1)
print("release gate: passed")
