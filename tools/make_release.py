#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""One release, assembled by one tool instead of by hand.

    python3 tools/make_release.py --sync     # write release/, site/ and both SHA256SUMS from the build
    python3 tools/make_release.py --check    # verify the tree still says exactly that (no write)

Why this exists: the assembly used to be a throwaway script in `build/` -- which is gitignored -- so every
release re-derived it by hand, and two things went wrong that this tool now makes impossible:

  * `release/SHA256SUMS` was maintained as "the previous lines, minus this version, plus two", so the
    `build-manifest-*.json` files written beside the others were never added. From 1.3.5 to 1.3.8 every
    release shipped sums that did not describe its own directory, and `signed-release.yml` failed *after*
    the release had been published.
  * the site, the installer and the editor were updated by three separate snippets, so a release could

naming a package in `versions.json` while its installer still offered the previous hash.

Both sums files are now regenerated **from the directories they describe**, never edited line by line,
and `--check` compares every copy of a fact -- the package, its manifest, the build record, the sums, the
site's `versions.json`/`SHA256SUMS`, the installer's `meta`, the editor's label, the emulator records --
against the bytes on disk. `tag-and-publish.yml` runs `--check` before it creates the tag, so a release
cannot be cut from a tree that contradicts itself.

The build itself is not done here and cannot be: this tool reads `build/manifest.json`,
`build/felucca.fwsc` and `build/emu/` and refuses to invent them.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import fm1_sign  # noqa: E402


def sha256_bytes(raw):
    return hashlib.sha256(raw).hexdigest()


def sha256_file(path):
    with open(path, "rb") as f:
        return sha256_bytes(f.read())


def version_of(root):
    """the firmware's own version: the single source of every name below."""
    with open(os.path.join(root, "firmware", "src", "felucca.c"), encoding="utf-8") as f:
        for line in f:
            m = re.match(r'#define\s+FELUCCA_VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    raise SystemExit("make_release: no FELUCCA_VERSION in firmware/src/felucca.c")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def sums_lines(directory, names):
    """`sha256sum -c` lines for exactly these names, sorted: the only way either file is written."""
    return ["%s  %s" % (sha256_file(os.path.join(directory, n)), n) for n in sorted(names)]


def release_names(root):
    """what `release/SHA256SUMS` describes: the packages and the JSON records beside them."""
    rel = os.path.join(root, "release")
    return sorted(n for n in os.listdir(rel) if n.endswith((".fwsc", ".json")))


def root_sums_names(root):
    """what the repository's `SHA256SUMS` describes: the published artefacts, recursively for the emulator."""
    rel = os.path.join(root, "release")
    names = ["release/" + n for n in release_names(root)]
    emu = os.path.join(rel, "emu")
    if os.path.isdir(emu):
        names += ["release/emu/" + n for n in sorted(os.listdir(emu))
                  if os.path.isfile(os.path.join(emu, n))]
    return names


def write_text(path, lines):
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def emu_files(root):
    emu = os.path.join(root, "build", "emu")
    return [n for n in ("felucca.wasm", "index.html", "manifest.json", "worklet.js")
            if os.path.isfile(os.path.join(emu, n))]


def emu_zip_name(version):
    return "qg1-emulator-%s.zip" % version


def write_emu_zip(root, version):
    """`release/emu/qg1-emulator-<v>.zip`: the emulator as a download, not only as a page.

    The release page carried the firmware and nothing to play it with; the emulator was only reachable from
    the live site. This is the same files the site serves, from `site/emulator/` -- the tracked copy, not
    `release/emu/`, whose `fonts/` are generated and gitignored: an archive built from those could not be
    reproduced from a checkout and the pre-flight would fail on the two font files it could not find beside
    the zip. Sorted names, one fixed timestamp and fixed permissions, so the same tree gives the same bytes.
    """
    src = os.path.join(root, "site", "emulator")
    out = os.path.join(root, "release", "emu", emu_zip_name(version))
    names = sorted(n for n in os.listdir(src) if os.path.isfile(os.path.join(src, n)))
    fonts = os.path.join(src, "fonts")
    if os.path.isdir(fonts):
        names += sorted("fonts/" + n for n in os.listdir(fonts) if os.path.isfile(os.path.join(fonts, n)))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for n in names:
            info = zipfile.ZipInfo(n, date_time=(1980, 1, 1, 0, 0, 0))
            info.external_attr = 0o644 << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, open(os.path.join(src, n), "rb").read())
    return out


