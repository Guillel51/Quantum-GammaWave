# Emulator synchronisation: what was wrong, what serves what now

M3.4, 2026-11-09. Every hash below was read over HTTPS from the public origin, and the commit from the
build log of the run that produced it. No local server and no browser screenshot is used as evidence.

## The state it was in

    origin (/, install) : package felucca-v1.9.0-7eb9636.fwsc (signed, 610 806 B,
                          sha256 80ad995c…), emulator 7eb9636f58f4, wasm 1 073 325 B,
                          sha256 c83c8bfb…
    /preview/           : did not exist (HTTP 404)

So the public emulator was the **release's** build, frozen at the release commit, and everything main
had done since had no public face at all: LATCH, punch-in, the splash, HOME, the planets, RESO, CLOUD,
delay, the FX screens. The site was not failing to update — it was publishing the release beside the
release's emulator, on purpose.

## Why the deploy job was red anyway

`ci/log/<run>` (the branch the workflow pushes its own log to) for run 42565730642 at `363011e`:

    the emulator is the build this run copied up                       ok
    the emulator names the same version                               ok
    versions.json's emulator record is the emulator's wasm            FAIL  record '3d039f75bbc8'/1073325

`tools/site_check.py` read the emulator record out of the **package's build record**. That record names
the emulator built with the package (`3d039f75`, the release's). The site served a different emulator, so
the comparison could never hold while the site was a release package plus a newer emulator — the site was
correct and the gate was impossible. It reported `FAIL` twenty times and then `::error::the deployed site
still does not serve the expected artefacts after 5 minutes`.

## The fix (two commits)

1. `e3797ef` — `versions.json` now publishes **both** facts, each under its own key: `build` (the
   package's provenance) and `emulator` (the emulator served beside it, from `build/emu/manifest.json`).
   `site_check.py` holds each to its own claim; a version that differs between main's emulator and the
   release package is named, not failed. The deploy condition lost the two side-branch names, so only
   `main` can publish the public site (a side branch deploying is how the site went *back* to
   `v1.9.0-7eb9636` with no push to main in between).
2. `1318df8` — the workflow already built `build/site_preview` (main's sources, main's emulator, the
   portal, the installer) and then copied it into the emulator artifact where nobody could reach it. It
   is published at `/preview/`, and the deploy job holds `/preview/emulator/manifest.json` to the run's
   commit, so the channel cannot go stale unnoticed.

Validated on the host before pushing: `make_site.py` + `site_check.py` over a local replica of the exact
state (release package 610 806 B beside an emulator from another commit) passes all 14 lines with the new
logic and fails with the old one.

## What the public origin serves now (read back after the deploy)

    run 42635269246 (success) at 1318df8
      deploy job      : completed / success  — "site: all checks passed"
      build log       : emu: packaged 1 079 658 B wasm, v1.9.0, build e3797ef8401a,
                        sha256 8c21befd14035732…

    /                     : package v1.9.0-7eb9636 (signed release), emulator 7eb9636f58f4,
                            wasm 1 073 325 B, sha256 c83c8bfb…     (unchanged, by design)
    /preview/             : HTTP 200
    /preview/emulator/manifest.json : commit 1318df8322d1, version v1.9.0,
                            wasm 1 079 658 B, sha256 8c21befd14035732…
    /preview/emulator/felucca.wasm  : downloaded, 1 079 658 B,
                            sha256 8c21befd1403573226e548487333d934c029a302d75f55de9983522a253bd055

The published bytes are the manifest's bytes and the build log's bytes: one artefact, three independent
readings, same hash. That is the proof the emulator is current; a page that loads is not.

## The two channels, and why they are two (3.3: the root is now the current tree)

M3.4 put main's build at `/preview/` and left the root on the release. That made the new work reachable but
kept the public emulator — the one the portal's button opens — a release behind the repository, which is what
the owner was looking at. 3.3 inverts the two: the root is this tree, and the release moves to its own path.

- `/` — this tree. The emulator, the package and the build record come from the run's commit, and the
  installer publishes that package while refusing to install it (it has no signature: the private key is not
  in CI, so this is fail-closed, not a setting). The footer names the version, the commit, the build date and
  the wasm's SHA-256, and the page will not start if the file it downloaded hashes to something else.
- `/release/` — the signed release, `v1.9.0`. Built from the release commit inside a worktree: portal,
  installer, package and the emulator built *with that package*, so the installer's provenance check holds on
  the page it is served with. This is the only package the installer will write.
- `/preview/` — retired. It carried the same build the root now serves, and two copies of one build is the
  inconsistency this pass exists to end.

The deploy job holds each channel to its own commit: the root to `$GITHUB_SHA` (package and emulator), and
`/release/` to the release record's commit and the signed package's SHA-256.

## Reproduce

    python3 tools/site_check.py --url https://guillel51.github.io/Quantum-GammaWave \
        --expect-commit 7eb9636f58f442ccbfb8440dec0aa61e55203889 \
        --expect-emu-commit 7eb9636 --expect-sha256 80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2
    curl -s https://guillel51.github.io/Quantum-GammaWave/preview/emulator/manifest.json
    curl -s https://guillel51.github.io/Quantum-GammaWave/preview/emulator/felucca.wasm | sha256sum

## Still open

- The **signed release has not been rebuilt** from current main: `/` still offers `v1.9.0-7eb9636`, and
  the new work is not installable, only playable in `/preview/`. Rebuilding it needs the release key and
  the gates in `docs/VALIDATION_MATRIX.md`; nothing here touches the owner's unit.
- `docs/VALIDATION_MATRIX.md` and the installer gates are unchanged by this pass.
