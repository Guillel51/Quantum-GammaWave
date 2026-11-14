# Quantum GammaWave — readiness for The Firmware Hub

What the Hub requires, what this repository has, and what is still missing. Every line says how it was
checked and of which class: **read from source**, **compiled**, **measured on host x86**, **read from the
published bytes/network**, or **PENDING** (needs a physical FM-1 and was not done here).

**Since this checklist was first written, `v2.9.0` shipped** (commit `0f40f7c`): the numbers below that
name the published package now read `v2.9.0`, `610959 B`, sha256 `edd2a23f…`; the v2.8.4 figures stay
where they describe what was checked at the time. See `docs/FINAL_RELEASE_SYNC.md`.

Checked on 2026-11-12 against `https://thefirmwarehub.com/kb/` (the "Getting your firmware listed",
"How to sign your release", "The identity" and "Your card's text" sections) and
`https://thefirmwarehub.com/catalog.json` (`updated 2026-11-12 18:01 UTC`, **198 listings**).

> The Hub's own rules are the authority. Where this document and the KB could disagree, the KB wins and
> this file is wrong until it is fixed.

### Re-checked on 2026-11-13 (phase 6)

Nothing in the Hub's requirements changed since 2026-11-12, so the table below still stands. What moved is
on our side:

| Item | State today | How it was checked |
|---|---|---|
| Live catalogue | 198 listings, `updated 2026-11-13 07:05 UTC` | `tools/check_identity.py` fetched it |
| `FM-1_965` | **FREE** — no listing's product or answered range covers it | same tool, on the live catalogue |
| `fmhub.json` | **GO** — every published rule holds | `python3 tools/check_fmhub.py` (all `ok`) |
| Package | `release/felucca-v2.9.1.fwsc`, **610 959 B**, sha256 `75e742fd…`, identity `FM-1_965` — **built, not published** (see `docs/QG_EVOLUTION_FINAL_REPORT.md` §8) | rebuilt after the F-1 fix; `tools/check_package_identity.py` reads the bytes: `ok`, loader `ota-FM-1_965` |
| Application image | **580 404 B** in a **583 612 B** slot, **3 208 B spare** (1 160 before the slot grew 2 KiB) | AC791N link |
| Release workflow | pinned to `v1`'s commit `85a003a…`, `tests-block: true`, separate `verify` job | read from `.github/workflows/release.yml` |
| Three installs on a real FM-1 | **PENDING** | no lab hardware; the owner's unit is not written (§7) |
| Listed or accepted by the Hub | **not claimed** — no application found, nothing sent | catalogue, by name, repository and identity |

The F-1 fix (a divide by zero in `voice_alloc`, see `docs/QG_EVOLUTION_PHASE_6_SECURITY.md`) is why the
package's sha256 differs from the v2.9.0 one named above: **the published release was not rewritten**. A
new package only reaches the Hub from a new tag, which is the owner's call.

### Re-checked on 2026-11-13 (phase 4)

The card had regressed after the phase-6 check: `9df4067` added `"sounds": "none"`, which the Hub does not
accept, and **one unknown field discards the whole file** (KB read again on 2026-11-13). The field was
removed. `python3 tools/check_fmhub.py` now passes every rule, live links included (200). The live catalogue
says **202 listings**, updated 2026-11-13 18:01 UTC, and `FM-1_965` is still **free**. The current package is
`release/felucca-v2.9.7.fwsc` (610 959 B, sha256 `8af698bf…a5d0`), identity `FM-1_965`, loader
`ota-FM-1_965`. `SHA256SUMS` was stale at v2.9.1 and now names the v2.9.7 artifacts. This is still **not** a
Hub listing or an acceptance, and no physical install was performed. The full audit is in
`docs/QG_PHASE4.md`.

Attestation is still not security: the badge says GitHub built the file from that commit. It says nothing
about whether the firmware is safe, and this document does not claim otherwise.

---

## 0. The short version

