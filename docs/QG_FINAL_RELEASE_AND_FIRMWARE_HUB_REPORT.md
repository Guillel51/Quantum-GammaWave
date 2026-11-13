# Final release and The Firmware Hub — report

Version **v2.9.4**. Every state below is one of the terms in the brief and nothing stronger.

## 1. The version, the commit, the tag

| | |
| --- | --- |
| Version | **v2.9.4** (`FELUCCA_VERSION`) |
| Commit | the commit that carries this file (see `git log -1`) |
| Tag | **`v2.9.4` exists on the remote** — the tag workflow created it on push |
| Package | `release/felucca-v2.9.4.fwsc`, **610 959 B**, sha256 `c800055f1e35b436…` |
| Manifest | `release/felucca-v2.9.4.fwsc.manifest.json`, verified by `tools/fm1_sign.py verify` |
| Build record | `release/build-manifest-v2.9.4.json` (commit, identity, SDK and toolchain hashes, artefact hashes) |
| Image | **568 488 B** of the 583 612 B slot — 15 124 B free |
| RAM | `.data+.bss` 87 748 B of 98 304; pool spare 15 392 B |
| WASM | **1 191 701 B**, sha256 `067f9a07…`, build recorded in `release/emu/manifest.json` |
| Identity | `FM-1_965` (`FELUCCA_ID`), package product `FM-1`, loader USB identity `ota-FM-1_015` |

### Why no GitHub Release existed for v2.9.1–v2.9.4, and what was wrong

The tag workflow (`tag-and-publish.yml`) creates the annotated tag, attests the package and runs
`gh release create … "$P" release/SHA256SUMS`. The **tags** exist; the **Releases** do not: the latest
published Release on GitHub is still **v2.7.6** (2026-11-12), read from the public API.

The cause is in the tree, not in the credentials: `release/SHA256SUMS` still listed the packages up to
v2.9.0 and did not name `felucca-v2.9.4.fwsc`, and `release/emu/manifest.json` still carried **v2.9.3**
while the firmware said v2.9.4. `tools/coherence_check.py` reported both, and the release step would have
failed after the tag was already pushed — the exact "tag without a release" state observed.

Fixed here: `release/build-manifest-v2.9.4.json` added, `release/emu/` regenerated to v2.9.4 with the same
WASM hash as the build record, and `release/SHA256SUMS` rewritten for every file in `release/` (91
entries). `coherence_check` is the gate; its result is recorded in §7.

**State: `PUBLICADO` and `VERIFICADO ONLINE`.** The release exists and was read back from GitHub:

| | |
| --- | --- |
| Release | `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.9.4` |
| Published | 2026-11-13T14:19:52Z |
| Asset `felucca-v2.9.4.fwsc` | 597 KB, sha256 `c800055f1e35b436…` — the same bytes as `release/` |
| Asset `SHA256SUMS` | 7.11 KB, sha256 `9c87eb21…` |
| Source | `Source code (zip)` / `Source code (tar.gz)` at the tag |

The workflow also attested the package (`actions/attest-build-provenance`), which is what the Hub's
traceability question asks for; the attestation page is linked from the release.

## 2. Flash: the two JEDEC IDs

Both parts are already accepted, in every layer that touches flash — this was audited, not added:

| Layer | File | What it does |
| --- | --- | --- |
| Firmware HAL | `firmware/hal/fm1_flash_id.h` | `FL_JEDEC_P25Q80H 0x856014`, `FL_JEDEC_B36014 0xB36014`, both 1 MiB, SR1 layout of the family |
| Vendor flash | `firmware/hal/fm1_flash.h` | `fl_jedec_ram()`, read/erase/program paths |
| Update loader | `firmware/src/ota.c:435-436` | the capacity/geometry table: `{0x856014, OTA_CAP_1M, 256, 0x1000, OTA_BLK_64K}` and `{0xB36014, …}` |
| Tools | `tools/spi_dump.py`, `spi_restore.py`, `fm1_flash_evidence.py`, `fm1_gate.py`, `fm1_preflight.py` | same allowlist in the reader, the restorer and the gate |

