# Audit of 2.1.2 — identity chain, three checks that could not fail, and the rewrite of `main`

Written 2026-11-10 by the session that audited the `FM-1_965` change and release 2.1.2, while other
sessions were working on the same repository. It is the record of what this session measured, what it
changed, and what it found about the shared branch. Nothing here is inferred from another report: every
line below was produced here, and the class of each measurement is named.

## 1. What the audit found

| Ref | Value, read with `git ls-remote` |
|---|---|
| `release/v2.1.2` | `bbca837179dd` |
| `refs/tags/v2.1.2` | `bbca837179dd` |
| package `release/felucca-2.1.2.fwsc` | 610 959 B, SHA-256 `1f52bf091586dde4cd08139fec637f40e70f3216fb78d690d8ff2eb3a08ba303` |
| manifest | `fcf4ccb1…`, signed `qg1-release-1` from build `03d3f80`, `dirty: []`, identity `FM-1_965` |
| live site (root) | `v2.1.2-03d3f80`, same SHA-256, installer page embeds the same hash |

The chain identity → package → signature → installer → site held, measured independently: the Ed25519
signature was verified **without any code from this repository** (`cryptography` plus a canonical JSON
written in the auditor's own script) against the pinned `qg1-release-1`; `FM-1_965` and the loader string
`ota-FM-1_965` were decoded from the bytes with an independent parser; a rebuild in a worktree at
`03d3f80` produced the same SHA-256.

Three defects were found, and all three are fixed on this branch.

### 1a. WASM/native parity could not pass

`web/emu/emu_test.mjs` records after `splashMs + 200` (`SPL_WRITE 1500 + SPL_HOLD 1600 + 200 = 3 300 ms`);
`web/emu/native_check.c` rendered a hardcoded `render(500)`. The two harnesses therefore recorded
different performances, and the first recorded sample already differed. It failed identically at
`9fbb8c0`, which is why an earlier pass read it as a toolchain artefact — **that reading was wrong and is
withdrawn**. The native harness now renders `SPL_WRITE + SPL_HOLD + 200` from `ui_draw.c` (already in its
translation unit): **bit for bit, 88 200 frames**, `emcc 6.0.11`.

### 1b. The stable emulator named a commit its bytes did not come from

`/emulator/` displayed `bbca837179dd`, while `firmware/versions.json` and the WASM said `03d3f80`
(`0d437cfe…`). `tools/package_emu.py` read `GITHUB_SHA` before the worktree's HEAD, and the deploy builds
the stable emulator inside a worktree at the release commit. The tree now decides; the environment is the
fallback (the same rule in `tools/build_manifest.py`). Rebuilt in a worktree at `03d3f80` **with
`GITHUB_SHA` set**, the page and the record both say `03d3f80ed385`.

`web/test_site.py` already refused such a page and **never ran in the `deploy` job**; it runs there now.
Negative control, run here: the published page relabelled to `bbca837179dd` is reported
`emulator page does not name its manifest's commit FAIL`.

### 1c. The host suite was red at the release commit

`web/test_accept.mjs` demanded `FM-1_965` from `tests/fixtures/felucca-v1.9.0.fwsc`, which answers
`FM-1_982`: since `03d3f80` the check could only pass by rewriting the fixture. It now reads the fixture's
own identity (history) and runs the same acceptance on the newest signed release, which must answer the
tree's `PRODUCT` — 23 checks, 0 failed. CI never saw it because `c-cpp.yml` runs the suite with
`NO_PACKAGE=1`; `target.yml`, which does build a package, runs `test_accept.mjs` now.

Also added: the 2.1.1 → 2.1.2 transition is driven with the **signed** package
(`web/test_update_mode.mjs` case 1b: a unit answering `FM-1_982` → `gammaWaveLegacy`, `upgrade-legacy`,
admitted, installed, coming back as `FM-1_965`; the same bytes under another model or with a false hash
are refused). The earlier case used the 1.9.0 fixture and passed for the wrong reason.

## 2. Verification actually run

| Check | Result | Class |
|---|---|---|
| `tests/run_tests.sh` | **ALL HOST TESTS PASSED**, 5 036 `ok`, `exit 0` | host x86-64, gcc 13, Emscripten 6.0.11 |
| WASM/native song | 88 200 frames, **bit for bit** | host, WASM + native `cc` |
| `web/test_update_mode.mjs` | 16 checks, 0 failed (incl. the transition) | host, simulated unit |
| `web/test_accept.mjs` | 23 checks, 0 failed | host, fixture + signed release |
| `web/test_site.py`, built site | 19 ids, 7 images, 31 links, 0 failed | host |
| package rebuild at `03d3f80` | same SHA-256 `1f52bf09…a303` | AC791N toolchain |
| the identity in the bytes | `FM-1_965`, loader `ota-FM-1_965` | host, independent parser |
| Ed25519 with the pinned key | valid, no project code | host, `cryptography` |
| live site before this session's push | root `v2.1.2-03d3f80`, `/preview` `v2.1.2-bbca837` (unsigned, `manifest: null`), no service worker anywhere | live HTTP |

Hardware: **nothing was measured on an FM-1**. No write, no flash, no recovery. The hub: `FM-1_965` is
free in `catalog.json` (175 listings, `updated 2026-11-10 12:27 UTC`); no submission was sent.

## 3. What happened to `main` (read this before merging anything)

1. This session committed `af2c002` (the three fixes) and `784abe2` (this record's numbers) on top of
   `bbca837`, and pushed both: the remote answered `bbca837..af2c002` and then `af2c002..784abe2`.
2. Minutes later `refs/heads/main` was `5cc49e8`, which **is not descended from `bbca837`**
   (`git merge-base --is-ancestor bbca837 origin/main` fails). Another session published its own line.
3. That line is not "2.1.2 without these fixes": it is a different tree — `firmware/src/felucca.c` says
   `FELUCCA_ID "FM-1_900"`, `tools/build.py` says `PRODUCT = "FM-1_982"`, `release/` holds only
   `felucca-v1.9.0.fwsc`, and `web/test_accept.mjs` does not exist there.
4. `release/v2.1.2` and `refs/tags/v2.1.2` still point at `bbca837`, and the live site still serves
   `v2.1.2-03d3f80`. **The signed release itself is intact.**

No force-push, reset or restore was performed. Both commits are preserved on
`audit/2.1.2-trazabilidad` (`784abe2`), on the remote, and they are two ordinary commits on top of
`bbca837` — a normal merge, not a rewrite.

## 4. How to reconcile without destroying anything

```bash
git fetch origin
# the fixes, as they were audited
git log --oneline origin/release/v2.1.2..origin/audit/2.1.2-trazabilidad   # af2c002, 784abe2
# put them on whatever main is, as new commits: a merge, never a reset
git switch -c reconcile/main origin/main
git merge --no-ff origin/audit/2.1.2-trazabilidad
```

Expect conflicts in `web/emu/native_check.c`, `tools/package_emu.py`, `tools/build_manifest.py` and
`.github/workflows/emulator.yml` if the other line solved the same problems its own way (its commits are
titled `pages: …` and it has `tools/emu_stamp.py`): keep **both** behaviours where they agree, and take
whichever stamps the emulator with the commit its bytes come from. Do not cherry-pick blind: `2.1.2` and
the `FM-1_900/982` line differ in the identity itself, so `web/test_accept.mjs`, `tools/check_identity.py`
and the installer tests need a human decision, not a global replace.

The decision that must be made first, and that only the owner can make: **which line `main` should carry**.
Until it is made, this branch is the safe home of the three fixes and of this record.
