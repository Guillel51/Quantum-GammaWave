# QG-1 — release blockers: what is missing, and what would clear it

State of the tree this was written against: `dcdb09f` (branch `freebuff/…`, 5 commits ahead of
`origin/main` = `bdc1a0d`, which is the published `v2.9.2`). Every line says how it was checked.
Classes: **read from source**, **compiled (AC791N)**, **measured on host x86**, **read from the published
bytes**, **emulated**, **PENDING** (needs a physical FM-1 and was not done here).

> This file replaces §8 of `docs/QG_EVOLUTION_FINAL_REPORT.md` as the place where the blockers are
> argued. That report stays as the record of the pass that first wrote them.

---

## B1 — the SDK's provenance: `uboot.boot` and `cfg/eq_cfg_hw.bin`

### What the two builds say

| Where | `uboot.boot` sha256 | `cfg/eq_cfg_hw.bin` sha256 |
|---|---|---|
| `tools/build.py` `SDK_SHA256` (the pins; the build refuses to call a mismatch tested) | `4e3b4c22…76ded4` | `41167491…0ea7d` |
| `release/build-manifest-v2.9.0.json` (published 2.9.0, SDK commit `0617f4cc…`) | `64ccc63e…b867e` | `179ac9f4…e57d8` |
| `release/build-manifest-v2.9.2.json` (published 2.9.2, SDK commit `unknown`) | `4e3b4c22…76ded4` | `41167491…0ea7d` |
| `$AC79_SDK/cpu/wl82/tools/` on the machine that built 2.9.1 and 2.9.2 | `4e3b4c22…76ded4` | `41167491…0ea7d` |

The pins and the last two releases agree with each other and with the files on disk. **v2.9.0 is the odd
one out**: the package people installed was built from a different SDK snapshot than the one this
repository pins.

### What the packages actually contain — measured, not read from a manifest

`tools/spl_diff.py` (new) opens the packages and reads the boot table, so the claim does not depend on a
JSON that the build itself wrote:

```
release/felucca-v2.9.0.fwsc: uboot.boot at 0x00a0, 14444 B, crc16 0xddbe, sha256 64ccc63e…b867e
  payload 14428 B, sha256 12831a9c…758b31
release/felucca-v2.9.2.fwsc: uboot.boot at 0x00a0, 14536 B, crc16 0xdafa, sha256 4e3b4c22…76ded4
  payload 14520 B, sha256 b57bd7b4…308415
  the stored container is different bytes (14536 vs 14444 B)
  payloads 14428 vs 14520 B, common prefix 6 B, common suffix 35 B,
  the region between them is 14387 vs 14479 B
```

