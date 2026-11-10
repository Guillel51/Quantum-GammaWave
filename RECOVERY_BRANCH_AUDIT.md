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

## Action
No branch was merged wholesale. Main retained as base. Recovery branch created: recovery/full-project-history-2026-11-10.