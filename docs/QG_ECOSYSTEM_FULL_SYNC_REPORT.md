# QG ecosystem full sync report — v2.9.4

One reference version for everything that ships, and a table that says exactly what was done to each
component and how it was checked. Nothing here is marked verified that was not verified.

## Reference

| | |
| --- | --- |
| Version | **v2.9.4** (`firmware/src/felucca.c`, `FELUCCA_VERSION`) |
| Source commit built | `8fe5dee`, the sync commit itself (clean tree) |
| AC791N build | `./build.sh`, identity **FM-1_965** |
| Package | `release/felucca-v2.9.4.fwsc`, **610 959 B**, sha256 `c800055f…` |
| Image | **568 488 B** of the 583 612 B slot — **15 124 B** free |
| Tables | **41 947 B** (`tools/table_sizes.py`, out of the ELF) |
| RAM | `.data+.bss` **87 748 B** of 98 304; pool spare **15 392 B** |
| WASM | **1 191 701 B**, sha256 `067f9a07…` |
| Emulator manifest | `build/emu/manifest.json`, emulator record inside the site's firmware manifest |

The version moved to **v2.9.4** because the bytes changed: v2.9.3 is already published and its package
must keep the hash it was published with. A new build that calls itself v2.9.3 would break
"same version, same bytes".

## What changed in the firmware

HOME went from ten screens to five: **MAIN, VISUALIZER, OVERVIEW, MIX, PERFORM**. `HV_COUNT` is 5, so
`HV_ENG`, `HV_MOD`, `HV_DYN`, `HV_SYSTEM` and `HV_ROUT` no longer exist as indices. The title strip is
three cells (the three pages that draw it) and the footer prints `HOME n/HV_COUNT`.

Nothing was removed: the engine page's eight parameters are EDIT's first page, the matrix is MOD, the
envelope is EDIT's ENV page, the routing is FX > BUSES and the system page is the SYSTEM section.

The image fell from 573 664 B to **568 488 B** (−5 176 B) because the five retired views' drawing code is
unreachable and `--gc-sections` drops it. That is the first time in this series a UI change *gave back*
flash instead of taking it.

## Matrix

