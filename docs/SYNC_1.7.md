# 1.7 sync record (commit 8e079e7)

- Target build: JieLi AC791N, app 545932 B, package 610113 B, identity FM-1_982.
- Package SHA-256: 17cd0b691451a91cd8992f452f561abed750a82ba4e8926e941f9322731c9712
- DSP target budget: tests/target_budget.py exit 0 (mod_begin 35/166, mod_voice 19/30).
- Package parser: web/test_pkg_bounds.mjs all checks passed.
- Site/installer generated from that package: build/site (1311487 B), version v1.7-QG-8e079e7,
  installer 86996 B; not deployed.
- Installer CLI: tests/install_test.py passed.
- Web Studio and backup: web/test_web.mjs, web/test_backup.mjs passed.
- Emulator: rebuilt from the same sources; emu_test all ok, bit-identical to native, 22.6-24.3 ms/s.

## Still open

- Preset classification: the static categorized preset list in web/editor.html exposes 100 entries, not
  the 200 factory sounds. No 200-entry primary-category table or filter is implemented yet.
- Preset Compare: absent. Motion Recorder: automation exists, real-time knob recorder does not.
  Preset Morphing: wave morph only, no preset-to-preset interpolation.
- No Pages deployment: the public site was not updated from this environment.
# 1.7 sync record (branch feat/v1.7-visual-presets, e23fb14)

- Target build: JieLi AC791N, package 610113 B, identity FM-1_982.
- Package SHA-256: dc2790f0ae20175d9d548110177fc3dce48913188cbbc3238da990dde0eb55a1
- DSP target budget: tests/target_budget.py exit 0 (`mod_begin` 35/166, `mod_voice` 19/30).
- Last site build: build/site_final, version v1.7-QG-73bf7b2, installer 86996 B, editor 432125 B; not deployed.
- Installer CLI: tests/install_test.py passed (including SHA-256 refusal and verify-only).
- Web Studio: web/test_web.mjs passed; the editor ships the 200-sound category JSON.
- Emulator: rebuilt from the same sources; emu_test all ok, bit-identical to native, 2.3 % of real time.
- UI: ui_test all passed; ui_render 172 screens x 17 palettes, 0 lint findings, 0 alignment errors.
- Presets: 200/200 classified, one primary category each, no per-engine duplicates.
- Test groups: tests/groups.sh splits ui, presets, web, emu and target under the five-minute shell limit.

## Still open

- UI stabilization is not complete: the HOME visualizer's peak decay is fixed in e23fb14, but its
  broader layout, GLO, Settings, FX overlap/icons and BIQUAD remain open. The FLAT/LINE round-knob
  attempt failed the 150000-frame soak (55 stale cards) and was reverted; it is not shipped.
- Preset Compare is implemented in Web Studio only; the FM-1 screen version is deferred.
- Motion Recorder is automation/MOTION only; a real-time timeline is not implemented.
- Preset Morphing is wave morph only; preset-to-preset interpolation is not implemented.
- No Pages deployment: the public site was not updated from this environment.
- No physical FM-1: display, encoder, audio and recovery behaviour are NOT VERIFIED on hardware.

## HOME encoder / mini-knob / splash pass (2026-11-03)

- `bg_knob()` uses `home_param()` in HOME; `page_desc()` remains correct on engine pages.
- New `test_home_knobs()` walks every available engine and KNOB 1–4; only the selected parameter and
  its mini-knob move.
- Mini-knobs: concentric rim/body and a 270-degree needle; splash: second electron, solar flares and
  progress-driven wave phase.
- Target: app 549028 B, package 610897 B, identity `FM-1_982`, package SHA-256
  `932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`.
- App slot 581564 B; current app 549028 B, unused 32536 B. `548708` / `549012` were earlier baselines,
  not the current app. The `0x93000` package image is not the physical 1 MiB part.
- UI: 172 × 17, 0 lint/alignment; splash 425 steps, 200 changing, 225 identical rewrites skipped.
- Loader: exact final `success\0` ACK required; wrong/lost ACK returns `-14` without clearing the
  update record. Host tests cover the lost-ACK path.
- Installation audit: **NO VERIFICADO EN HARDWARE — NO APTO PARA INSTALACIÓN REAL**; no physical FM-1,
  backup or USB_KEY recovery demonstration.

## perf/security-hardening (earlier baseline)

- Branch `perf/security-hardening`, merged with `origin/main` (`e3475f0`) at `08c74d0`; PRs #47–#51
  are closed and merged. No open PR remained to update at the time of this record.
- This is a historical baseline, not the final HEAD. It had app 548852 B, package 610876 B, identity
  FM-1_982; package SHA-256 `b0d4d9c56690f6224dadcefbf06690b3e33ef0d03c819e3778eb7d85dd9c6a8b`.
- RAM: `RAM spare 3732 B; pool spare 9248 B; RAMTEXT spare 2436 B; NOINIT spare 124 B`.
- Host: loader normal + ASan/UBSan pass; web, security and preset groups pass; UI 172 screens x 17
  palettes, 0 lint / 0 alignment; FX draw cost 123.9 host units (was 125.3).
- Final visual pass: the scope now separates waveform, real phase plot and spectrum; the splash adds the
  QUANTUM wordmark and solar core. App +748 B, package unchanged; 198 of 425 splash steps change pixels.
- Flash/loader/UI detail and residual risks: `docs/PERF_SECURITY_AUDIT.md`. No physical FM-1
  validation, no release, no Pages deployment, no force push.

## Final audit (2026-11-03, perf/security-hardening)

- Target: app **549028 B**, package **610897 B**, `FM-1_982`, package SHA-256
  `932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`.
- RAM spare 3716 B; pool 9248 B; RAMTEXT 2404 B; NOINIT 124 B. `perf_block` 569/522;
  `DRUM_PATH` 1189/1191.
- Green: UI (172 x 17, 0 lint/alignment), presets, web, emu, security, stress 1, target,
  loader normal and ASan/UBSan.
- FX UI redesign is IMPLEMENTED/TESTED ON HOST. The buffered Reverse/Glitch/Freeze/Tape Stop bank was
  DISCARDED because it exceeded `perf_block` and RAMTEXT; no budget was relaxed. F#5 remains DIVE.
- Host finish ACK now reports `lost` if the `success` frame cannot be sent.
- **NO VERIFICADO EN HARDWARE; not approved for installation.** See `docs/AUDIT_FINAL_FM1.md` and
  `docs/HARDWARE_TEST_PLAN.md`.
