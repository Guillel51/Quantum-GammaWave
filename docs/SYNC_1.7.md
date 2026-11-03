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

## perf/security-hardening

- Branch `perf/security-hardening`; target app 545676 B, package 610116 B, identity FM-1_982
  (the package changed after the loader record-clear fix).
- Package SHA-256: 1a3ca1353d7aa3697b85e8e2651b90a76e797a0d2d0241ac96a3053ffee696d9
- RAM: `.data+.bss` 94572 B of 98304; pool spare 9248 B (minimum 8192).
- Host: 220 golden renders 0 changed; six-part 10-voice stress and sanitized project/persistence/robust/MIDI all pass;
  the loader record-clear regression and its ASan/UBSan build pass.
- Emulator: all ok, 2.3 % of real time; UI: 172 screens, 0 lint / 0 alignment.
- Detail and residual risks: docs/PERF_SECURITY_AUDIT.md. No hardware validation, no deploy.