Unknown IDs are rejected, not guessed. No flash limit, map, bootloader or protected region was touched.
What is **`PENDIENTE`** is the physical reading of either ID on a unit, and therefore the write, backup
and recovery proofs: a recognised JEDEC authorises nothing on its own (AGENTS.md §4).

## 3. Presets

Measured from the firmware's own tables (`build/host/preset_audit_test --dump`, `tools/preset_audit.py`):

* **267 presets** across the nine live engines; **0 audit problems**, **0 presets with no send**,
  **265 of 267 with motion** assigned.
* Categories: PADS 57, KEYS 56, TEXTURES 49, DRUMS 24, BASS 18, ORGANS 18, BELLS 15, LEADS 12,
  PLUCKS 11, ARPS 7.
* Levels were measured in the previous pass (`preset_levels`): bank median **−18.5 dBFS**, no clipping,
  every family within 0.5 dB of the median.

No preset was added in this pass: the flash headroom is 15 124 B and every candidate would have to be
audited and levelled; inventing sounds without measuring them is worse than not adding them.

## 4. Sounds and their provenance

There is **no audio file in the repository**: `git ls-files` finds zero `.wav`, `.aiff`, `.flac`, `.mp3`,
`.ogg`. Every sound is synthesised at run time by the nine engines and the modelled DRUM-X kit; the
sample-based engines (SAMPLE, GRAIN, SLICE) are retired and reserved, so no code path reads sample data.

The only distributed patch asset is `web/banks/gw1-keys.syx` (4 104 B): this project's own 11 FM6 KEYS
voices packed as a DX7-style bank by `tools/gen_fm6_patches.py`, which regenerates the committed file
byte for byte. Full evidence: `docs/FM1_HUB_AUDIO_PROVENANCE.md`.

Therefore `fmhub.json` declares `"sounds": "none"` — verified, not assumed.

## 5. `fmhub.json` and the card

`fmhub.json` is at the repository root and complete:

```json
{"what": "A six-track groovebox for the M-VAVE FM-1: …", "author": "Guille Lauría",
 "kinds": ["groove"], "play": "…/emulator/", "editor": "…/webapp/editor/",
 "cheat": "https://github.com/Guillel51/Quantum-GammaWave#readme",
 "image": "docs/card.png", "color": "#35CFFF", "sounds": "none"}
```

`docs/card.png`: **512 × 512**, PNG, **20 860 B**, a real 240 × 240 render of HOME at v2.9.4 scaled ×2 with
nearest-neighbour (no invented artwork), inside the 240–1024 px and 300 KB limits.

**Identity caveat, `PENDIENTE`:** the Hub asks for `FM-1_XXXXXXXX`; this firmware uses `FM-1_965`.
Changing it now would change the package product string the installer and the loader compare, so it is
**not** changed here. Whether `FM-1_965` (or the 8-digit form) is free in the Hub's registry can only be
answered by the Hub's own mechanism; that check has **not** been made.

## 6. Update loader and safety

The package carries the FM-1 loader (`loader/ota.bin`, 7 803 B, hashed in the build record) and the
package manifest lists both images. `fm1_sign.py verify` passes: hash, size, product `FM-1`, images, build
record. The package is **unsigned by design** (SHA-256 + size + build record; Ed25519 is historical), and
the site and the installer say "integrity, not authenticity".

Writing stays gated: no flash limit, map or bootloader was modified, and the installer's gates are
unchanged. Leaving Quantum GammaWave for another firmware is **not** claimed: that needs a physical unit.

## 7. What was run in this pass

| Check | Result |
| --- | --- |
| `./build.sh` (AC791N, JieLi toolchain + AC79 SDK) | **ok** — image 568 488 B, package 610 959 B, identity FM-1_965 |
| `web/emu/build.sh` (Emscripten 6.0.11) | **ok** — WASM 1 191 701 B, sha256 `067f9a07…` |
| `tools/preset_audit.py` | 267 presets, 0 problems, 0 without send |
| `tools/fm1_sign.py verify` (v2.9.4 and v2.9.3) | both **verified**, integrity only |
| `tools/coherence_check.py` | **ok** after the fixes in §1 |
| `build/host/ui_test`, `regress`, `ui_render` | all green (186 screens × 22 palettes, 0 findings; 285 goldens, 0 changed) |
| `node web/test_web.mjs`, `web/test_backup.mjs`, `tests/install_test.py` | pass |

