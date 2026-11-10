# RECOVERY BRANCH AUDIT — Quantum GammaWave

## Method
For each remote branch, compared commits not in main (git log main..origin/<branch>) and diff stat. Branches that are destructive (net deletions) were not merged wholesale.

## Branches investigated
- work/v18-integration: no commits ahead of main at audit time.
- feature/quantum-handoff-evolution-7533491563390614422: no commits ahead of main at audit time.
- feature/qg1-ui-nav-themes-presets-10550540118776558527: no commits ahead of main at audit time.
- feature/fix-home-viz-and-drum-presets-12092529543400008897: commits ahead, but diff is net -21154 lines (deletes tests, assets, i18n, site files). NOT merged.
- perf/integral-audit: ~12 commits ahead (EQ fixes, installer chip gate, docs). Cherry-pick attempted; conflicts in fx.c, felucca.c, i18n.js, index.html, groups.sh, eq_response_test.c. Aborted to protect tree.
- perf/security-hardening: merge commit only; no unique commits ahead at audit time.
- perf/six-track-hardening: commits ahead (perf tests, ui fixes). Not cherry-picked (conflict risk).
- work/fm1-signing-custody: commits ahead (OTA reply buffer, AC791N build). Not cherry-picked.
- main: base of truth (most complete).
- **freebuff/6ae847e53332b13a021ccec4**: 10 commits ahead, last is `b6d53cf build AC791N, wasm`; already in main or contains merge commits already integrated
- **freebuff/4549bd389852a9607bba2622**: 5 commits exclusive to main, changes in `firmware/src/gfx.c`, `firmware/src/ui_being.c`, `firmware/src/ui_draw.c`, `firmware/src/ui_graph.c`, `firmware/src/ui_menu.c`, `tests/morph_audible_test.c`, `tests/morph_six_test.c`, `tests/particle_bounds_test.py`, `tests/run_tests.sh`, `tests/ui_render.c`; cherry-picked f5d3eed and b8a9be7 (track meter and docs improvements)

## Action
No branch was merged wholesale. Main retained as base. Cherry-picked useful commits from `freebuff/4549bd389852a9607bba2622`. Recovery branch created: recovery/full-project-history-2026-11-10.