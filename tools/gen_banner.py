#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""RETIRED (FASE 8). One banner, one generator: `tools/make_banner.py`.

This file used to draw its own `assets/branding/quantum_banner.png` at 1600 x 560, while
`tools/make_banner.py` drew the same path at 2400 x 900 and the three social sizes from the firmware's own
240 x 240 captures. Whoever ran last won, so the repository could ship a hero image that no other artefact
matched: two generators, one file, two layouts.

It is kept only as a door that now leads to the single generator -- any script or note still calling it
gets the same four images as `make_banner.py` instead of a second, different banner:

    python3 tools/gen_banner.py            # == python3 tools/make_banner.py
    python3 tools/gen_banner.py --check    # == python3 tools/make_banner.py --check
"""
import runpy
import sys
from pathlib import Path

TOOL = Path(__file__).with_name("make_banner.py")

if __name__ == "__main__":
    sys.argv[0] = str(TOOL)
    runpy.run_path(str(TOOL), run_name="__main__")
