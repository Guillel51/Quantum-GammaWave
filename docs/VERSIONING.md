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

## The 1.8.x series: one patch a phase (2026-11-23)

The owner asked that the number stop jumping: from now on a phase of work is **one patch**, `1.8.N` to
`1.8.N+1`, and the minor only moves when a whole series of features is done. What this cycle shipped that way:

| version | phase | what it is |
|---|---|---|
| v1.8.3 | 1/3 | HOME: the MASTER's numbers and the charging battery (no overlap, 4/5 layout) |
| v1.8.4 | 2/3 | FX: the meter's scale (Q15, not 7000) -- the distortion report |
| v1.8.5 | 3/3 | QUANTUM MORPH: an end from another track, CC 78/79/80, the rail |
| v1.8.6 | extra | the global audit: presets, flicker, idle, lint, polish |

Nothing was renumbered: 1.8.3, 1.8.4 and 1.8.5 are published, tagged and in the site's `versions.json`
list, and **a published package is never rolled back to an older name** (the site offers entry 0, and the
installer reads it by publication order, but a downgrade on a unit is a decision the owner makes, not a
side effect of tidying names). A request to "leave everything at 1.7.4" would have meant exactly that
downgrade; the series went forward instead, one patch at a time.

## The 1.11.x line: the real number, and the one that comes next (2026-11-24)

The tree's single source is `FELUCCA_VERSION` in `firmware/src/felucca.c`. On 2026-11-24 it reads
**v1.11.1**, and that is the only version this repository claims: v1.11.0 is the package the site served
and the release page carried, and FASE 9 is **one patch on top of it**.

What that means in practice:

- **No renumbering and no skipped numbers.** The line is 1.9.4 → 1.10.0 → 1.11.0 → 1.11.1. The `v1.12.x`
  packages and the `v2.x`–`v4.x` tags in `release/`, `site/firmware/` and the tag list are the history of an
  earlier numbering that was later reconciled back to 1.x; they are **published** and stay as they are
  (AGENTS §5, and the rule above: a published package is never renamed or deleted). Nothing new may be
  numbered outside 1.x.x.
- **A package for the version, or no release.** `tools/coherence_check.py` fails when `FELUCCA_VERSION`
  names a version `release/` has no package for. So the number moves only together with a target build,
  and a release is never cut from a guessed artefact.
- **What the number may not carry.** A documentation-only edit that ships no rebuilt package does not need
  a new number; it leaves the tree at the last version whose package exists. `v1.11.1` is a real build:
  the app image, the loader and the emulator were rebuilt from this tree and the package names this
  commit.
