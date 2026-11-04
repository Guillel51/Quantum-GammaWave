#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The README's navigation block, checked as a contract.

The GitHub README is the project's front door, and its first block is a navigation table:
row one carries the five accesses plus the flashing shortcut, row two the licence. Both the
names and their order are requirements (WEB SITE first and double width, WEB EMULATOR and
never "EMULATOR WASM", the licence split into LICENSE and GPL-3.0-ONLY), and every link must
resolve: an in-repo path must exist, a destination on the published site must be one of the
routes web/make_site.py and the Pages workflow build, and any other URL must be one of the
project's known external homes.

  python3 tests/readme_nav_test.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"

SITE = "https://guillel51.github.io/Quantum-GammaWave/"
SITE_ROUTES = {"": "web/site/index.html", "emulator/": "web/emu/index.html",
               "webapp/editor/": "web/editor.html", "webapp/installer/": "web/index_pkg.html"}
EXTERNAL = {"https://github.com/Guillel51/Quantum-GammaWave/releases",
            "https://www.gnu.org/licenses/gpl-3.0-standalone.html"}

ROW1 = ["WEB SITE", "WEB EMULATOR", "WEB STUDIO", "WEBAPP / EDITOR", "USB INSTALLER"]
ROW2 = ["FLASH FM-1", "LICENSE", "GPL-3.0-ONLY"]


def cells(row):
    """[(colspan, label, href)] for one <tr>"""
    out = []
    for td in re.findall(r"<td\b([^>]*)>(.*?)</td>", row, re.S):
        attrs, body = td
        span = re.search(r'colspan\s*=\s*"(\d+)"', attrs)
        img = re.search(r'<img[^>]*alt\s*=\s*"([^"]*)"', body)
        link = re.search(r'<a[^>]*href\s*=\s*"([^"]*)"', body)
        out.append((int(span.group(1)) if span else 1,
                    (img.group(1) if img else re.sub(r"<[^>]+>", "", body)).strip(),
                    link.group(1) if link else ""))
    return out


def main():
    text = README.read_text(encoding="utf-8")
    bad = []

    # the block is the first thing after the title: a table, not a pile of loose badges
    tables = re.findall(r"<table\b.*?</table>", text, re.S)
    if not tables:
        print("no navigation table in README.md")
        return 1
    nav = tables[0]
    rows = re.findall(r"<tr\b.*?</tr>", nav, re.S)
    if len(rows) < 2:
        print("the navigation table needs two rows")
        return 1
    r1, r2 = cells(rows[0]), cells(rows[1])
    labels1 = [c[1] for c in r1]
    labels2 = [c[1] for c in r2]

    if labels1 != ROW1:
        bad.append(f"row 1 is {labels1}, the required order is {ROW1}")
    if labels2 != ROW2:
        bad.append(f"row 2 is {labels2}, the required order is {ROW2}")
    if not r1 or r1[0][1] != "WEB SITE" or r1[0][0] != 2:
        bad.append("WEB SITE must come first and span two columns")
    if sum(c[0] for c in r1) != sum(c[0] for c in r2):
        bad.append("the two rows must span the same number of columns")
    if sum(c[0] for c in r1) < 6:
        bad.append("row 1 must hold the five accesses plus FLASH FM-1 across at least six columns")

    # the retired names must not come back
    for old in ("PROJECT PORTAL", "PROJECT-PORTAL", "EMULATOR WASM", "EMULATOR-WASM",
                "PROJECT-PORTAL-ff5500", "EMULATOR-WASM-ff9424"):
        if old in nav:
            bad.append(f"the old navigation name {old!r} is still in the table")

    # every destination resolves
    for span, label, href in r1 + r2:
        if not href:
            bad.append(f"{label}: no link")
        elif href.startswith(SITE):
            route = href[len(SITE):]
            if route not in SITE_ROUTES:
                bad.append(f"{label}: {href} is not a route the site build produces")
            elif not (ROOT / SITE_ROUTES[route]).is_file():
                bad.append(f"{label}: {SITE_ROUTES[route]} is missing")
        elif re.match(r"^[a-z]+://", href):
            if href not in EXTERNAL:
                bad.append(f"{label}: unexpected external destination {href}")
        elif not (ROOT / href).exists():
            bad.append(f"{label}: {href} does not exist in the repository")

    # the palette the table asks for: the Quantum dark identity, cyan / blue / orange / green
    for colour in ("00e5a3", "3effff", "1f6feb", "ff9424", "ff5500"):
        if colour not in nav:
            bad.append(f"the Quantum accent {colour} is missing from the navigation")

    for b in bad:
        print(f"README nav: {b}")
    print("README navigation: FAILED" if bad else "README navigation: ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
