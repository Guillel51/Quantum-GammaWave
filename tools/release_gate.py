#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Barrera de publicación para Quantum GammaWave 3.x.

La regla del plan 3.0 es que la versión no se asigna, etiqueta ni publica hasta completar las
fases y las pruebas físicas. El repositorio tenía dos caminos que podían saltarse esa regla:
`tag-and-publish.yml` creaba el tag y el release en cada push a `main`, y un tag manual llegaba
a `release.yml`. Esta barrera es el único punto que ambos consultan.

    python3 tools/release_gate.py v2.9.7                 # permitido
    python3 tools/release_gate.py v3.0.0                 # bloqueado
    QG_RELEASE_APPROVED=true python3 tools/release_gate.py v3.0.0

La aprobación es una variable de repositorio, no una constante en el código: `QG_RELEASE_APPROVED`
con el valor `true` (también `1` o `yes`). Sin ella, una versión 3.x no se publica. Las versiones
2.x siguen su camino actual para no romper releases anteriores.
"""
import argparse
import os
import re
import sys

APPROVED = ("true", "1", "yes")


def version_parts(version):
    """(major, minor, patch) de `v3.0.0`, `3.0.0` o `v3.0.0-rc.1`; None si no es una versión."""
    m = re.match(r"^v?(\d+)\.(\d+)\.(\d+)(?:[-+].*)?$", version or "")
    if not m:
        return None
    return tuple(int(x) for x in m.groups())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("version", help="la versión que el árbol lleva, p. ej. v3.0.0")
    ap.add_argument("approved", nargs="?", default=os.environ.get("QG_RELEASE_APPROVED", "false"),
                    help="QG_RELEASE_APPROVED: true/1/yes para autorizar una 3.x")
    a = ap.parse_args()

    parts = version_parts(a.version)
    if parts is None:
        print(f"release gate: '{a.version}' no es una versión vX.Y.Z; no se publica", file=sys.stderr)
        return 1

    major = parts[0]
    if major < 3:
        print(f"release gate: {a.version} es anterior a 3.0; se mantiene el camino existente")
        return 0

    if str(a.approved).strip().lower() in APPROVED:
        print(f"release gate: {a.version} autorizada por QG_RELEASE_APPROVED")
        return 0

    print(
        f"release gate: {a.version} es 3.0 o posterior y QG_RELEASE_APPROVED no está en "
        f"{'/'.join(APPROVED)}. No se crea tag ni release: completa las fases, las pruebas de "
        "target/WASM/navegador y las pruebas físicas, y autoriza la publicación de forma explícita.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
