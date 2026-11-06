#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static checks for the official portal.

The portal is one HTML file plus its screenshots. This verifies the parts that
do not need a browser: unique ids, real local assets, the routes the Pages
workflow creates, no placeholder copy, and (when build/site exists) that the
version page's SHA-256 and size are those of the .fwsc actually published.

  python3 web/test_site.py [SITE_DIR]
"""
import hashlib
import json
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "site"
ROUTES = ("emulator/", "webapp/editor/", "webapp/installer/", "firmware/versions.json")
REQUIRED = ("Quantum", "QUANTUM", "FM-1", "versions.json", "SHA-256", "Web Studio", "Instalador", "Web Site",
            "Web Emulator")
# The portal names the browser build "Web Emulator" and keeps the Quantum identity: Solar Heat was the
# original palette and must not come back as the dominant design or as the access name.
FORBIDDEN = ("Emulador WASM", "Solar Heat", "Solar <em>")


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids, self.assets, self.links, self.errors = set(), [], [], []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            if a["id"] in self.ids:
                self.errors.append(f"duplicate id {a['id']}")
            self.ids.add(a["id"])
        if tag == "img":
            if not a.get("src"):
                self.errors.append("img without src")
            else:
                self.assets.append(a["src"])
            if not a.get("alt"):
                self.errors.append(f"img without alt: {a.get('src', '?')}")
        if tag == "a" and a.get("href"):
            self.links.append(a["href"])


def main(site_dir=None):
    src = SOURCE / "index.html"
    page = Page()
    html = src.read_text(encoding="utf-8")
    page.feed(html)
    bad = list(page.errors)
    for asset in page.assets:
        if asset.startswith(("data:", "http")):
            continue
        if not (SOURCE / asset).is_file():
            bad.append(f"missing source asset {asset}")
    text = src.read_text(encoding="utf-8")
    for route in ROUTES:
        if f'href="{route}"' not in text:
            bad.append(f"missing route {route}")
    for word in REQUIRED:
        if word.lower() not in text.lower():
            bad.append(f"missing copy: {word}")
    for word in FORBIDDEN:
        if word.lower() in text.lower():
            bad.append(f"forbidden copy: {word}")
    # Web Site is the primary access and must come before every other one.
    at = text.find('id="accesos"')
    if at < 0:
        bad.append("no access section")
    else:
        order = [text.find(h, at) for h in ('href="./"', 'href="emulator/"', 'href="webapp/editor/"',
                                            'href="webapp/installer/"')]
        if any(i < 0 for i in order):
            bad.append("the access section misses one of the four links")
        elif order != sorted(order):
            bad.append("Web Site is not the first access")
        primary = text[at:order[1]] if order[1] > 0 else ""
        if 'class="access-card primary"' not in primary:
            bad.append("Web Site is not the highlighted access")
    if "location.hash === \"#emu\"" in text:
        bad.append("the portal still redirects a #emu hash to the emulator")
    if "TODO" in text or "lorem ipsum" in text.lower():
        bad.append("placeholder copy")
    if ".reveal { opacity: 0" in text:
        bad.append("reveal hides copy until the scroll observer runs")
    if 'id="ver-sha"' not in text or "versions.json" not in text:
        bad.append("version panel is not wired to versions.json")
    if 'id="lang"' not in text or '<script src="i18n.js"></script>' not in text:
        bad.append("language selector or i18n.js is missing")
    if 'id="lang"' in text and 'localStorage' not in text:
        bad.append("language choice is not persisted")
    for word in ("English", "Español", "中文"):
        if word not in text:
            bad.append(f"language selector misses {word}")

    if site_dir:
        out = Path(site_dir)
        versions = out / "firmware" / "versions.json"
        if not versions.is_file():
            bad.append(f"missing {versions}")
        else:
            listed = json.loads(versions.read_text(encoding="utf-8"))
            if not isinstance(listed, list) or not listed:
                bad.append("versions.json is empty")
            else:
                for entry in listed:
                    pkg = out / "firmware" / entry["file"]
                    raw = pkg.read_bytes() if pkg.is_file() else b""
                    if not raw:
                        bad.append(f"missing package {pkg}")
                    elif hashlib.sha256(raw).hexdigest() != entry["sha256"]:
                        bad.append(f"SHA-256 mismatch for {pkg}")
                    elif len(raw) != entry["size"]:
                        bad.append(f"size mismatch for {pkg}")
        for asset in page.assets:
            if not asset.startswith(("data:", "http")) and not (out / asset).is_file():
                bad.append(f"missing built asset {asset}")
        for href in page.links:
            if href.startswith(("http", "mailto:", "#", "data:")):
                continue
            target = out / href.split("#", 1)[0].split("?", 1)[0]
            if not (target.is_file() or (target.is_dir() and (target / "index.html").is_file())):
                bad.append(f"link goes nowhere in the build: {href}")
        # The portal test above only follows the portal's links. The same depth mistake used to
        # hide in /emulator/ and /webapp/editor/ (their links resolve from a different directory),
        # so verify every local href in each generated page as the browser would. The optional
        # stock FM-1.fwsc is intentionally absent from a build without it and is not fetched.
        built_pages = [out / "index.html", out / "emulator" / "index.html",
                       out / "webapp" / "editor" / "index.html", out / "webapp" / "installer" / "index.html"]
        for built in built_pages:
            if not built.is_file():
                bad.append(f"missing built page {built}")
                continue
            sub = Page()
            sub.feed(built.read_text(encoding="utf-8"))
            for href in sub.links:
                if href.startswith(("http", "mailto:", "#", "data:")) or "FM-1.fwsc" in href:
                    continue
                target = (built.parent / href.split("#", 1)[0].split("?", 1)[0]).resolve()
                if not (target.is_file() or (target.is_dir() and (target / "index.html").is_file())):
                    bad.append(f"link goes nowhere from {built.relative_to(out)}: {href}")
        emu, built = out / "emulator", HERE.parent / "build" / "emu"
        if (emu / "manifest.json").is_file() and (built / "manifest.json").is_file():
            try:
                a = json.loads((emu / "manifest.json").read_text(encoding="utf-8"))
                b = json.loads((built / "manifest.json").read_text(encoding="utf-8"))
            except ValueError as e:
                bad.append(f"bad emulator manifest: {e}")
            else:
                if a != b:
                    bad.append(f"stale emulator copy in {emu}: {a} != {b}")
                if a.get("commit") and f'<code id="build-commit">{a["commit"]}</code>' not in (emu / "index.html").read_text(encoding="utf-8"):
                    bad.append("emulator page does not name its manifest's commit")

    # the access grid: WEB SITE is the dominant box and fills it, Web Studio shares the editor's,
    # USB INSTALLER is the only way in (the FLASH FM-1 entry is gone) and takes both cells it
    # shared with it, and the emulator is never called "Emulador" on this page
    acc = html.split('id="accesos"', 1)[-1].split("</section>", 1)[0]
    if ".access-card.primary { grid-column: 1; grid-row: 1 / span 2; align-content: space-between; }" not in html:
        bad.append("the primary access card does not span both rows and fill them")
    if ".access-card { display: grid; gap: 7px; min-height: 100%;" not in html:
        bad.append("an access card does not fill its box")
    if ".access-card.wide { grid-column: 2 / span 2; grid-row: 2; }" not in html:
        bad.append("USB INSTALLER does not take the two cells it shared with FLASH FM-1")
    for want in ('WEB SITE', 'Web Emulator', 'WEB STUDIO · WEBAPP / EDITOR', 'USB INSTALLER',
                 'Abrir Web Emulator'):
        if want not in acc:
            bad.append(f"the access grid is missing {want!r}")
    for wrong in ('Abrir el emulador', '>Emulador<', 'COPYLEFT', 'FLASH FM-1', 'Flash FM-1'):
        if wrong in acc:
            bad.append(f"the access grid still says {wrong!r}")

    # (7.x) the support box: a plain external link to Mercado Pago, nothing else
    sup = html.split('id="apoyo"', 1)[-1].split("</section>", 1)[0] if 'id="apoyo"' in html else ""
    if 'href="https://link.mercadopago.com.uy/quantumgammawave"' not in sup:
        bad.append("the support section does not link to Mercado Pago")
    if 'target="_blank"' not in sup or "noopener" not in sup:
        bad.append("the support link does not open externally and safely")
    for wrong in ("paypal", "stripe", "<form", "fetch(", "XMLHttpRequest"):
        if wrong in sup.lower():
            bad.append(f"the support section carries {wrong!r}: it must stay a plain link")
    for want in ("Support Quantum GammaWave", "DONATE"):
        if want not in sup:
            bad.append(f"the support section is missing {want!r}")

    for line in bad:
        print(f"site: {line} FAIL")
    print(f"site: {len(page.ids)} ids, {len(page.assets)} images, {len(page.links)} links, "
          f"{len(bad)} FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1] if len(sys.argv) > 1 else None))