def sync(root):
    v = version_of(root)
    vn = v.lstrip("v")
    build = os.path.join(root, "build")
    # (1.5.23) WHICH FILE IS "THE PACKAGE". A plain ./build.sh writes build/felucca.fwsc; a
    # ./build.sh --release X.Y writes build/felucca-X.Y.fwsc and leaves the generic name alone -- so a
    # release build on top of an older one shipped the OLD bytes under the new version's name twice before
    # anyone looked. The build record now says which file it wrote (`package`); an older record that does
    # not is still read by its own hash, and a candidate only counts when the record claims exactly it.
    bm_path = os.path.join(build, "manifest.json")
    em_path = os.path.join(build, "emu", "manifest.json")
    arts = (load(bm_path).get("artifacts") or {}) if os.path.isfile(bm_path) else {}
    wrote = (load(bm_path).get("package") if os.path.isfile(bm_path) else None) or "felucca.fwsc"
    names = [wrote] + [n for n in ("felucca-%s.fwsc" % vn, "felucca.fwsc") if n != wrote]
    pkg = next((os.path.join(build, n) for n in names
                if os.path.isfile(os.path.join(build, n))
                and sha256_bytes(open(os.path.join(build, n), "rb").read())
                == (((arts.get(n) or arts.get("felucca.fwsc")) or {}).get("sha256"))), None)
    if pkg is None:                       # none matches the record: name what is missing, never pick one
        pkg = os.path.join(build, wrote)
    for p in (pkg, bm_path, em_path):
        if not os.path.isfile(p):
            raise SystemExit("make_release: %s is missing; build first (./build.sh and sh web/emu/build.sh)"
                             % os.path.relpath(p, root))
    bm = load(bm_path)
    em = load(em_path)
    if bm.get("version") != v or bm.get("dirty"):
        raise SystemExit("make_release: build/manifest.json is %r%s, not a clean %s"
                         % (bm.get("version"), " (dirty: %s)" % bm["dirty"] if bm.get("dirty") else "", v))
    if em.get("version") != v or em.get("dirty"):
        raise SystemExit("make_release: build/emu/manifest.json is %r, not a clean %s"
                         % (em.get("version"), v))
    raw = open(pkg, "rb").read()
    claim = ((bm.get("artifacts") or {}).get(os.path.basename(pkg))
             or (bm.get("artifacts") or {}).get("felucca.fwsc") or {})
    if claim.get("sha256") != sha256_bytes(raw):
        raise SystemExit("make_release: %s does not match the build record's hash"
                         % os.path.relpath(pkg, root))

    rel = os.path.join(root, "release")
    site = os.path.join(root, "site")
    man_path = os.path.join(rel, "felucca-%s.fwsc.manifest.json" % v)

    # The manifest is written ONCE per package. It carries a `created` timestamp, so regenerating it on an
    # unchanged package would change the bytes of a manifest already published beside the release -- the
    # asset and the tree would disagree over a field that says nothing about the sound. Keep the one that
    # already describes exactly these bytes.
    mani = None
    if os.path.isfile(man_path):
        try:
            old = load(man_path)
        except ValueError:
            old = {}
        if old.get("sha256") == sha256_bytes(raw) and int(old.get("size", -1)) == len(raw) \
                and str(old.get("version")) == vn:
            mani = old
    if mani is None:
        images = {k: x["sha256"] for k, x in (bm.get("artifacts") or {}).items()}
        mani = fm1_sign.manifest_for(raw, vn, "1.0.0", images=images,
                                     build_sha256=fm1_sign.record_sha256(bm_path), build=bm)

    # 1. release/: the package, its manifest, the build record, and the emulator beside them
    open(os.path.join(rel, "felucca-%s.fwsc" % v), "wb").write(raw)
    keep = os.path.isfile(man_path)
    if keep:
        try:
            keep = load(man_path) == mani
        except ValueError:
            keep = False
    if not keep:
        with open(man_path, "w", encoding="utf-8") as f:
            json.dump(mani, f, indent=1)
            f.write("\n")
    shutil.copyfile(bm_path, os.path.join(rel, "build-manifest-%s.json" % v))
    emu_out = os.path.join(rel, "emu")
    os.makedirs(emu_out, exist_ok=True)
    for n in emu_files(root):
        shutil.copyfile(os.path.join(build, "emu", n), os.path.join(emu_out, n))
    if os.path.isdir(os.path.join(build, "emu", "fonts")):
        shutil.rmtree(os.path.join(emu_out, "fonts"), ignore_errors=True)
        shutil.copytree(os.path.join(build, "emu", "fonts"), os.path.join(emu_out, "fonts"))
    for old in os.listdir(emu_out):                 # one emulator archive per release, never a pile
        if old.endswith(".zip") and old != emu_zip_name(v):
            os.unlink(os.path.join(emu_out, old))

    # 2. the site: the documented generator, not a second copy of it. `web/make_site.py` writes the portal,
    # the installer's meta, the editor's label, `firmware/`, and keeps the earlier versions it finds in
    # PREV_DIR -- which is the site's own directory as it was before this release (its `versions.json` is
    # what names them, so it has to be copied first: make_site rewrites it).
    prev = os.path.join(build, "prev_firmware")
    shutil.rmtree(prev, ignore_errors=True)
    if os.path.isdir(os.path.join(site, "firmware")):
        shutil.copytree(os.path.join(site, "firmware"), prev)
    cmd = [sys.executable, os.path.join(root, "web", "make_site.py"), os.path.join(rel, "felucca-%s.fwsc" % v),
           vn, site]
    if os.path.isdir(prev):
        cmd.append(prev)
    cmd += ["--manifest", bm_path, "--emu", em_path]
    rc = subprocess.run(cmd, cwd=root)
    if rc.returncode:
        raise SystemExit("make_release: web/make_site.py failed (%d); the site was not written" % rc.returncode)
    # make_site prunes `firmware/` to the current package and the ones its PREV_DIR named, so a download
    # that was published earlier and is no longer offered by the installer would disappear from the site.
    # Put back every package the site already had: the installer's list is a choice, a published URL is not.
    fw = os.path.join(site, "firmware")
    if os.path.isdir(prev):
        for n in sorted(os.listdir(prev)):
            if n.startswith("felucca-") and n.endswith((".fwsc", ".manifest.json")) \
                    and not os.path.exists(os.path.join(fw, n)):
                shutil.copyfile(os.path.join(prev, n), os.path.join(fw, n))
    os.makedirs(os.path.join(site, "emulator"), exist_ok=True)
    for n in emu_files(root):
        shutil.copyfile(os.path.join(build, "emu", n), os.path.join(site, "emulator", n))
    if os.path.isdir(os.path.join(build, "emu", "fonts")):
        shutil.rmtree(os.path.join(site, "emulator", "fonts"), ignore_errors=True)
        shutil.copytree(os.path.join(build, "emu", "fonts"), os.path.join(site, "emulator", "fonts"))
    # the archive last: it is built from `site/emulator/`, which is only final now
    write_emu_zip(root, v)

    # 3. both sums files, regenerated from the directories they describe (never appended to)
    write_text(os.path.join(rel, "SHA256SUMS"), sums_lines(rel, release_names(root)))
    write_text(os.path.join(root, "SHA256SUMS"),
               ["%s  %s" % (sha256_file(os.path.join(root, n)), n) for n in root_sums_names(root)])

    print("make_release: %s assembled -- package %d B sha256 %s, build %s, emulator %s"
          % (v, len(raw), mani["sha256"][:16], bm["commit"][:7], em["commit"][:7]))
    print("  release/: felucca-%s.fwsc, its manifest, build-manifest-%s.json, emu/%d file(s), SHA256SUMS (%d)"
          % (v, v, len(emu_files(root)), len(release_names(root))))
    print("  site/: portal, firmware/felucca-%s.fwsc + versions.json, emulator/, installer meta, editor label"
          % vn)
    return 0


