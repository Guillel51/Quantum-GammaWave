# FM-1 audit without hardware

Date: 2026-11-04. Branch: `audit/fm1-no-hardware`, based on `main` at `fe61aa2`.
`main` was not modified, merged, pushed or deployed. No FM-1, bootloader, SPL, ROM,
partition table or flash map was touched. No physical device was connected.

This report separates **implemented**, **host tested**, **simulated**, **target compiled**
and **hardware not verified**. A green build is not a physical validation.

## Artifact chain

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `build/felucca.fwsc` | 610897 | `580d4275243491c8bbb2003c457da1d49422d98927b8c03118e46fed3689b936` |
| `build/felucca.bin` | 555196 | `d3fd450f3e49c2273c3d5ec84b915eb76908ae70926258fa91a9da59956c3bf9` |
| `build/felucca.elf` | 626184 | `9c05a8ce4c5c84762a9fc553b8cb15461fc0c83d4dbb73d14ab6a96746708f8d` |
| `build/felucca.dis` | 8188229 | `dac5be6b47e34265f288e23ff0b6b1f1095d51e41eb0412d2bf687f5fc61e306` |
| `build/loader/loader.elf` | — | `19915a8d94ae57e8815043e9cf71175f9947fc117942c2683d5e6468ffefd878` |
| `build/emu/felucca.wasm` | 1038272 | `1d880f332a633d2e8a9c457a3bf95b39bd65881544365d8c0a3bfb6d651a09fe` |
| `build/emu/manifest.json` | — | `338ab669a0d55c8995ad049727e58202b4eff66645df903be52ba2238c270bc9` |

The emulator manifest says `v1.8.6`, commit `fe61aa254642`, and the same WASM
size/hash. `tests/groups.sh sync` independently compared page and manifest and passed.
The WASM row is from the rebuild after the EQ and PUNCH-IN FX fixes below; the
`.fwsc`/`.bin`/`.elf` rows predate those source changes, because the JieLi toolchain is
not present in this VM (`tests/groups.sh target` reports it missing).

## Executed checks

| Area | Result | Evidence |
|---|---|---|
| Target build and memory budgets | **PASS** | `build/audit_target.log`: `.data+.bss 95732 B`; RAM spare `2572 B`; pool spare `9248 B`; RAMTEXT spare `2112 B`; NOINIT spare `124 B` |
| Security / sanitizers / fuzz / EQ | **PASS** | `build/audit_security.log`: ASan/UBSan; `fuzz_ed` 18777 frames; `fuzz_proj` 5000 iterations; EQ 480 corners |
| UI rendering | **PASS** | `build/ui_group.log` / `build/ui_group/report.txt`: `tests/groups.sh ui` exit 0, 173 screens × 17 palettes; 0 lint findings; 2,642,626 alignment measurements, 0 off by 1 px; splash 425 steps / 200 changing frames |
| UI draw cost (performance) | **PASS (host, cc -O1)** | `build/ui_group/report.txt`: full redraw 126–231 µs per screen; PUNCH-IN FX (`fx`) 136.2 µs, 30 texts+icons, 8008 glyph px read; an unchanged frame 0.34–0.83 µs. Host timings only the ratios mean anything on the device |
| Web, editor protocol and assets | **PASS** | `build/audit_web3.log`: `web tests passed` |
| WASM host emulator | **PASS** | `build/audit_emu.log`: `emu: all ok`; same WASM hash as manifest |
| Presets | **PASS** | `build/audit_presets.log`: 200/200 presets, one primary category each |
| Stress / regression | **PASS** | `build/audit_stress.log`: two full regression passes |
| Installer simulator | **PASS** | `build/audit_install_test.log`: disconnects, lost ACKs, wrong model, `--verify-only`, bad packages, read-past-package; official V15 restore skipped (file absent) |
| M-UPGRADE/OTA staging | **PASS** | `build/audit_ota_test.log`: no head reads; writes only `0xE0000..0xE4FFF`; no commit without `success`; boot cleanup |
| Full host suite (`tests/run_tests.sh`) | **BLOCKED / incomplete** | Tool limit stopped it around `ASan/UBSan: fm6bank_test`; no final exit status. The focused groups above completed separately |
| Full-system emulator with exact `.fwsc` | **PASS (emulated only)** | `fm1-emu 0.4.0 (9de1ef053)`; 1000 ms, 240,000,000 instructions, CPU1 released, 8,787 interrupts, 144 LCD frames, 0.88 s WAV; `build/audit_emu_full.log` |
| Full-system display | **PASS (emulated only)** | `--snap 900,1000` produced `work/lcd_1000.bmp`; converted to `artifacts/audit-lcd-1000.png`, showing the current `v1.8.6` splash |
| Core-1 flaky RAM loads | **PARTIAL / simulated** | 600 ms, `--flaky-core1 0.01 --cpu1-survives`; 1 misread/100 loads, 77 LCD frames, 0.48 s WAV, no crash (`build/audit_emu_fault.log`). This is not a real core loss or a fault-injection proof |
| Browser emulator | **PASS (browser tested)** | Chromium with local sysroot; clicked `Switch on`; captured `artifacts/audit-emulator-on.png` showing HOME, 120 BPM, ANALOG/SAW LEAD; no page or console errors |
| Web Studio visual | **BLOCKED** | The editor page closes the Chromium renderer before a screenshot, with separate browser processes and `--disable-gpu` / `--single-process`. Its Node protocol test passes, so this is a browser/renderer limitation of this environment, not evidence of a product failure |
| Physical flash, recovery, bootloader, secure boot | **HARDWARE NOT VERIFIED** | See limits below |