That settles the question the earlier report could not: **these are two different SPL builds, not one
payload in two containers.** The BANKCB payload is 92 B larger in 2.9.2, its first differing bytes are at
offset 6 (the payload's own header/entry region), and the decoded bodies differ across essentially their
whole length. `eq_cfg_hw.bin` differs as well (`179ac9f4…` vs `41167491…`), from the same pair of SDK
snapshots.

### Why that is a blocker and not a nuisance

`uboot.boot` is the boot payload written to the boot block of every unit the installer writes to. The
owner's standing rule is that the bootloader is not touched, and a package whose SPL is a different build
than the one in the field is exactly that. What is still missing is **whether the change is benign**, and
that cannot be answered from here:

- there is no copy of SDK commit `0617f4cc…` in this environment (`$AC79_SDK` holds only
  `cpu/wl82/tools/`, is not a git checkout, and `~/ac79.zip` is a *truncated* download of a different
  release, `AC79NN_SDK_V1.2.0`, whose contents cannot be hashed past the cut);
- the path recorded in the 2.9.0 manifest is another session's home, which this one may not read;
- nothing here can run the SPL or watch it boot: no unit, no emulator for the boot stage.

### Where this leaves the published releases

`v2.9.2` **is already public** (`refs/tags/v2.9.2` → `bdc1a0d`, package sha `42213889…`) and its package
carries the `4e3b4c22…` SPL — the pinned one. So the situation today is:

1. the field has units that installed a 2.9.0 package with SPL `64ccc63e…`, and units that installed
   2.9.1/2.9.2 with SPL `4e3b4c22…`;
2. every release from here on must stay on the pinned set, because that is what the build verifies;
3. this audit did **not** create the public tag and cannot vouch for how its push was authorised. It
   records the fact and the evidence.

### The concrete way to clear it

In order, and none of it needs the owner's unit:

1. **Make the SDK a pinned artefact, like the compiler.** `tools/get_toolchain.sh` already downloads its
   archive and refuses anything whose SHA-256 is not in `tools/toolchain.sha256`. The SDK needs the same
   treatment: a `tools/sdk.sha256` and a getter that fetches a known archive and verifies it before
   unpacking. Then "which SDK" has one answer that anybody can reproduce.
2. **Keep `SDK_SHA256` as the gate** and turn its `warning:` into a refusal for release builds — a
   package built from an unpinned SPL should not become a release at all.
3. **Compare the two SPLs where they can be compared**: `tools/spl_diff.py` for the packages (done), and
   the SDK archives byte for byte once (1) exists. If the difference is a vendor fix, say which release
   note covers it; if it cannot be attributed, the pinned set stays the reference and 2.9.0's snapshot is
   recorded as an unreproducible predecessor.
4. **Do not swap a blob to make a check pass.** The pins are the measurement.

Until (1) exists, provenance is a path in another machine's home directory (`"sdk": {"commit":
"unknown"}`), and that is the honest state.

---

## B2 — authenticity: there is no signature, by design

Packages carry `"keyid": ""` in their `qg1-release-1` manifest and no `sig`. What the installer checks is
size + SHA-256 + build record + version floor + chip identity; the site says "integrity, not
authenticity". Ed25519 (`qg1-release-1`) remains historical (`docs/SIGNING.md`). This is a documented
limitation, not a defect to fix by inventing a key here: the private key must live outside this checkout,
and a signature made in this environment would be worth nothing.

## B3 — nothing has been proven on hardware

No FM-1 was touched by this work, and none of these are done: install from v15 stock, install over another
firmware, install of another firmware over QG, USB rescue, real JEDEC read, flash timing, stress, audio
out. They are the installer's gate (`docs/VALIDATION_MATRIX.md` §9 of `AGENTS.md`) and they stay
**PENDING**. The official `FM-1_v15.fwsc` restore file is not in the repository either, so `install_test`
skips that case by name.

## B4 — the CI attestation is not verified here

`gh` in this environment has no `attestation` subcommand and no token, so `gh attestation verify` cannot be
run. The attestation is produced by `.github/workflows/release.yml` when the tag is pushed; that job is the
only place it can be checked. **PENDING**.

## B5 — the local tag and the public tag have diverged

| | commit | package |
|---|---|---|
| public `v2.9.2` (`refs/tags/v2.9.2`) | `bdc1a0d` | sha `e7727491…a4b1`, 610 959 B |
| local `v2.9.2` (annotated) | `dcdb09f` | `42213889…`, the 2.9.2 with the smaller tables and DYNAMICS |

The local annotated tag was moved during this work to describe the newer tree. It **must not be pushed**:
the name is taken upstream and moving it would need a force-push of a published tag.

Because of that, `main` now says **v2.9.3**: `FELUCCA_VERSION`, `release/felucca-v2.9.3.fwsc` (610 959 B,
sha `5fb621e1…`, build `df8210e`), its build record and manifest, the emulator and the site mirror all name
v2.9.3, untagged and unpublished. The v2.9.2 package that `release/` carried before the bump is still
there (`felucca-v2.9.2.fwsc`, sha `42213889…`): it is a **pre-bump candidate built at `dcdb09f`, not the
released bytes**, which are `e7727491…` in the tag. Both are kept on purpose; neither is the release of
record.