| Component | Version shown | Origin | Artefact | State |
| --- | --- | --- | --- | --- |
| Firmware source | v2.9.4 | `8fe5dee` | `firmware/src/` | **SINCRONIZADO** |
| AC791N build | v2.9.4 | this workspace | `build/felucca.bin` 568 488 B | **IMPLEMENTADO LOCALMENTE** (compiled, not run on a unit) |
| Package `.fwsc` | v2.9.4 | this workspace | `release/felucca-v2.9.4.fwsc` 610 959 B | **IMPLEMENTADO LOCALMENTE** |
| Package manifest | v2.9.4 | `tools/fm1_sign.py request` | `release/felucca-v2.9.4.fwsc.manifest.json` | **SINCRONIZADO** (unsigned by design, `keyid ""`) |
| WASM / emulator | v2.9.4 | `web/emu/build.sh` | 1 191 701 B, sha256 `067f9a07…` | **IMPLEMENTADO LOCALMENTE**, **no** opened in a browser here |
| Site (`site/`) | v2.9.4 | `web/make_site.py` | portal + installer + editor + `firmware/felucca-2.9.4.fwsc` | **IMPLEMENTADO LOCALMENTE**, not served |
| Installer | v2.9.4 | `web/site/installer` via make_site | 250 647 B self-contained page | **IMPLEMENTADO LOCALMENTE**; gates unchanged, see below |
| Web Studio | v2.9.4 | `web/site/editor` | editor page | **SINCRONIZADO** (it reads the firmware's SysEx ids; no new ids in this build) |
| Screenshots | v2.9.4 | `tools/generate_screenshots.py` | `assets/screenshots/` 37 files | **SINCRONIZADO** (36 regenerated from the 186-screen sweep, 3 retired removed) |
| Site pictures | v2.9.4 | `tools/fw_shots.py --render` | `web/site/assets/fw-*.png`, `site/assets/` | **SINCRONIZADO** |
| README | v2.9.4 | this commit | `README.md` | **SINCRONIZADO** |
| CHANGELOG | v2.9.4 | this commit | `CHANGELOG.md` | **SINCRONIZADO** |
| Guides / design docs | v2.9.4 | this commit | `docs/` | **SINCRONIZADO** (see the corrections below) |
| GitHub Release | v2.9.4 | tag workflow on push | — | **PENDIENTE** (no `gh` token here; the workflow publishes on push, its result is not observable from this workspace) |
| Online site / emulator | v2.9.4 | Pages workflow | — | **PENDIENTE** (not opened in a browser here) |

States are not interchangeable: *sincronizado* means the file names this version and no other;
*implementado localmente* means built here; *pendiente* means it needs network or a device.

## What was actually run

| Check | Result |
| --- | --- |
| `tools/generate_screenshots.py` (renders the firmware's own drawing code) | 186 screens, 22 palettes, 0 lint findings, 3 152 530 alignment measurements, 0 misalignments |
| `build/host/regress` | 285 golden renders, **0 changed**, 0 health failures, 0 CPU entries over budget |
| `build/host/ui_test` | all passed, incl. the five-screen HOME walk |
| `./build.sh` (AC791N) | **ok**: image 568 488 B, RAM 87 748/98 304, pool spare 15 392, package 610 959 B |
| `web/emu/build.sh` | **ok**: WASM 1 191 701 B |
| `node web/test_web.mjs` | pass |
| `node web/test_backup.mjs` | pass |
| `python3 tests/install_test.py` | pass |
| `python3 web/make_site.py … --no-release` | built, installer 250 647 B, package + manifest + emulator record |

Not run, and therefore not claimed: any browser session against the built emulator or the published
site, any AC791N run on hardware, the tag workflow's own result.

## Corrections made while auditing

* README said **"Ten HOME screens"** and described HOME as one page with the system, the spectrum and the
  tracks; both now describe the five screens.
* README said `v2.9.3` was *a candidate* and that the last published package was `v2.9.2`. v2.9.3 was
  published by the tag workflow on 2026-11-13; the text now says so.
* The site (portal and built copy) captioned a picture **"HOME 9/9 — RUTEO"**, a screen that no longer
  exists. The picture is now the real **FX > BUSES** page and the caption says that, with the MOD page
  beside it instead of the retired HOME matrix view.
* `tools/fw_shots.py` pointed at `quantum_33_home_mod_matrix.png` and `quantum_36_home_routing.png`; it
  now renders `GAMMA_fx_buses.ppm` and `GAMMA_lfo_dest.ppm` from the sections that own those controls.
* `quantum_32_home_engine.png`, `quantum_33_home_mod_matrix.png`, `quantum_36_home_routing.png` deleted
  with the views they showed — no page referenced them any more.
* `firmware/src/felucca.c.bak` deleted: a stale copy of a source file, tracked by accident.
* `docs/FINAL_RELEASE_SYNC.md` and `docs/QG_SCL_FX_REDESIGN.md` still described HOME 9/9; the first is
  corrected here, the second is a design record of the phase that built it and keeps its wording.

## Known limitations, unchanged by this sync

* **The installer gate is still closed.** `docs/VALIDATION_MATRIX.md` requires a reproducible AC791N
  build, a real JEDEC read, a physical backup and a demonstrated recovery. The `uboot.boot` in this
  package is a *different compilation* from the one in v2.9.0 (14 444 B vs 14 536 B, see
  `tools/spl_diff.py` and `docs/QG_RELEASE_BLOCKERS.md`), so reproducibility is not demonstrated and
  INSTALL stays gated on the published site. The package is integrity-checked (SHA-256 + size), not
  signed.
* **No hardware verification.** Nothing in this report was measured on an FM-1.
* **The boot splash's freeze** is diagnosed, not fixed: the clock is fine (TIMER4 from the first pixel and
  176 of 1125 four-millisecond steps change the picture); the repaints are tied to init stage boundaries.
  Fixing it means interleaving the boot stages with the paint loop, which must be validated on a unit.
* **SCL, per-engine EDIT/LFO and the Solar Heat sweep** of the non-HOME screens are not implemented;
  see `docs/QG_UI_AND_SOUND_DESIGN_REFINEMENT.md`.

## Publishing

Sources, docs, captures and the rebuilt site are committed together. The tag workflow publishes on push
when `FELUCCA_VERSION` has no tag yet, which is the repository's own behaviour; this workspace cannot read
the workflow's result (no `gh` token), so the Release and the online site are **PENDIENTE** until someone
opens them. The installer stays disabled for v2.9.4 for the same reason it was for v2.9.3.