Not run, not claimed: a browser session against the built emulator, the site online, the tag workflow's
result, and anything at all on an FM-1.

## 8. The Firmware Hub readiness

| Requirement | State |
| --- | --- |
| Public repository, full source, GPL-3.0-only, third-party licences present | **IMPLEMENTADO** (`LICENSE`, `LICENSING.md`, `LICENSES/`) |
| Official GitHub Release with `.fwsc`, manifest, hashes, notes | **BLOQUEADO** — the workflow needs the push in §1; no token here to read the result |
| FM-1 identity, format and availability | **PENDIENTE** — `FM-1_965` in use, Hub availability unchecked, format differs (`_XXX` vs `_XXXXXXXX`) |
| `fmhub.json` | **IMPLEMENTADO** |
| `docs/card.png` | **IMPLEMENTADO** |
| Sound provenance | **IMPLEMENTADO** — none: no audio file ships |
| Flash compatibility evidence for `0xB36014` | **IMPLEMENTADO** in code and tools; **PENDIENTE FÍSICAMENTE** |
| Real install on an FM-1 and leaving it again | **PENDIENTE** — `docs/FM1_PHYSICAL_VALIDATION_GUIDE.md` |
| Site, emulator, installer and Web Studio online | **VERIFICADO ONLINE** (HTTP + manifests + `site_check`); the emulator's audio and controls are **PENDIENTE** |
| Submission to the Hub | **PENDIENTE** — needs the owner's authorisation and the physical test |

The project is prepared. It is **not** accepted, listed or approved by The Firmware Hub, and this
document does not claim it is.

## 9. The public site: it was frozen, and why

`https://guillel51.github.io/Quantum-GammaWave/` answered with a **v1.4-QG preview page** — the emulator's
old single page — while the repository's `site/` holds the v2.9.4 portal. No file in this tree produces
that page, and this repository had **no Pages workflow at all**: `site/` was built and committed by hand,
so the deployment GitHub Pages kept serving was the last artifact of a workflow that no longer exists.

`.github/workflows/pages.yml` now deploys `site/` as committed (portal at the root, emulator at
`/emulator/`, installer at `/webapp/installer/`, Web Studio at `/webapp/editor/`), after checking
`tools/site_check.py` and that the package the portal serves is the one the release carries.

The first run also failed at `site_check`, because that tool compares a **URL** with the files: run before
the deployment it was validating yesterday's site. It now runs *after* deploying, and the pre-deploy step
checks the tree instead (the version the portal offers).

Two more things the deployment exposed, both fixed:

* `site/firmware/*.fwsc` was **git-ignored** (`.gitignore` allows `release/*.fwsc` only), so the package the
  installer downloads was a 404 on every published version until now. `!site/firmware/*.fwsc` and the
  v2.9.4 package are committed.
* `site/emulator/` still held the **v2.9.1** WASM. It is updated, and the workflow copies `release/emu/`
  over it and asserts version and `wasm_sha256` before uploading, so it cannot drift again.

**State: `PUBLICADO` and `VERIFICADO ONLINE`.** Read back from the live site:

| Address | Answer |
| --- | --- |
| `https://guillel51.github.io/Quantum-GammaWave/` | 200, prints **v2.9.4** |
| `/firmware/felucca-2.9.4.fwsc` | 200, sha256 `c800055f1e35b436…` — the release's bytes |
| `/firmware/versions.json` | 200, offers 2.9.4 |
| `/emulator/manifest.json` | **v2.9.4**, commit `9df4067`, `wasm_sha256 067f9a07…` |
| `/emulator/`, `/webapp/installer/`, `/webapp/editor/` | 200 |

`python3 tools/site_check.py --url https://guillel51.github.io/Quantum-GammaWave` → **all checks passed**
(the portal's version, the emulator link, the installer's published version and its Verify-Only checklist).

What is **not** verified: the emulator was never opened in a browser here, so audio, the controls and the
canvas are `PENDIENTE`; the pages were read over HTTPS, not looked at.
