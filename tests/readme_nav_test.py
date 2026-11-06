#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""The README's navigation block, checked as a contract.

The GitHub README is the project's front door, and its first block is a navigation table in
three rows: WEB SITE alone and dominant on the first (the entry point, the largest box), the
other accesses on the second, the licence filed small at the bottom. The names, their order
and their relative size are requirements (WEB SITE full width and tallest, the emulator called
"Emulator" and never "Emulador", Web Studio and WEBAPP / EDITOR sharing one box, USB INSTALLER
as the only way onto an FM-1 -- the separate FLASH FM-1 access is retired --, LICENSE and GPL 3.0
small and last, and no COPYLEFT anywhere), and every
link must resolve: an in-repo path must exist, a destination on the published site must be one
of the routes web/make_site.py and the Pages workflow build, and any other URL must be one of
the project's known external homes.

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
            "https://github.com/Guillel51/Quantum-GammaWave",
            "https://www.gnu.org/licenses/gpl-3.0-standalone.html"}

ROW1 = ["WEB SITE"]                                      # the entry point, alone and dominant
ROW2 = ["WEB EMULATOR", "WEB STUDIO + WEBAPP / EDITOR", "USB INSTALLER"]
ROW3 = ["LICENSE", "GPL 3.0", "REPOSITORY"]             # small, at the foot


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
    if len(rows) != 3:
        bad.append(f"the navigation table is {len(rows)} rows, the contract is three")
        return 1
    r1, r2, r3 = cells(rows[0]), cells(rows[1]), cells(rows[2])
    labels1 = [c[1] for c in r1]
    labels2 = [c[1] for c in r2]
    labels3 = [c[1] for c in r3]

    if labels1 != ROW1:
        bad.append(f"row 1 is {labels1}, the required order is {ROW1}")
    if labels2 != ROW2:
        bad.append(f"row 2 is {labels2}, the required order is {ROW2}")
    if labels3 != ROW3:
        bad.append(f"row 3 is {labels3}, the required order is {ROW3}")

    # WEB SITE is the entry point: alone on its row, full width, and taller than every other box
    if len(r1) != 1 or r1[0][0] < 4:
        bad.append("WEB SITE must stand alone on the first row, spanning the table's width")
    heights = {}
    for row in (rows[0], rows[1], rows[2]):
        for td in re.findall(r"<td\b([^>]*)>(.*?)</td>", row, re.S):
            h = re.search(r'height\s*=\s*"(\d+)"', td[0]) or re.search(r'height\s*=\s*"(\d+)"', td[1])
            alt = re.search(r'alt\s*=\s*"([^"]*)"', td[1])
            if h and alt:
                heights[alt.group(1)] = int(h.group(1))
    web_site = heights.get("WEB SITE", 0)
    others = [v for k, v in heights.items() if k != "WEB SITE"]
    if web_site < 56 or (others and web_site <= max(others)):
        bad.append(f"WEB SITE must be the tallest box ({web_site} px against {others})")
    for small in ROW3:
        if heights.get(small, 99) > 24:
            bad.append(f"{small} must be small at the foot ({heights.get(small)} px)")

    # the emulator names Emulator, the studio shares the editor's box, the installer sits beside FLASH FM-1
    if any("EMULADOR" in (c[1] + c[2]).upper() for c in r1 + r2 + r3):
        bad.append("the emulator access must say Emulator, never EMULADOR")
    if any(c[1] == "WEB STUDIO" for c in r1 + r2):
        bad.append("WEB STUDIO must share one box with WEBAPP / EDITOR")
    if "FLASH FM-1" in [c[1] for c in r1 + r2 + r3] or "FLASH FM-1" in nav:
        bad.append("FLASH FM-1 is retired: the USB INSTALLER is the only way onto an FM-1")
    if "COPYLEFT" in nav.upper():
        bad.append("the COPYLEFT text is retired from the navigation")

    # the two rows must stay aligned and the licence must not come before the accesses
    spans = {len(r1) and sum(c[0] for c in r1), sum(c[0] for c in r2), sum(c[0] for c in r3)}
    if len(spans) != 1:
        bad.append(f"the three rows must span the same number of columns, they are {sorted(spans)}")

    # the retired names must not come back
    for old in ("PROJECT PORTAL", "PROJECT-PORTAL", "EMULATOR WASM", "EMULATOR-WASM",
                "PROJECT-PORTAL-ff5500", "EMULATOR-WASM-ff9424"):
        if old in nav:
            bad.append(f"the old navigation name {old!r} is still in the table")

    # every destination resolves
    for span, label, href in r1 + r2 + r3:
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
    # (1.9) ff5500 went with the retired FLASH FM-1 badge; the installer keeps ff9424
    for colour in ("00e5a3", "3effff", "1f6feb", "ff9424"):
        if colour not in nav:
            bad.append(f"the Quantum accent {colour} is missing from the navigation")

    for b in bad:
        print(f"README nav: {b}")
    print("README navigation: FAILED" if bad else "README navigation: ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