| Requirement | State | Evidence |
|---|---|---|
| It makes music | **GO** | six tracks, nine live engines, sequencer, song mode, FX |
| Public source, redistributable licence | **GO** | public repository, `GPL-3.0-only` (`LICENSE`, `LICENSING.md`) |
| A `.fwsc` on a GitHub release | **GO** | `v2.9.0` (and `v2.8.4` before it), asset `felucca-v2.9.0.fwsc`, 610 959 B |
| Its own `FM-1_` identity, kept in the package | **GO** | `FM-1_965` read from the published bytes; free in the live catalogue |
| The update loader | **GO** | `firmware/loader/loader.c` answers `ota-FM-1_965`; resumable `FM-1_9NN` form |
| Both flash parts accepted (app and loader) | **GO (compiled)** | `0x856014` Puya and `0xB36014` UCUN in both, one allowlist |
| Built from source on GitHub (attested) | **GO CONDITIONAL** | `.github/workflows/release.yml` at the workflow's reviewed `v1` commit; the badge needs the next tag to run |
| The attestation checked by us (`gh attestation verify`) | **PENDING** | `gh` here has no `attestation` subcommand and no token; the check is now a CI job on every tag (§5) |
| `fmhub.json` card file | **GO** | `python3 tools/check_fmhub.py`, every rule holds (§3) |
| Install from V15, install from another firmware, leave again, USB identity, interrupted update | **PENDING** | needs a real FM-1; the owner's unit is never written (§7) |
| An existing application for this repository | **none found** | not in the live catalogue by name, repository or identity (§1) |

**Not claimed anywhere in this document:** that the Hub has accepted or listed Quantum GammaWave, that
the firmware is safe, or that anything works on hardware.

---

## 1. The submission: is there already one?

The Hub has no application queue that a repository can read; a submission is an email to
`zven@thefirmwarehub.com` and a listing appears afterwards. What can be checked from here is the
**catalogue**, which is what the Hub actually serves:

```
$ curl -sS https://thefirmwarehub.com/catalog.json | python3 -c "..."
listings: 198   updated: 2026-11-12 18:01 UTC
name contains "Quantum"   : none
repo contains "Guillel51" : none
ident contains "965"      : none
```

So at the time of writing **there is no listing for this repository**, and sending the message in §8
would not duplicate an existing card. Two related facts a reviewer will ask about:

* **`Pocket GammaWave`** (`id: gammawave`, author *ShadowTune (GammaWave Audio)*, repo
  `fvalente/GammaWave-FM1`, identity `FM-1_981`, version `1.2.1-GW`, added 2026-10-16) is **a different
  firmware**: it is the upstream project this one derives from, by a different author. The names are
  close; the repository, the identity and the code are not. The message says so in its first line.
* This repository has **no record of an earlier submission** (no `docs/` entry, no template, no saved
  email). If an email was sent outside the repository, only the sender's mailbox can confirm it — the
  Hub does not expose a status to the public.

---

## 2. Identity: `FM-1_965` (§4 of the phase brief)

| What | Value | How it was checked |
|---|---|---|
| Package identity | `FM-1_965` | read from the **published** bytes (`v2.9.0`: `edd2a23f…`; `v2.8.4`: `37f7276e…`) with `tools/check_package_identity.py` |
| Loader identity | `ota-FM-1_965` | the same tool finds it inside the package; `firmware/loader/loader.c` |
| App identity over USB (SysEx `0x11`) | `FM-1_965` | `firmware/src/felucca.c` `FELUCCA_ID`, `tests/ota_test.c` |
| Taken on the Hub? | **free** | `python3 tools/check_identity.py` against the live catalogue (exit 0) |
| Not M-VAVE's V15 | ✓ | the tool refuses `FM-1_015` by name; the live catalogue says not to use it |
| Resumable form | ✓ | three digits after `FM-1_9`, which is the family the Felucca loader resumes |

```
$ python3 tools/check_package_identity.py /tmp/qg.fwsc
package  /tmp/qg.fwsc  610959 B
sha256   37f7276e27f06545e6c2508c2fbf43d6efbd8483694e9a41bebbc42ea7b9e111
identity 'FM-1_965'
ok    this is FM-1_965's own identity, not another firmware's
loader   ota-FM-1_965 (the identity the update loader answers)
```

The package was downloaded from
`https://github.com/Guillel51/Quantum-GammaWave/releases/download/v2.8.4/felucca-v2.8.4.fwsc`
(a public URL, no token) at 19:05 UTC on 2026-11-12.

