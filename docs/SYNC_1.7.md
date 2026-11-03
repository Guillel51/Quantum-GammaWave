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