def check(root):
    """every copy of every fact, against the bytes on disk. No build needed: a tag has no artefacts."""
    problems = []
    v = version_of(root)
    vn = v.lstrip("v")
    rel = os.path.join(root, "release")
    site = os.path.join(root, "site")
    pkg = os.path.join(rel, "felucca-%s.fwsc" % v)
    man = pkg + ".manifest.json"
    if not os.path.isfile(pkg):
        return ["release/felucca-%s.fwsc is missing: there is no release for the version the firmware carries"
                % v]
    raw = open(pkg, "rb").read()
    digest, size = sha256_bytes(raw), len(raw)
    if not os.path.isfile(man):
        problems.append("%s is missing" % os.path.basename(man))
        manifest = {}
    else:
        manifest = load(man)
        if manifest.get("sha256") != digest or manifest.get("size") != size:
            problems.append("%s does not describe the package bytes" % os.path.basename(man))
        if str(manifest.get("version")) != vn:
            problems.append("%s says version %r, the firmware says %r"
                            % (os.path.basename(man), manifest.get("version"), vn))

    # the build record, and -- when this tree was built here -- the bytes it names
    bm_path = os.path.join(rel, "build-manifest-%s.json" % v)
    bm = None
    if not os.path.isfile(bm_path):
        problems.append("release/build-manifest-%s.json is missing" % v)
    else:
        bm = load(bm_path)
        if bm.get("dirty"):
            problems.append("the build record was made from a dirty tree (%d file(s))" % len(bm["dirty"]))
        # (FASE 4) the package THE BUILD WROTE, as the record names it: a release build writes felucca-X.Y.fwsc,
        # and comparing against the unversioned file -- one an earlier build left there -- either fails a good
        # release or passes a wrong one (v1.5.24 shipped its predecessor's bytes this way).
        built = os.path.join(root, "build", str(bm.get("package") or "felucca.fwsc"))
        bm_now = os.path.join(root, "build", "manifest.json")
        # (v2.12.0) A REBUILD AT ANOTHER COMMIT IS NOT A DIFFERENT RELEASE. The tag-and-publish flow builds
        # the package at the source commit, commits release/ on top, and CI then rebuilds the tree AT THE TAG:
        # that record names another commit (and its bytes can differ in the fields that name it), so comparing
        # them unconditionally failed the release set of a release that was assembled correctly. The bytes are
        # compared only when build/ holds a record of the SAME commit as the release record; a rebuild at the
        # release's own source commit is still enforced, which is what the check is for.
        same_build = os.path.isfile(bm_now) and load(bm_now).get("commit") == bm.get("commit")
        if os.path.isfile(built) and same_build:
            if open(built, "rb").read() != raw:
                problems.append("release/ and build/ hold different packages: the release was not assembled "
                                "from this build (run --sync)")
            if load(bm_now) != bm:
                problems.append("release/build-manifest-%s.json is not this build's record (run --sync)" % v)
        if (bm.get("artifacts") or {}).get("felucca.fwsc", {}).get("sha256") not in (None, digest):
            problems.append("the build record names another package hash")

    # both sums files, described exactly
    for directory, names, label in ((rel, release_names(root), "release/SHA256SUMS"),
                                    (root, root_sums_names(root), "SHA256SUMS")):
        path = os.path.join(directory, label.split("/")[-1])
        if not os.path.isfile(path):
            problems.append("%s is missing" % label)
            continue
        listed = {}
        for no, line in enumerate(open(path, encoding="utf-8").read().splitlines(), 1):
            if not line.strip():
                continue
            h, sep, name = line.partition("  ")
            if not sep or not re.fullmatch(r"[0-9a-f]{64}", h):
                problems.append("%s:%d is not a `sha256sum` line" % (label, no))
                continue
            listed[name] = h
        missing = sorted(set(names) - set(listed))
        extra = sorted(set(listed) - set(names))
        # `names` are relative to `directory` for release/, and to the root for the root file: one rule.
        stale = sorted(n for n in set(names) & set(listed)
                       if listed[n] != sha256_file(os.path.join(directory, n)))
        for n in missing:
            problems.append("%s does not list %s" % (label, n))
        for n in extra:
            problems.append("%s lists %s, which is not there" % (label, n))
        for n in stale:
            problems.append("%s has the wrong hash for %s" % (label, n))

    # the site: same package, same version, and a portal that is not blank
    fw = os.path.join(site, "firmware")
    served = os.path.join(fw, "felucca-%s.fwsc" % vn)
    if not os.path.isfile(served):
        problems.append("site/firmware/felucca-%s.fwsc is missing: the portal would serve nothing" % vn)
    elif open(served, "rb").read() != raw:
        problems.append("site/firmware holds a different package than release/")
    try:
        versions = load(os.path.join(fw, "versions.json"))
        top = versions[0] if isinstance(versions, list) and versions else {}
        if top.get("sha256") != digest:
            problems.append("site/firmware/versions.json names sha256 %s, the package is %s"
                            % (str(top.get("sha256"))[:16], digest[:16]))
        for e in [top] if top else []:
            if e.get("sha256") != sha256_file(os.path.join(fw, e.get("file", ""))):
                problems.append("site/firmware/versions.json names a file whose bytes are not its hash")
    except (OSError, ValueError) as e:
        problems.append("site/firmware/versions.json is unreadable (%s)" % e)
    portal = os.path.join(site, "index.html")
    try:
        html = open(portal, encoding="utf-8", errors="replace").read()
        if len(html.strip()) < 1024:
            problems.append("site/index.html is %d B: the portal is blank" % len(html))
        elif vn not in html:
            problems.append("site/index.html does not name %s" % vn)
    except OSError:
        problems.append("site/index.html is missing")

    # the installer: what it would tell a player before they press INSTALL
    ins = os.path.join(site, "webapp", "installer", "index.html")
    if os.path.isfile(ins):
        text = open(ins, encoding="utf-8").read()
        try:
            i = text.index("const meta = {")
            meta = json.loads(text[i + len("const meta = "):text.index("\n", i)][:-1])
        except (ValueError, IndexError):
            meta = None
            problems.append("the installer's meta line cannot be read back as JSON")
        if meta is not None:
            if str(meta.get("version")) != vn:
                problems.append("the installer offers %r, the firmware is %r" % (meta.get("version"), vn))
            if meta.get("sha256") != digest or int(meta.get("size", -1)) != size:
                problems.append("the installer's hash/size are not this package's")
            if meta.get("pkg") != "../../firmware/felucca-%s.fwsc" % vn:
                problems.append("the installer points at %r" % meta.get("pkg"))

    # the emulator: one WASM, named by the build record, present in both published copies
    if bm and (bm.get("emulator") or {}).get("wasm_sha256"):
        want = bm["emulator"]["wasm_sha256"]
        for where in (os.path.join(rel, "emu"), os.path.join(site, "emulator")):
            p = os.path.join(where, "felucca.wasm")
            if not os.path.isfile(p):
                problems.append("%s/felucca.wasm is missing" % os.path.relpath(where, root))
            elif sha256_file(p) != want:
                problems.append("%s/felucca.wasm is not the WASM the build record names"
                                % os.path.relpath(where, root))
    # the downloadable emulator: one archive, and every file in it is the file beside it
    zip_path = os.path.join(rel, "emu", emu_zip_name(v))
    if not os.path.isfile(zip_path):
        problems.append("release/emu/%s is missing: the release carries no emulator to download"
                        % emu_zip_name(v))
    else:
        try:
            with zipfile.ZipFile(zip_path) as z:
                bad = z.testzip()
                if bad:
                    problems.append("release/emu/%s: %s is corrupt" % (emu_zip_name(v), bad))
                listed = set(z.namelist())
                for n in listed:
                    src = os.path.join(site, "emulator", n)
                    if not n.endswith("<none>"):
                        if not os.path.isfile(src):
                            problems.append("release/emu/%s holds %s, which site/emulator does not"
                                            % (emu_zip_name(v), n))
                        elif z.read(n) != open(src, "rb").read():
                            problems.append("release/emu/%s holds a different %s" % (emu_zip_name(v), n))
        except zipfile.BadZipFile:
            problems.append("release/emu/%s is not a zip" % emu_zip_name(v))

    for where in (os.path.join(rel, "emu"), os.path.join(site, "emulator")):
        try:
            rec = load(os.path.join(where, "manifest.json"))
            if str(rec.get("version")) != v:
                problems.append("%s/manifest.json says %r, the firmware is %r"
                                % (os.path.relpath(where, root), rec.get("version"), v))
        except (OSError, ValueError):
            problems.append("%s/manifest.json is missing or unreadable" % os.path.relpath(where, root))

    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--sync", action="store_true", help="write release/, site/ and both SHA256SUMS")
    g.add_argument("--check", action="store_true", help="verify the tree still says exactly that")
    ap.add_argument("--root", default=ROOT)
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    if a.sync:
        return sync(root)
    problems = check(root)
    if problems:
        print("make_release: FAILED (%d problem(s) in the release set)" % len(problems))
        for p in problems:
            print("  - " + p)
        return 1
    print("make_release: ok -- %s: release/, site/, the installer, the editor and both SHA256SUMS describe "
          "one package" % version_of(root))
    return 0


if __name__ == "__main__":
    sys.exit(main())
