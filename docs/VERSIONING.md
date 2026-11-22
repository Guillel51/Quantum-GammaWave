# Versioning

## What the number is

`FELUCCA_VERSION` in `firmware/src/felucca.c` is the single source: the splash, the menu, the console, the
editor, `tools/make_release.py` and every package name below it read that define. Nothing else in the tree
carries a version of its own.

## The 1.7.x series (v1.7.19)

The public numbering reached **v1.12.18** over the 1.x line. The owner asked for it to return to **1.7.x**
without losing anything, so v1.7.19 is:

- **the same tree** as v1.12.18 -- ten engines, three FX buses, five reverb models, DIST's six models (CRUSH
  among them), the twelve LFO shapes, the 293-sound bank, the 203-screen UI -- plus this release's fixes. No
  feature, preset, optimisation or commit was removed to change the number;
- **the patch continues** where the old line stopped: 1.12.18 -> 1.7.19. Both lines share one count, so the
  series has gone on 1.7.19 -> 1.7.20 (the flickers) -> 1.7.21 (the visualizer's frame), each newer than
the last in the same way.

### How ordering works, given that "1.7.19" < "1.12.18" as a string

Nothing in the toolchain compares version strings to decide what is newer:

- `site/firmware/versions.json` is **ordered by publication**, newest first; entry 0 is what the installer and
  the site offer as current, and every entry carries its package's SHA-256, size, commit and manifest;
- a package's manifest (`felucca-<version>.fwsc.manifest.json`, schema `qg1-build-1`) ties the artefact to the
  commit that produced it, so identity never rests on the number alone;
- `tools/fm1_sign.py`'s "downgrade" refusal is about a package's **minimum firmware** requirement, not about
  the package's own version; installing another version of the same model is a separate, explicit decision
  (`tests/preflight_test.py`).

So there is no ambiguity to resolve at install time: the site publishes one current package, the manifest
proves which commit and bytes it is, and the version string is a name, not an ordering key.

### History is preserved

Tags `v1.7.0`, `v1.8.x`, `v1.9.0`, `v1.10`, `v1.12.x` and their releases are untouched. This file adds a
series note; it does not rename, retag, delete or republish anything already published.
