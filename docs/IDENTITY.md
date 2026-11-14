# The identity: `FM-1_965`

Every FM-1 package carries a short identity that the device answers over USB and that installers and
The Firmware Hub use to tell firmwares apart. This project answered **`FM-1_982`** up to 2.1.1, and
**GuitChordi 0.1.0-beta.1 answers `FM-1_982` too** — the hub could not tell the two apart, so it could
not mark either card as *on your FM-1*. Since 2.1.2 this firmware answers and ships as **`FM-1_965`**.

| | |
|---|---|
| Previous identity | `FM-1_982` (shared with GuitChordi) |
| New identity | `FM-1_965` |
| Update loader | `ota-FM-1_965` (was `ota-FM-1_900`, the Felucca-family loader) |
| Checked against | `https://thefirmwarehub.com/catalog.json`, `updated 2026-11-10 12:27 UTC`, 175 listings |
| Result | **free**: no listing's `product` and no listing's answered range covers it |
| Tool | `python3 tools/check_identity.py` (re-runnable; exit 0 free, 1 taken, 2 could not read) |

## 1. How the check was made

The hub has **no reservation**: its catalogue is the only register, and it is what the live
"All identities on the hub" table is rendered from. Two fields claim a number there:

* `product` — the identity the project's newest package reports, exactly (`FM-1_982`);
* `ident` — a regular expression over every identity that project has answered across its releases
  (`FM-1_91\d` for Felucca, `FM-1_99[0-8]` for Dinghy, `FM-1_94[1-9]` for Bytemania). Ignoring this
  field is how a "free" number turns out to be taken: three digits are often inside someone's range.

Twenty-two listings carry `FM-1_\d{3,8}` instead, which matches every number and therefore claims none:
the hub could not read an identity off that project, and its own page ignores the pattern when a real
range matches. `tools/check_identity.py` reports those separately instead of refusing everything.

Measured on the catalogue of 2026-11-10 12:27 UTC, `tools/check_identity.py` exits 0 for thirteen
numbers of the `FM-1_9NN` block (900–999):

```
922 954 956 959 962 963 965 967 984 986 987 988 989
```

Five of them sit inside a longer range another project already uses — `FM-1_922\d{5}` (BB1), `FM-1_9620`
(ARTIHIVE), `FM-1_963\d{5}` (Scintilla), `FM-1_984\d` (VIBE FM Link), `FM-1_987\d{6}` (X0X-RMX) — free
as three-digit numbers, but a future release of theirs would land in the same prefix. **965** was
chosen: nothing uses it as a product, nothing answers a range that covers it, and no identity on the
hub starts with `965`. Of the rest, **954, 956, 959, 988 and 989** were equally clear at the time, and
`986` was left alone because TerkWerX's Crepuscular Homunculus documents that number for itself
although it is not listed yet.

```
$ python3 tools/check_identity.py
identity  FM-1_965
catalogue https://thefirmwarehub.com/catalog.json (fetched now), updated 2026-11-10 12:27 UTC, 175 listing(s)
FREE      no listing's product and no listing's answered range covers it
          (22 listing(s) carry no identity of their own; they match every number and claim none)
          (the hub reserves nothing: this is what its catalogue says now, not a promise)
```

And the number it replaces, for contrast:

```
$ python3 tools/check_identity.py FM-1_982
TAKEN     claimed by:
          GuitChordi: product FM-1_982
```

## 2. The rules this identity obeys

From the hub's knowledge base (https://thefirmwarehub.com/kb/, read 2026-11-10) and this repository's
own tools:

* **`FM-1_` and digits.** The package carries it in the first 20 blocks, one marker byte after each
  0x30 block (`productOf` in `web/fm1pkg.js`, `tools/fm1_install.py`): at most 20 characters.
* **`FM-1_9NN` is the resumable range.** The hub's installer resumes an interrupted update only when the
  loader answers `^ota-FM-1_9\d\d$`, and this project installs only through a loader it recognises
  (`web/fm1ota.js`, `tools/fm1_install.py`). A longer identity would forfeit that, so the number is
  three digits. `tools/check_identity.py` refuses another shape unless `--any-form` says otherwise.
* **`FM-1_015` is M-VAVE's V15** and is never used for a custom package; the tool refuses it by name.
* **One number, every build.** `tools/build.py` keeps `PRODUCT` fixed across releases: the hub knows a
  project by its identity, not by its version. The version travels in `FELUCCA_VERSION` (on the splash
  and in the build record), not in the identity.

## 3. Where the identity lives

One constant, read by everything else:

| Place | What it does |
|---|---|
| `tools/build.py` `PRODUCT` | the package identity: `fm1pkg_make.ufw(..., PRODUCT)`, and `-DFELUCCA_ID` for the app |
| `firmware/src/felucca.c` `FELUCCA_ID` | the default when the app is compiled without `build.py`; answered on the SysEx 0x11 handshake |
| `firmware/loader/loader.c` `FELUCCA_ID` | `ota-FM-1_965`: what the unit answers while the update loader waits |
| `tools/package_emu.py` | reads `PRODUCT` from `tools/build.py` for the emulator's build record |
| `web/make_site.py` | reads the identity out of the package it publishes (`FM-1_9\d\d` only) |
| `web/fm1ota.js`, `web/fm1verify.js` | `isOurLoader` (965, and 900 for the packages up to 2.1.1), `LEGACY_IDS` |
| tests and fixtures | the identity in each fixture is the one that fixture's package carries |

## 4. The transition, and why it is not a loosened check

A unit still running 2.1.1 answers `FM-1_982`. Refusing it as "another firmware" would strand every
user on the old release, so:

* `web/fm1ota.js` `LEGACY_IDS = ["FM-1_982"]`: the install path accepts that identity as **this**
  firmware's. Nothing else changes — the model must still match, the 0x12 chip identification still
  runs on the app link before any write frame, and the package still has to pass its manifest.
* `web/fm1verify.js` reports it as `gammaWaveLegacy` with compatibility `upgrade-legacy` ("the same
  firmware on its previous identity: an update"), which is neither a reinstall of the same tag nor
  another build.
* `isOurLoader` accepts `ota-FM-1_965` and `ota-FM-1_900`: the second is the loader the 2.1.1 packages
  carried (an interrupted install of 2.1.1 leaves the unit in it). A loader that is neither is still
  foreign and is never written through.

The old number is not erased from history: `release/felucca-2.0.0.*`, `release/felucca-2.1.1.*`,
`tests/fixtures/felucca-v1.9.0.fwsc` and the changelog entries keep `FM-1_982`, because those are the
bytes and the releases that carried it. What changed is what *this* build reports and publishes.

## 5. What was re-measured after the change

| Check | Result |
|---|---|
| AC791N build (`./build.sh`, v3.1.1) | app 574 992 B of a 583 612 B slot, package 610 959 B, `identity FM-1_965` |
| the bytes themselves | `FM-1_965` in the app, in `build/loader/ota.bin`, and in the 20 marker bytes of the `.fwsc` |
| SysEx 0x11 (`tests/ota_test.c`) | `identity frame is FM-1_965` |
| installer (`tests/install_test.py`) | install from V15, resume, refusals: passed with the new identity |
| Verify Only (`web/test_verify_only.mjs`) | ours / legacy / stock / other / loaders: 153 checks, 0 failed |
| emulator (`web/emu/build.sh`) | WASM rebuilt; `build/emu/manifest.json` `"identity": "FM-1_965"` |
| hub | `tools/check_identity.py`: FREE (above) |

Not measured here: a physical FM-1. Nothing in this document is a claim about hardware; the identity a
real unit answers after an install is the check that only a unit can make.