### 2.1 Two inherited names, left alone on purpose

1. **`firmware/src/ota.c:392`** writes `"ota-FM-1_015"` into the `UPDATA_PARM` it hands to the boot ROM,
   under a comment saying it is the loader's own USB identity, while the loader itself
   (`firmware/loader/loader.c`) answers **`ota-FM-1_965`**. This is inherited Felucca code, and the
   published package does **not** carry the `015` string at all (the tool above finds only
   `ota-FM-1_965`). Changing a value the closed boot ROM interprets, with no unit and no SDK
   documentation to read the field against, is a guess: it is **reported here, not changed**. What
   would settle it: a unit in update mode answering the SysEx identity, or JieLi's `updata_parm` spec.
2. **The asset is named `felucca-*.fwsc`** (`tools/build.py`, `tools/coherence_check.py`,
   `tools/fm1_sign.py`, the site and several tests all key on that name). The *identity inside* is
   Quantum GammaWave's own; the file name is a legacy of the upstream tree. Renaming it would touch the
   build, the manifest, the signing tool, the site and the installer in one commit for a cosmetic gain,
   and the Hub does not care about the file name (it takes the release's `.fwsc`, and lets a project
   name which one if there are several). **Decision: keep the name**, say it in the message to the Hub.

---

## 3. The card file: `fmhub.json`

> **Schema note (2026-11-13):** the Hub accepts `what`, `author`, `kinds`, `play`, `editor`, `cheat`,
> `image` and `color`; it ignores the **whole file** if one field is unknown. `sounds` is **not** a
> field. The sound count belongs inside `what`, which now says “nine engines and 275 sounds”. The
> card is checked by `python3 tools/check_fmhub.py`.

The Hub reads it from the default branch, every three hours, and **ignores the whole file** when one
field is unknown or out of bounds. `tools/check_fmhub.py` (new) checks it against the published rules
and the live examples of other firmwares' files:

```
$ python3 tools/check_fmhub.py
ok    fmhub.json is valid JSON
ok    only accepted fields (author, cheat, color, editor, image, kinds, play, what)
ok    what: 274 characters, plain text, no links
ok    author: Guille Lauría
ok    kinds: ['groove']
ok    play: https://guillel51.github.io/Quantum-GammaWave/emulator/ answers 200
ok    editor: https://guillel51.github.io/Quantum-GammaWave/webapp/editor/ answers 200
ok    cheat: https://github.com/Guillel51/Quantum-GammaWave#readme answers 200
ok    image: docs/card.png 480x480 PNG, the Solar Heat palette, under 300 KB
      #35CFFF: luminance 0.526; contrast on black 11.52:1, on white 1.82:1; dark ink #101014 on it 10.41:1
ok    color: #35CFFF, bright, 11.5:1 on a dark card, readable ink #101014 (10.4:1)
```

Notes:

* **The text was corrected, not just re-coloured.** The previous `what` promised "a sampler with mic
  input": the SAMPLE, GRAIN and SLICE engines are **retired** (`firmware/src/engines.c` marks slots 4, 8
  and 13 reserved) and there is no audio input path. The text now lists what the card's firmware really
  has: nine engines (analog, six-operator FM, wavetable, digital drums…), a 64-step sequencer with
  probability and ratchets, song mode, scales and chords, a mod matrix and three FX buses, plus the
  browser emulator and the web editor.
