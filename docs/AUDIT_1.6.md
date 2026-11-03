# Final audit of `feat/v1.6-qg` (2026-11-03)

Independent pass over the 1.6 branch and its diff against `main`, before closing the version. No new
features and no cosmetic changes were made in this pass except where a real defect was found.

## 1. Compatibility — one critical defect, fixed

`ui.c`, `song_main.c`, `project.c` and `console.c` load a stored sound with `preset % npresets`. With the
two DRUM kits removed from the table, a project (or favorite, or user preset) saved with DRUM 10
(HEARTH KIT) or 11 (CYBER DRUM) would have loaded 0 or 1 **silently**: same file, different kit, no message.

Fix: the two kits are back in `DRUM_PRESETS` at their original indices, flagged `.legacy = 1`, and
`engine_library()` (core.h) is the count every browser, dice and editor path uses. `npresets` stays 12, so
every stored index resolves exactly as before. The 200-sound library is the count of non-legacy entries.

Evidence (host, byte-level):

| Check | Result |
|---|---|
| `preset/DRUM/10_HEARTH_KIT` golden hash vs `8dd9dda` | `16abd7bc65061f08` in both — **identical** |
| `preset/DRUM/11_CYBER_DRUM` golden hash vs `8dd9dda` | `b3505f622844a7d8` in both — **identical** |
| DRUM browsable indices 0..9 | unchanged, in order (`tests/preset_library_test.py`) |
| DRUM legacy names and positions | `HEARTH KIT`, `CYBER DRUM` at 10/11 (same test) |
| `sizeof(preset_t)` | 40 B before and after (the flag fits the existing padding) |
| Project format | `P_COUNT`, size and offsets untouched; `project.c`'s frozen assertions compile |
| `tests/regress.c` | 220 renders, 0 changed, 0 gone, 0 health failures |

This is **VERIFIED** as far as a host simulation can: the audio of the two restored entries is bit-identical
to the pre-retirement build. It is not a claim about a physical FM-1.

## 2. Library audit

- 200 browsable sounds: ANALOG 56, PHASE 25, LOFI 25, FORMANT 20, WHEEL 18, DRUM 10, NOISE 17, FM6 29.
- 16 new sounds, each with a distinct parameter signature; the test's duplicate-signature report is empty,
  so none is a hidden copy of another sound.
- Every entry: 8 E values, a 4-stage envelope in 0..127, filter-envelope amount in −64..63, mono 0/1, four
  sends in range, a valid pattern hint. Names are unique within each engine (cross-engine twins such as
  BELL/BRASS/WIND are historical and remain).
- The DRUM replacement pair was chosen because they sit at the end: no index below 10 moves.

## 3. Firmware and emulator parity

- Both use the same translation unit (`firmware/src/felucca.c` / `web/emu/felucca_web.c`); the preset tables,
  EVO wave (`P_LWAVE = 6`), `PAT` source and browser count are shared by construction.
- The emulator could not be rebuilt before this audit: `web/emu/felucca_web.c` still called `lcd_init()`
  without the argument introduced in 1.5. Fixed; rebuilt with Emscripten 6.0.13 (998,738 B) and
  `web/emu/emu_test.mjs` passed, including bit-for-bit equality with the native build.
- The editor's engine table now exposes the 200 browsable sounds (legacy excluded), matching the device.

## 4. Security and flash

- No bootloader, linker script, flash layout, update loader or recovery change.
- `0xA0` handling is the same shape as the existing `0xD0`: three bytes, routed by the existing channel
  table, value clamped, only already-held voices touched; no allocation or new buffer.
- EVO uses bounded integer arithmetic and clamps to ±32767; no table index outside `N_LWAVE`.
- Existing fuzz/package/backup suites pass. Package signing remains design-only: a key in updatable flash is
  not a root of trust, and no physical recovery path is available to validate it.

## 5. Git hygiene

- `main`'s tree is restored to `8dd9dda` (the accidental direct push was followed by a revert commit; no
  force-push and no history rewrite).
- The feature branch contains only the intended sources, tests and docs; `build/` (wasm, PNGs, objects) is
  ignored and not committed; no secrets, tokens or binaries.

## Verdict

| Area | Status |
|---|---|
| DRUM 10/11 old projects | **VERIFIED (host)** — byte-identical golden hashes |
| 200-sound library, ranges, names | **VERIFIED (host)** |
| UI lint/alignment, splash | **VERIFIED (host)** |
| Emulator rebuild and native parity | **VERIFIED (wasm/host)** |
| Installer, Web Studio, backup, package bounds | **VERIFIED (host, simulated device)** |
| ASan/UBSan editor + SysEx fuzz | **VERIFIED (host)** |
| 8/10/12 voices, CPU/SRAM | **SIMULATED (host)**, **NO VERIFICADO on AC791N** |
| Physical FM-1, flash, power cuts | **NO VERIFICADO** |
| Package signature | **NOT IMPLEMENTED (design only)** |
