# QG-1 — consolidation report (2026-11-13)

What this pass found, what it changed, what it measured, and what it did not do. Every number carries its
class: **source**, **AC791N build**, **host x86**, **emulator (WASM)**, **browser**, **network**,
**PENDING** (needs the unit). No FM-1 was touched.

## 1. The state this started from (not the state the brief assumed)

| | |
|---|---|
| Branch | `freebuff/5b290d6da44f6281decb5d8d` (working branch, never pushed by this pass) |
| HEAD at the start | `dcdb09f` — *Repackage v2.9.2 with HOME > DYNAMICS* |
| `origin/main` (fetched, `git rev-parse FETCH_HEAD`) | **`bdc1a0d`** — *Package v2.9.2 with the drawn icons* |
| Public tag `v2.9.2` | `refs/tags/v2.9.2` → `bdc1a0d`, package sha `e7727491…`, **the release is already public** |
| Local annotated tag `v2.9.2` | moved to `dcdb09f` (the newer tree); **not pushed** |
| Commits ahead of `origin/main` | 5 |
| Untracked | `release/emu/fonts/` (the emulator's own webfont, copied by the packaging step) |

The brief's "last public state `9c8ccbf`" is the v2.9.1 sync commit; the public tree has moved five commits
past it since (v2.9.2 was packaged and tagged). All ten commits the brief lists exist and are ancestors of
HEAD — `git log --oneline -1` was run on each.

## 2. Audit of the ten commits — claim vs code

| Commit | What it claims | Checked how | Verdict |
|---|---|---|---|
| `192039f` | lit panel edges, HOME voice pool, build toolchain | source + AC791N build | present |
| `ea01671` | polyphony choice, CPU MODE, chord on keys | source; `NVOICE` is 12 (`firmware/src/core.h:58`) | present |
| `23a4209` | measured levels in the browser and HOME | source | present |
| `7496469` | kerning the 12 px grid cannot show | `tests/text_spacing_test.py` **pass** | present |
| `4329e4e` | icon fringe, FX-BUS floor and movement | `ui_render` sweep | present |
| `7d3cb37` | FX BUS rebuild + 51-parameter audit | source, docs | present |
| `25c3a497` | 216 EDIT controls, 267 presets audited | `preset_audit_test` + `tools/preset_audit.py`: **267 presets, 9 engines, 0 problems** | present |
| `bb2ee216` | voice_alloc divide-by-zero | source guard, `regress` | present |
| `1e4c31a` | version + README | README now says `v2.9.2` | present |
| `9c8ccbf` | artefact sync + publication block recorded | `coherence_check`, `docs/QG_RELEASE_BLOCKERS.md` | present |

## 3. Defects found and fixed in this pass

1. **`tests/test_polyphony.c` deleted.** It was a standalone copy of the voice allocator with its own fake
   `track_t`, and it did not compile (`lvalue required as unary '&' operand` at line 32). It linked none of
   the firmware, so it could not fail when the real allocator broke. The behaviour it pretended to test is
   covered against the real engine in `tests/regress.c` (shared budget of `NVOICE`, stolen voices fade
   instead of stepping, MONO/LEGATO/UNISON handing over, the VOICE engine's 4-voice cap, release freeing
   voices). Deleting it loses no coverage; keeping it kept a red test out of the suite.
2. **The render harness crashed on a single-screen dump** (`finding()` wrote to a `FILE*` that is only
   opened in the full run). It reported the crash as nothing; now it writes to `stderr` and the finding is
   visible. (Committed with the DYNAMICS work, `9866c31`.)
3. **README's highlights were `v2.7.4.7`** — heading and numbers (image 541 484 B, WASM 1 104 650 B) while
   the header said `v2.9.2`. Rewritten from this build's measurements.
4. **`docs/BUILD_AC791N.md` said 190 screens**; the sweep is 191 (DYNAMICS is a screen).
5. **The screenshots were older than the UI.** `tools/generate_screenshots.py` re-ran: 9 of the 40 firmware
   captures changed (the HOME strip is eight cells now, ROUTING and the themed homes moved with it).

## 4. Measurements taken here

| Measurement | Value | Class |
|---|---|---|
| Image | **573 664 B** of a 583 612 B slot (`APP_SLOT 0x8E7BC`) → **9 948 B free** | AC791N build |
| RAM `.data+.bss` | 87 748 B / 98 304; pool spare 15 392 B | AC791N build |
| Constant tables | **41 947 B** (S 14 669 · M 8 420 · L 5 049 · icons 7 885 · keycaps 5 414 · shapes 158 · palettes 352) | AC791N ELF, `tools/table_sizes.py` |
| Package | 610 959 B, sha `42213889…` | AC791N build |
| WASM | 1 223 359 B, sha `5bc3387e…`, emcc 6.0.11, commit `9866c31` | emulator build |
| UI sweep | 191 screens × 22 palettes, **0 lint findings, 0 alignment errors, 0 GREY/MONO off**, 3 156 050 measurements | host x86 |
| Golden renders | **285 renders, 0 changed**, 0 health, 0 voice/routing, 0 CPU over budget | host x86 |
| Presets | **267** across **9** engines (ANALOG 68, FM6 37, PHASE 32, LOFI 29, VOICE 25, NOISE 22, WHEEL 22, DRUM 18, WTABLE 14), 0 audit problems, 265/267 with motion | host x86 (`preset_audit_test --dump` + `tools/preset_audit.py`) |
| Preset loudness | bank median **−18.5 dBFS**; **NOISE −18.9** against ANALOG −18.5 (was −29.7 vs −17.9, a 12 dB gap); range −29.2…−17.5; **0 clipping** | host x86 (`preset_levels --csv`, 267 rows) |
| Compatibility tests | `install_test`, `site_check`, `coherence_check`, `tests/text_spacing_test.py`, `web/test_web.mjs`, `test_backup.mjs`, `test_pkg_bounds.mjs` all **pass** | host x86 |
| Full host suite | `NO_PACKAGE=1 ./tests/run_tests.sh` | `[HOST]` | **1 failure, 1 fixed**: `ui_test`'s HOME walk now expects DYNAMICS (fixed here, 4 checks green again); `morph_six_test` fails **6/6** and it failed before this pass too — see §6 |

## 5. Answers to the brief's premises

- **"204 B of flash left."** Not today: the table pass (`37296d4`, `1deff79`) and the DYNAMICS screen
  (`9866c31`, `dcdb09f`) leave **9 948 B**. There is room for a small feature, not for a new engine
  (GRANULAR was measured at 22 051 B).
- **"NOISE sits ~12 dB below ANALOG."** Measured now: NOISE median −18.9 dBFS, ANALOG −18.5 — the gap is
  **0.4 dB**, because levels are applied from `preset_level.c` at load time. No preset was re-voiced in this
  pass; the numbers are the ones the shipped bank produces.
- **"`tests/test_polyphony.c` is a dead copy that does not compile."** Confirmed, and removed (§3).
- **"README says v2.9.1 and highlights v2.7.4.7."** Half true: the header was already `v2.9.2`; the
  highlights were not. Fixed.

## 6. What was *not* done, and why

| Not done | Why |
|---|---|
| `morph_six_test` (6/6 transitions) | **Pre-existing failure, not caused here and not hidden.** Its own control render is silent (`rms per block 8:0/0 …`, level 0 → 0), so it measures nothing; it reproduces identically at `bdc1a0d` (compiled from a clean worktree of the published tag with the same headers) and at `dcdb09f`. Setting `mc_off = 1` in its reset (the host has no second core) does not change it. Left failing in the suite, reported: it needs a pass of its own |
| Any hardware test | The owner's unit is not written, ever; no other FM-1 is attached. **PENDING** |
| The SPL/SDK equivalence question | Needs the SDK snapshot that built v2.9.0, which is not here. See `docs/QG_RELEASE_BLOCKERS.md` §B1 |
| A browser session over the emulator page | Not run in this pass; the WASM is verified against its manifest, the page is not exercised. **PENDING** |
| SCALE / CHORD / MOD MATRIX / LFO feature work of §6 of the brief | Each needs a UI + engine design pass and its own measurements; nothing was started. The margin (9 948 B) exists but a design does not |
| New presets (SID/ACID/POLY as sounds) | No preset was added; flash existed, but adding sounds without an audible check would be guessing |
| Moving `FELUCCA_VERSION` | v2.9.2 is already published at `bdc1a0d`; the newer tree is a **v2.9.3** candidate and renaming it now would contradict `release/` |

## 7. Files and commits of this pass

- `tools/spl_diff.py` (new): reads the SPL out of two packages and compares them — the measurement behind
  §B1 of the blockers.
- `docs/QG_RELEASE_BLOCKERS.md` (new), this report (new), `docs/VALIDATION_MATRIX.md` (appended section),
  `README.md` (highlights), `CHANGELOG.md`, `docs/BUILD_AC791N.md` (191 screens),
  `assets/screenshots/*`, `assets/ui-art/art/*` (regenerated).
- `tests/test_polyphony.c` removed.
- Earlier in the same branch, already committed: `9866c31` (HOME > DYNAMICS), `dcdb09f` (repackage), and
  the pass before that (`37296d4`, `1deff79`).

## 8. What this report does not claim

It does not claim the firmware runs on an FM-1, that the emulator sounds, that the site renders correctly in
a browser, that the Hub accepted a card, or that any package is signed. It claims that this tree builds, that
its tables and goldens are measured, that the package, manifest and mirror agree, and that the one thing
still open about publishing is evidence about a vendor boot payload that this environment cannot produce.
