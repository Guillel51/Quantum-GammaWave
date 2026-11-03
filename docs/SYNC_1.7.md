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