## Corrections applied

1. **CI fail-closed release gate** (`.github/workflows/emulator.yml`).
   The workflow keeps its emulator-only fallback, but a new `if: always()` step requires
   `build/felucca.fwsc` and `build/felucca.dis` before the site/deploy path. A failed
   toolchain or package step can no longer produce a green publish with a stale package.
   The YAML parses and the jobs are `build` and `deploy`. GitHub Actions itself was not run.

2. **Built-page link regression** (`web/test_site.py`).
   The old test only followed links from the portal. It did not resolve links from
   `/emulator/`, `/webapp/editor/` or `/webapp/installer/`, where a relative-depth
   regression had already shipped once. The test now resolves every local `href` from
   each generated page. A deliberately broken `../webapp/missing-editor/` link made it
   fail (`BROKEN_LINK_TEST_EXIT=1`); the real build passed with 0 failures.

3. **Uninitialized test buffer** (`tests/ota_test.c`).
   `u[64]` in the test's `ota_wire_send` hook was read at `u[2]` after a decode that
   could be shorter. Initializing it removes the `-Wmaybe-uninitialized` warning and
   the undefined read in the test harness. `ota_test` was rebuilt with `-Wall` and passed.

4. **EQ biquad deformed at extreme settings** (`firmware/src/fx.c`).
   The coefficients were generated in Q14/Q15 and `a2` was clamped to `BQ_A2_MAX 0.995`
   (`BQ_MARGIN` 1/4096). At 20 Hz the true denominator is about 8e-6, so the clamp and the
   coarse quantisation bent the curve: maximum gain at minimum frequency did not reach the
   requested dB and the shelves lost their corner. The coefficients are now generated in
   Q26 from Q30 sine/cosine (`sin(w/2)` for `1-cos(w)`) and stored in Q25; the Jury guard
   is 1 LSB. `tests/eq_sweep_test.c` grew an explicit 20 Hz / +24 dB case for all four
   types at Q 0 and 127. Measured: peak +24 dB → +23.9 dB, low and high shelf +11.9 dB at
   the corner, notch −24 dB, symmetric at −24 dB; 480 corners with 0 residual tails and 0
   invalid coefficients. `tests/groups.sh security` re-runs it and passes under ASan/UBSan.

5. **PUNCH-IN FX screen rebuilt at 240×240** (`firmware/src/ui_layer.c`, mirrored in
   `web/editor.html`). The effects, the mutes and KNOB 1–4 are unchanged; only the
   presentation was rebuilt. The five state cards (BANK, LATCH, TARGET, TRACK, DEPTH) are
   44×40 px at y 3, 3 px apart; the keyboard starts below them (white keys 12 px with a
   2 px channel and a 1 px edge, black keys 8×58 px, 6 px between neighbours), against the
   old 9 px keys and `T_RAISE` fill that made neighbours merge. Two defects were found by
   the UI audit and fixed: the third line of each card fell 1 px past the card's bottom
   edge (1700 lint findings; the card is now 40 px tall with a 2 px margin), and the white
   and black keys were indistinguishable. Final state: `0 lint findings`, 0 alignment
   errors, and `tests/ui_test.c` passes. The Web Studio punch view keeps the same card
   contents and the 12 px / 2 px keyboard.

No firmware clock, flash-map, bootloader or architecture change was made.

## Security / brick-resilience mission (same branch)

A second, separate pass audited brick prevention and worst cases; see
[`BRICK_RESILIENCE_FM1.md`](BRICK_RESILIENCE_FM1.md). New evidence it adds, without
changing this report's verdict:

- Power-cut matrices (host): `tests/storage_cut_test.c` (40 objects, 160 cuts) and
  `tests/ota_cut_test.c` (23 staging cuts + 3 recovery cuts), both wired into
  `tests/groups.sh security`, which passes.
- Emulator limits found while trying to repeat that on the device model: `FM1_FLASH_CUT`
  is absent from fm1-emu 0.4.0, `--flash` rejects every image built from the package, and
  `--raw-flash` is read-only (0 bytes written in 2.5 s / 180 LCD frames). The flash-cut
  half of the mission is therefore **BLOCKED** at the emulator, not skipped.