* **`color` is `#35CFFF`** (the brief's colour). It is unique among the 52 coloured listings in the live
  catalogue; the two nearest are `#2BA6E8` (Tuna) and `#3FA2E6` (Flyfish). Luminance 0.526 makes it a
  bright accent, not a panel: 11.5:1 on a dark card. Any text drawn **on** the colour must be dark
  (`#101014`, 10.4:1) — white on it is 1.8:1 and would fail.
* **Image:** `docs/card.png`, 512 × 512, PNG, 20 860 B — square, in range, well under 300 KB. It is a
  generated card from the repository, not a photograph. See `docs/FM1_HUB_AUDIO_PROVENANCE.md`.
* The Hub takes the name, author, picture and version from the repository and the releases; only the text,
  kind, links, picture *path* and colour come from this file.

---

## 4. Package, loader and flash

| Check | Result | Class |
|---|---|---|
| Package is an FM-1 package with an update loader | yes: `v2.8.4` from the release | read from the published bytes |
| Identity in the first 20 blocks | `FM-1_965` | read from the published bytes |
| Loader identity | `ota-FM-1_965` | read from the published bytes + source |
| Size against the FM-1's 1 MiB of SPI NOR | 610 959 B | measured |
| JEDEC allowlist, app | `0x856014` Puya P25Q80H, `0xB36014` UCUN UC25HQ80 | read from source (`firmware/hal/fm1_flash_id.h`, `firmware/src/ota.c`) |
| JEDEC allowlist, loader | same two, same header, `ldr_flash_known()` | read from source (`firmware/loader/loader.c`) |
| Unknown JEDEC | refused | `tests/ldr_test.c`, `tests/ota_test.c`, `web/test_update_mode.mjs` |
| Real parts read from a unit | **not done** | PENDING (§7) |
| Package SHA-256 vs the release's own record | `37f7276e…` published; the release's build manifest is the authority | measured |
| `SHA256SUMS` in the repository | **stale**: it lists `release/felucca-2.7.3.fwsc` and the emulator's wasm only, not `v2.8.4` | read |

`SHA256SUMS` is not what the Hub checks (it mirrors the release asset and pins its own copy), but a stale
integrity file next to a newer release is a trap for a human: the authority for a release is its
**build manifest** (`release/build-manifest-*.json`) plus the **attestation**, and `SHA256SUMS` should
either be regenerated by the release build or removed. Recorded as debt, not changed here (it is
generated by the build, and hand-editing an integrity file is worse than leaving it obviously stale).

---

## 5. Built from source on GitHub (§6–§7 of the brief)

`.github/workflows/release.yml` calls the Hub's shared workflow:

```yaml
uses: zvenson/fm1-build/.github/workflows/build.yml@85a003ab455b1c074b0bd6d7c282c096a464b973
```

and that commit **is** the reviewed tag `v1`:

```
$ git ls-remote https://github.com/zvenson/fm1-build 'refs/tags/*'      # 2026-11-12
5ba8c359b1744cc53c4530b15eff52d0d5c1255b    refs/tags/v1        (annotated tag)
85a003ab455b1c074b0bd6d7c282c096a464b973    refs/tags/v1^{}     (the commit it points at)
```

The job names the commit rather than the tag because a tag is a mutable ref in someone else's
repository and this job receives `contents: write` and `id-token: write`. Publishing from that sha is
publishing from `v1` as reviewed, and it is what the previous phases shipped.

Permissions are `contents: write`, `id-token: write`, `attestations: write` for the build, and
`contents: read`, `attestations: read` for the verification job. Tests are blocking
(`tests-block: true`) and the command is the real one:

```
NO_PACKAGE=1 sh tests/run_tests.sh && python3 tests/install_test.py && node web/test_web.mjs \
  && node web/test_backup.mjs && node web/test_pkg_bounds.mjs && node web/test_provenance.mjs
```

Packages are `build/*.fwsc` — exactly the path `./build.sh --release` writes.

**New in this phase:** a `verify` job that runs on every `v*` tag after the build and checks the exact
bytes the build attested, without rebuilding or re-uploading anything:

```
gh release download "$GITHUB_REF_NAME" --pattern '*.fwsc' --dir dist
gh attestation verify dist/*.fwsc --repo "$GITHUB_REPOSITORY" \
  --signer-workflow zvenson/fm1-build/.github/workflows/build.yml
```

That job is the automated form of §7 of the brief. **It has not run**: it runs on the next tag. Here,
locally, the same command cannot be used — the `gh` client installed in this environment has no
`attestation` subcommand and there is no authenticated token, and a token is never written into the
repository or the chat. **State: PENDING, with the mechanism in place.**

---

## 5b. The v2.9.2 tag, prepared and not pushed (2026-11-13)

`v2.9.2` exists as an **annotated tag in this working tree only** (`git tag -a`, object pointing at the
release commit on `main`). Pushing it is the whole act of publishing: it is what runs `release.yml`, which
builds, attests and uploads through the Hub's reviewed shared workflow. Nothing was pushed and no release
was created; the tag waits for the owner's word.

What was checked locally, before any push, on the bytes that tag would ship:

| Check | Result | Class |
|---|---|---|
| `python3 tools/check_identity.py` | `FM-1_965` **FREE**: catalogue fetched now (`updated 2026-11-13 09:55 UTC`, 198 listings), no listing's product or answered range covers it | live HTTP + the Hub's own JSON |
| `python3 tools/check_fmhub.py` | every published rule holds; `play` / `editor` / `cheat` answer 200; image 240x240 PNG, 15 407 B; `#35CFFF` 11.5:1 on a dark card | live HTTP |
| `python3 tools/check_package_identity.py release/felucca-v2.9.2.fwsc` | `FM-1_965`, loader `ota-FM-1_965`, 610 959 B, sha256 `28a78359a9e1ee26…` | read from the package's bytes |
| `python3 tools/coherence_check.py` | package, release manifest, site and emulator record agree | host |
| `gh attestation verify` on those bytes | **not run and not runnable here**: no tag pushed (nothing is attested yet), and this `gh` has neither the `attestation` subcommand nor a token | PENDING |

The attestation is not something this machine can produce *or* read: it is created by the tag's workflow
and verified by the `verify` job on that same tag. So "the attested package checked against the Hub
catalogue" splits in two, and only the first half is done: the **candidate** is verified against the
catalogue now, and the **attested bytes** can only be checked after the tag is pushed -- the `verify` job
downloads the release asset and runs `gh attestation verify` against the reviewed workflow sha.

## 6. What was run, on 2026-11-12, on this tree

| Command | Result |
|---|---|
| `python3 tools/check_fmhub.py` | every published rule holds (output in §3) |
| `python3 tools/check_identity.py` | `FM-1_965` FREE against 198 listings |
| `python3 tools/check_package_identity.py` on the published `v2.8.4` | `FM-1_965`, loader `ota-FM-1_965`, 610 959 B |
| `python3 tools/gen_fm6_patches.py --syx` vs `web/banks/gw1-keys.syx` | byte-identical (`7c39bf4b…`): the distributed bank regenerates |
| `python3 tests/install_test.py` | install, resume and refusals: passed (skips that need a package or V15 are named in its log) |
| `node web/test_pkg_bounds.mjs` | package bounds: all checks passed |
| `node web/test_provenance.mjs` | provenance: passed |
| `python3 tests/gate_test.py` | gate: passed |
| `NO_PACKAGE=1 ./tests/run_tests.sh` (this session) | `ALL HOST TESTS PASSED` — 192 screens × 22 palettes with 0 lint findings, 0 misalignments in 3 164 009 measurements, 230 golden renders 0 changed, 202 presets 0 silent/duplicated |
| `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/release.yml'))"` | parses; jobs `build`, `verify` |

All of it is **host x86**: a passing test proves host behaviour, not device behaviour.

---

## 7. PENDING: the four checks only a real FM-1 can settle

The Hub lists a firmware only after its author has done these, and each one writes to a unit. **The
owner's FM-1 is never written from this repository or by an agent working in it**, so they stay pending
here — not "not applicable", not "passed".

| # | Check | State |
|---|---|---|
| 1 | Install it **from the official V15**; the unit ends up running QG and the Hub names it after Connect | **PENDING** |
| 2 | Install it **from another custom firmware** (and check that QG's own loader was the one that wrote) | **PENDING** |
| 3 | **Leave it again**: with QG running, install another firmware; QG must hand over to the next package's loader | **PENDING** |
| 4 | **USB**: shows up as a MIDI device, answers the identity request with `FM-1_965`, port name known to the Hub | **PENDING** |
| 5 | **Interrupted update**: unplug once during Writing, plug back in, press Install; the update continues | **PENDING** |

What exists as software evidence for each: the installer's V15/other/legacy paths and the loader handoff
are covered on host by `tests/install_test.py`, `web/test_update_mode.mjs`, `web/test_verify_only.mjs`
(153 checks) and `tests/gate_test.py`; the identity frame by `tests/ota_test.c`; the resume logic by
`web/fm1ota.js` and its tests. None of that is a device test, and this document does not present it as
one.

---

## 8. The message for Sven

Short, because the hub writes the card from `fmhub.json`; it says what the Hub needs and nothing it does
not.

```
Subject: FM-1 Firmware Hub: Quantum GammaWave

Hi Sven,

I would like to have Quantum GammaWave listed on the hub.

Repository: https://github.com/Guillel51/Quantum-GammaWave
Version:    2.9.0 (release v2.9.0, asset felucca-v2.9.0.fwsc, 610959 bytes,
            sha256 edd2a23f59d513c20c286f11b0f55d0b75519fe7a24afe63bba1dd7334b4ac1b)
Identity:   FM-1_965, loader ota-FM-1_965. Checked free against your catalogue today
            (198 listings); the asset name is inherited from the Felucca tree, the
            identity inside is this firmware's own.
Licence:    GPL-3.0-only, complete source in the repository. Third-party parts and
            every distributed binary are listed in LICENSING.md and in
            docs/FM1_HUB_AUDIO_PROVENANCE.md: OFL/MIT fonts, JIANT's interface art in
            GPL-3.0, and no sample or patch data from anywhere else. The one audio
            asset (web/banks/gw1-keys.syx) is generated from this firmware's own FM6
            presets and regenerates byte for byte.
Loader:     Felucca's loader, so an interrupted update resumes (FM-1_9NN).
Flash:      both documented parts are accepted by the app and by the loader:
            0x856014 (Puya P25Q80H) and 0xB36014 (UCUN UC25HQ80). Both are compiled in
            and unit-tested; no part has been read from a unit by me.
Build:      .github/workflows/release.yml, the shared workflow at v1's commit
            (85a003ab). Tags v2.8.3 and v2.8.4 were published by GitHub Actions from
            that workflow; whether each package carries a valid attestation is what the
            workflow's verify job checks now, on every tag.
Card:       fmhub.json in the repository root (text, kind groove, play, editor, cheat,
            240x240 PNG, colour #35CFFF), checked against your rules with
            tools/check_fmhub.py.
One thing to know: this firmware is a derivative of Pocket GammaWave (fvalente /
ShadowTune, also on your hub as FM-1_981) by way of JIANT FM and Felucca, with a
different author, repository and identity.

Limitations, stated plainly: no physical unit here has been flashed by me. The four
device tests your page asks for (install from V15, install from another firmware,
leaving again, USB identity) and the interrupted-update test are still to be done on a
unit; everything above them is host-side. I will not ask you to list it before those
are done, and I will send the results when they are.

Reports: docs/FM1_HUB_READINESS.md (this checklist, with the state of every item),
docs/FM1_HUB_AUDIO_PROVENANCE.md (every distributed resource, its author and licence),
docs/IDENTITY.md (the identity and how it was checked against your catalogue),
docs/VALIDATION_MATRIX.md (per-requirement evidence for the installer).

Thanks,
Guille
```

Before sending it, two things stay open by design: the physical tests (§7) and the attestation check
(§5). The Hub's own sentence is the reason: *"Untested firmwares are not listed."*

---

## 9. Debt this document leaves open

1. **The physical tests** (§7) — the only ones that can turn the listing from conditional to done.
2. **`gh attestation verify`** run once from a machine with a current `gh` and a token, on the `v2.9.0`
   asset, or the `verify` job that runs on that tag (§5).
3. **`SHA256SUMS`** regenerated by the release build instead of being stale (§4).
4. **The asset name** `felucca-*.fwsc`, kept for compatibility (§2.1); a future rename needs one commit
   across `tools/build.py`, `coherence_check.py`, `fm1_sign.py`, the site and the tests.
5. **`ota.c`'s `UPDATA_PARM` string** `ota-FM-1_015` (§2.1) — to be settled with a unit in update mode
   or JIANT's/Felucca's protocol notes, not by guessing.
6. **The name collision to watch**: the hub already lists *Pocket GammaWave*; if it ever lists this one,
   its card must read *Quantum GammaWave* so players do not install the wrong derivative.
