#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  index.html                  the official portal (web/site/index.html + its real screenshots)
  firmware/felucca-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  src/                        not touched

  The emulator is copied to OUT/emulator by the Pages workflow: the portal is the root, and the
  installer / editor keep their existing /webapp/ paths.

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR [PREV_DIR]

PREV_DIR (JIANT 0.6.46): what web/fetch_prev.py downloaded from the live site, its firmware/versions.json and its
packages: up to KEEP_OLD earlier versions (the newest build of each) stay installable next to the current one
(firmware/versions.json, the installer's version list).

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package; the device must report it after
the install.
"""
import base64
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
KEEP_OLD = 5
SAFE = re.compile(r"felucca-[A-Za-z0-9.-]+\.fwsc")


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def base_of(version):
    """v0.6.46-abc1234 -> v0.6.46: the builds of one version"""
    return version.split("-")[0]


def earlier(prev, version, product):
    """[(entry, file)]: PREV_DIR's versions, newest first, one per version, not this one, at most KEEP_OLD, each
    package there and with this device's identity"""
    out, seen = [], {base_of(version)}
    try:
        listed = json.loads((prev / "versions.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return out
    for e in listed if isinstance(listed, list) else []:
        v, f = str(e.get("version", "")), str(e.get("file", ""))
        src = prev / f
        if not SAFE.fullmatch(f) or base_of(v) in seen or not src.is_file():
            continue
        if product_of(src.read_bytes()) != product:
            continue
        seen.add(base_of(v))
        out.append(({"version": v, "file": f}, src))
        if len(out) >= KEEP_OLD:
            break
    return out


def build_record(pkg_path, manifest=None):
    """the qg1-build-1 record of the build that produced this package, if it is here (tools/build_manifest.py)"""
    cands = ([Path(manifest)] if manifest else []) + [Path(pkg_path).parent / "manifest.json",
                                                      Path(str(pkg_path) + ".build.json")]
    for c in cands:
        try:
            m = json.loads(c.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if isinstance(m, dict) and m.get("schema") == "qg1-build-1":
            return m
    return None


def release_manifest(pkg_path):
    """the release manifest tools/fm1_sign.py wrote next to the package, if it is here.

    It is the authenticity half of the chain: an Ed25519 signature over a canonical JSON of the
    manifest, by a key the custodian pinned in tools/fm1_keys.json. The site publishes it beside the
    bytes; a package without one is published with `manifest: null`, and the installer then refuses to
    enable INSTALL -- that is the released state, not an error to work around."""
    import hashlib
    raw = Path(pkg_path).read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    cands = [Path(str(pkg_path) + ".manifest.json"), Path(pkg_path).with_suffix(".manifest.json")]
    # the committed release: its manifest is used only for the exact bytes being served, so a site can
    # never publish a signature that belongs to a different package
    try:
        cands += sorted((HERE.parent / "release").glob("*.manifest.json"))
    except OSError:
        pass
    for cand in cands:
        try:
            m = json.loads(cand.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if (isinstance(m, dict) and m.get("format") == "qg1-release-1"
                and str(m.get("sha256", "")).lower() == digest and int(m.get("size", -1)) == len(raw)):
            return m
    return None


def pinned_keys():
    """only the PUBLIC keys (hex) the custodian pinned: they travel with the page, nothing else does"""
    try:
        doc = json.loads((HERE.parent / "tools" / "fm1_keys.json").read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}
    keys = doc.get("keys") if isinstance(doc, dict) else None
    return {k: v for k, v in (keys or {}).items() if isinstance(v, str)} if isinstance(keys, dict) else {}


def emulator_record(pkg_path, emu_manifest=None):
    """build/emu/manifest.json (qg1-emu-1), when the emulator was built next to this package.

    `emu_manifest` names the emulator the site will actually serve. It matters when the package is a
    committed release: the site copies this run's `build/emu`, so the record in versions.json has to be
    this run's too, or the page describes an emulator that is not on the site."""
    path = Path(emu_manifest) if emu_manifest else Path(pkg_path).parent / "emu" / "manifest.json"
    try:
        m = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    return m if isinstance(m, dict) and m.get("schema") == "qg1-emu-1" else None


def with_csp(html):
    """(QG-1) a strict Content-Security-Policy: only the page's own inline script (by its hash) runs, nothing is
    fetched from elsewhere (the packages are same-origin), no plugins, no <base>, no forms"""
    m = re.search(r'<script type="module">(.*?)</script>', html, re.S)
    if not m or html.count("<script") != 1:
        raise SystemExit("index_pkg.html: expected exactly one inline module script (the CSP hashes it)")
    # Every module is concatenated into the page's single scope, so two top-level declarations with one
    # name are a SyntaxError on load: the page would be blank while every source test still passed.
    declared = re.findall(r"^(?:const|let|var|function|class|async function)\s+([A-Za-z_$][\w$]*)", m.group(1), re.M)
    dup = sorted({d for d in declared if declared.count(d) > 1})
    if dup:
        raise SystemExit(f"index_pkg.html: duplicate top-level declaration(s) {dup} -- the inlined modules "
                         "share one scope and the page would not load")
    h = base64.b64encode(hashlib.sha256(m.group(1).encode("utf-8")).digest()).decode()
    csp = (f"default-src 'none'; script-src 'sha256-{h}'; style-src 'unsafe-inline'; img-src 'self' data: blob:; "
           "connect-src 'self'; font-src 'self' data:; media-src 'self' blob:; object-src 'none'; base-uri 'none'; "
           "form-action 'none'")
    meta = f'<meta http-equiv="Content-Security-Policy" content="{csp}">\n<meta name="referrer" content="no-referrer">\n'
    return html.replace('<meta charset="utf-8">\n', '<meta charset="utf-8">\n' + meta, 1)


def main(pkg, version, out, prev=None, manifest=None, emu_manifest=None):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1diag.js").read_text(encoding="utf-8")) + "\n" + \
    strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
    strip_module((HERE / "fm1accept.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1verify.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1state.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"felucca-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    old = earlier(Path(prev), version, product) if prev else []
    # (QG-1) each package with its SHA-256 and size: the installer refuses bytes that do not match
    def digest(b):
        return {"sha256": hashlib.sha256(b).hexdigest(), "size": len(b)}
    # (v1.9.0) the build record travels with the page (inlined): the verify report shows it
    build = build_record(pkg, manifest)
    emu = emulator_record(pkg, emu_manifest)
    # (M3.4) `emulator` is the emulator this site serves (build/emu/manifest.json of this run), which is
    # not the one the package's build record names when the package is a committed release: the release
    # pins the emulator built with it, the site copies the one built now. Both are true, so both are
    # published, each under its own key, and tools/site_check.py holds each to its own claim.
    rel_manifest = release_manifest(pkg)
    keys = pinned_keys()
    versions = [{"version": version, "file": name, **digest(raw), "build": build, "emulator": emu,
                 "manifest": name + ".manifest.json" if rel_manifest else None}] + \
        [{**e, **digest(src.read_bytes()), "build": None,
          "manifest": e["file"] + ".manifest.json" if release_manifest(src) else None}
         for e, src in old]
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name, **digest(raw),
                       "stock": (HERE.parent / "FM-1.fwsc").is_file(), "build": build, "emu": emu, "keys": keys,
                       "versions": [{"version": e["version"], "pkg": "../../firmware/" + e["file"],
                                     "sha256": e["sha256"], "size": e["size"],
                                     "manifest": ("../../firmware/" + e["file"] + ".manifest.json")
                                     if e.get("manifest") else None,
                                     "build": e.get("build")} for e in versions]})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    html = with_csp(html)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for f in fw.glob("felucca-*.fwsc"):            # the current package and the earlier ones kept, nothing else
        f.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    # (plan) the page that answers "what will M-Flasher write" without pretending to install: it reads
    # the signed release's own versions.json, hashes the package, and runs the acceptance trace and the
    # sector plan on the owner's dump. Same two modules as the installer's card, nothing else.
    plan = (HERE / "plan.html").read_text(encoding="utf-8")
    if plan.count("/*LIB*/") != 1:
        raise SystemExit("plan.html must contain /*LIB*/ once")
    plib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1accept.js").read_text(encoding="utf-8"))
    (out / "plan").mkdir(parents=True, exist_ok=True)
    (out / "plan" / "index.html").write_text(with_csp(plan.replace("/*LIB*/", plib)), encoding="utf-8")
    shutil.copy(pkg, fw / name)
    if rel_manifest:
        (fw / (name + ".manifest.json")).write_text(json.dumps(rel_manifest, indent=1, sort_keys=True) + "\n",
                                                    encoding="utf-8")
    if (HERE.parent / "FM-1.fwsc").is_file():
        shutil.copy(HERE.parent / "FM-1.fwsc", fw / "FM-1.fwsc")
    for e, src in old:
        shutil.copy(src, fw / e["file"])
        m = release_manifest(src)
        if m:
            (fw / (e["file"] + ".manifest.json")).write_text(json.dumps(m, indent=1, sort_keys=True) + "\n",
                                                             encoding="utf-8")
    (fw / "versions.json").write_text(json.dumps(versions, indent=1) + "\n", encoding="utf-8")
    # the same facts in `sha256sum -c` form, beside the bytes: the package, and -- when the package
    # carries its build record -- the app image and the loader it was made from, so a download can be
    # checked against the build it says it is without trusting the page that served it.
    sums = [f"{hashlib.sha256(raw).hexdigest()}  {name}"]
    for art, meta in sorted(((build or {}).get("artifacts") or {}).items()):
        if isinstance(meta, dict) and meta.get("sha256"):
            sums.append(f"{meta['sha256']}  {art}  ({meta.get('bytes', '?')} B, from the build record)")
    (fw / "SHA256SUMS").write_text("\n".join(sums) + "\n", encoding="utf-8")
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Quantum GammaWave (QG-1) licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    # the editor names the build it is too: the token is required, so the page cannot go stale silently
    editor = (HERE / "editor.html").read_text(encoding="utf-8")
    if "{{VERSION}}" not in editor:
        raise SystemExit("web/editor.html: no {{VERSION}} token: the studio would not name the build it serves")
    (ed / "index.html").write_text(editor.replace("{{VERSION}}", version), encoding="utf-8")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js", "preset_categories.json"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    site = HERE / "site"
    if not (site / "index.html").is_file():
        raise SystemExit(f"{site / 'index.html'} is missing")
    # the portal states the version it serves in its own HTML, not only from versions.json: a page
    # whose static label is an old release is a page that lies when its script does not run (and the
    # label read "v1.8.9" for two releases). The token is required, so it cannot silently go stale.
    portal = (site / "index.html").read_text(encoding="utf-8")
    if "{{VERSION}}" not in portal:
        raise SystemExit("web/site/index.html: the portal has no {{VERSION}} token: its static label "
                         "would not name the build this site serves")
    (out / "index.html").write_text(portal.replace("{{VERSION}}", version), encoding="utf-8")
    shutil.copy(site / "i18n.js", out / "i18n.js")
    assets = out / "assets"
    if assets.exists():
        shutil.rmtree(assets)
    shutil.copytree(site / "assets", assets)
    print(f"site: {out}: portal + assets, webapp/installer ({len(html)} B), webapp/editor, "
          f"firmware/{name} ({len(raw)} B, {product}, "
          + (f"build {build['version']} {build['commit'][:7]}" if build else "no build record")
          + ", " + ("emulator record" if emu else "no emulator record") + ")"
          + (f", signed by {rel_manifest['keyid']}" if rel_manifest else ", NO manifest: INSTALL will refuse it")
          + (f", earlier: {' '.join(e['version'] for e, _ in old)}" if old else ""))


if __name__ == "__main__":
    args = sys.argv[1:]
    manifest = emu_manifest = None
    for flag, name in (("--manifest", "manifest"), ("--emu", "emu_manifest")):
        if flag in args:
            i = args.index(flag)
            if i + 1 >= len(args):
                sys.exit(__doc__)
            if name == "manifest":
                manifest = args[i + 1]
            else:
                emu_manifest = args[i + 1]
            del args[i:i + 2]
    if len(args) not in (3, 4):
        sys.exit(__doc__)
    main(*args[:3], args[3] if len(args) == 4 else None, manifest, emu_manifest)
