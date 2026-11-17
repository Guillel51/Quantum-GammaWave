# Release process — QG-1

A release is one package, one tree and one set of facts about it. This file says how it is made, what has
to agree before a tag exists, and which workflow proves which part. Classes of evidence are named the same
way `AGENTS.md` names them: a build is not a test, and a test is not a device.

## 1. The version

`FELUCCA_VERSION` in `firmware/src/felucca.c` is the only version string in the project. The package's
manifest, `site/firmware/versions.json`, the installer's `meta`, the editor's label, the emulator's record
and the tag all derive from it — nothing else is edited by hand.

## 2. The build (AC791N + WASM)

```bash
export JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_IoT_SDK
./build.sh                                  # app, loader, package; the memory budgets run inside
sh web/emu/build.sh                         # Emscripten 6.0.11 -> build/emu/
```

`build/manifest.json` records the commit, the toolchain, the SDK files and the hash of every artefact. A
release build is refused from a dirty tree: `dirty: []` is part of what the manifest asserts.

## 3. The release set

```bash
python3 tools/make_release.py --sync
python3 tools/make_release.py --check       # what CI runs before tagging
```

`--sync` writes, from the build alone: `release/felucca-<v>.fwsc`, its manifest, `build-manifest-<v>.json`
and `release/emu/`; then the site through `web/make_site.py` (portal, installer `meta`, editor label,
`firmware/`, `plan/`, and the earlier versions the site already served); then **both** `SHA256SUMS` files,
regenerated from the directories they describe. A manifest already published beside a package is never
rewritten for the same bytes — it carries a `created` timestamp, and the release asset and the tree must
stay the same file.

`--check` re-derives every fact from disk and compares: package bytes and hashes, the build record, both
sums files (complete and correct, in both directions), `site/firmware/versions.json`, the portal (not
blank, names this version), the installer's `meta`, and the emulator records in `release/emu/` and
`site/emulator/`. It needs no build, so it runs on a tag checkout too.

## 4. Publish

```bash
git commit -am "vX.Y.Z: ..."
git push origin main
```

On that push:

1. **`tag-and-publish.yml`** runs the pre-flight (`coherence_check`, `make_release.py --check`,
   `test_site.py site`), verifies the package against its manifest, creates the annotated tag **once**,
   writes the release notes from this version's `CHANGELOG.md` section, attests the package, creates the
   GitHub release (`gh release create --verify-tag`) with the package, its manifest, the build record and
   `SHA256SUMS`, and then dispatches `release.yml` for that tag.
   Re-running it is safe: an existing tag is left alone, an existing release gets its notes refreshed, and
   of its assets only the two **derived** ones (`SHA256SUMS`, `build-manifest-<v>.json`) are replaced, and
   only when their digest differs from the tree's. The package and its manifest are never swapped.
2. **`release.yml`** builds `vX.Y.Z` from its tag with the shared `zvenson/fm1-build` workflow (the
   attestation that gives the Hub its "Built from source on GitHub" badge) and its `verify` job downloads
   the release's package, compares it byte for byte with the committed one and runs
   `gh attestation verify` against the signer workflow.
3. **`pages.yml` / `emulator.yml`** publish the site; **`signed-release.yml`** verifies every package in
   `release/` and that `release/SHA256SUMS` describes exactly that directory; **`target.yml`** rebuilds
   twice and compares the bytes (reproducibility) and **`coherence.yml`** checks the tree against itself.

### What the site keeps

`site/firmware/` holds the current package and the two before it — the installer's list, plus the URLs that
were already published and are still likely to be linked. Older packages stay on their release pages; there
is no reason for the repository to carry five 600 KB copies of the same firmware forever.

## 5. What a release does not prove

Nothing in this pipeline reads or writes a physical FM-1. A green release means the tree is consistent,
the package is reproducible from it and its bytes are what were published — not that the firmware runs on
the hardware. See `docs/VALIDATION_MATRIX.md` for the state of each requirement.

## 6. If something is already published and wrong

The tag is immutable and the package is never replaced. Fix the tree, and let the derived files catch up:
`python3 tools/make_release.py --sync` rewrites `SHA256SUMS` and the build record, and the next run of
`tag-and-publish.yml` replaces those two assets on the existing release. Anything that would need the
package itself to change is a new version.