- `--flaky-core1` matrix: 14 runs (seeds 1/7/13, rates 0.001..0.05), all exit 0, LCD on,
  no crash screen; the emulator reports a single misread and stops injecting.
- No public AC791N/pi32v2 erratum sheet was found for the multiply/coherence/watchdog
  claims, so they remain **unverified** rather than accepted.

## Findings and limits

- **Physical JEDEC is not read by the installer.** The firmware has an allowlist
  (`0x856014`, `0xB36014`) and falls back to RAM-only for unknown IDs, and the loader
  checks the package's chip key. The CLI/WebMIDI installer itself does not read the
  physical JEDEC ID. Therefore the installer's simulated tests cannot prove that an
  unknown or counterfeit chip is rejected before a physical write. This remains a
  hardware-blocked gap; no speculative protocol was added.
- **Target SDK metadata differs from the reference.** `build/audit_target.log` warns that
  `uboot.boot` and `cfg/eq_cfg_hw.bin` differ from `AC79NN_SDK_V1.2.1`. The package is
  internally consistent and sync-tested, but it must not be called bit-identical to the
  reference SDK build without resolving that difference.
- **No firmware authentication.** SHA-256 is an integrity check against a trusted hash;
  it is not a signature and does not establish secure boot. The project docs already state
  this. No Ed25519 private-key workflow was invented in this audit.
- **Bootloader, power-loss recovery and the reboot after self-update are not modeled**
  by `fm1-emu`. The OTA tests cover the app-side staging and no-head-write rules, not a
  physical bricking/recovery proof.
- **pi32v2 32×32→64 errata not reproduced.** The external research did not document the
  reported pattern, and the current disassembly did not expose an obvious affected
  multiply sequence. No `noinline`, NOP or clock mitigation was applied. This is
  **unverified**, not a PASS.
- **Second core fallback is code-reviewed, not fault-proven.** The flaky-load emulator run
  shows the firmware continues through one misread configuration, but no real core halt,
  late job or corrupted mailbox was injected. The host-side fault path remains unproven.
- **Stale build artifacts exist.** `build/fm1-emu-test` contains older `v1.7-QG` captures
  and logs. They were not used as current evidence; the current splash and browser
  screenshots were generated from `build/felucca.fwsc`/`build/site` at `fe61aa2`.
- **Web Studio visual check is blocked in this VM.** Chromium renders the portal and
  emulator, but the editor renderer exits before capture. The emulator screenshot is
  current and functional; the editor remains a browser-check limitation.
- **The target package is older than the last source fixes.** `build/felucca.fwsc` was built
  before the EQ and PUNCH-IN FX changes, because the pi32v2 toolchain and the AC79 SDK are
  absent here. Host tests, the WASM build and the full-system emulator run are current; a
  package rebuilt from this tree has not been produced or booted in this session.
- **The PUNCH-IN FX screen was inspected as a rendered frame, not on the LCD.** The audit
  renders 240×240 P6 frames (`build/ui_group/ppm/GAMMA_layer_lock_fx.ppm`, converted to
  `artifacts/punch_fx_gamma.png`) and measures them; that is not a panel, gamma or
  viewing-angle check. The Web Studio punch view could not be screenshotted, because the
  editor renderer still exits before capture in this VM.
- **No hardware was available or touched.** Physical audio, DMA dropouts, real MIDI,
  flash timing, panel/encoder behavior, power cuts and recovery are **HARDWARE NOT
  VERIFIED**.

## Reproduction highlights

```sh
tests/groups.sh target
tests/groups.sh security
tests/groups.sh ui
tests/groups.sh web
tests/groups.sh emu
tests/groups.sh sync
tests/groups.sh presets
tests/groups.sh stress
python3 tests/install_test.py
python3 web/test_site.py build/site

# full-system, exact package, no ALSA / no hardware
/tmp/fm1emu/fm1-emu-0.4.0-linux-x86_64/fm1-emu \
  --fwsc build/felucca.fwsc --ms 1000 --snap 900,1000 \
  --wav /tmp/fm1run/1000.wav
```

The browser checks used a local `python3 -m http.server` over `build/site`, Playwright
Chromium, and the libraries extracted under `build/apt/root` and `build/sysroot`.
The editor renderer failure is reproducible in that setup and is recorded as blocked.

## Verdict

The repository is **not** declared safe to install, and the FM-1 is **not** declared
physically validated. What is demonstrated: the target build and budgets, host security
and DSP/UI regressions, the exact `.fwsc` booting in a full-system emulator with LCD and
audio, the WASM page/manifest chain, and a current browser emulator session. What remains
hardware-blocked: physical JEDEC identification, real flash/erase/power-cut behavior,
bootloader/recovery, core-loss recovery and secure boot.
