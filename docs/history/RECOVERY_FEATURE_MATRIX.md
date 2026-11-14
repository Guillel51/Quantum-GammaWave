# RECOVERY FEATURE MATRIX — Quantum GammaWave

| Feature | In main | In other branch | Real state | Action |
|---|---|---|---|---|
| 12-voice global pool | yes | yes (perf) | Integrated (NVOICE=12, VPOOLN=NVOICE+NPART) | None |
| 4/5/6 track modes ECO/BAL/FULL | yes | yes | Integrated | None |
| Punch-in FX + LATCH | yes | yes (work/v18) | Integrated | None |
| T1-T6 individual mutes | yes | - | Integrated | None |
| Karplus-Strong as engine | no | no (only tools/gen_waves.py) | NOT integrated; never was | Do not claim |
| Three-head motor / LOWBUF overlay | no | no | NOT found in code | Do not claim |
| EQ (master biquad) | yes | perf/integral-audit (fixes) | Integrated; extra fixes not merged (conflicts) | Re-evaluate later |
| Phaser / M-WIDE | yes | - | Integrated | None |
| Reverb (5), Delay, Chorus | yes | - | Integrated | None |
| Slicer, Punch, Duck, Limiter | yes | - | Integrated | None |
| UI Quantum / themes / knobs | yes (partial) | feature/* (destructive) | Present in main; branch version regresses | None |
| Presets (200 factory) | yes | - | Integrated | None |
| Emulator / WASM | yes | - | Present; not rebuilt this pass | Rebuild when toolchain available |
| Web Studio / installer | yes | - | Present; protections intact | None |
| Banner + gallery | yes (banner regenerated) | - | Banner integrated in assets/branding | README gallery restored partially |
| Signed package v2.4.1 | no | - | Blocked: no private key | Declare block |
| Tag/Release v2.4.1 | no (branch v2.4.1 only) | - | Blocked: no tag push creds | Create via GitHub UI |