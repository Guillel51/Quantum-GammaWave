# Quantum GammaWave — provenance of every distributed audio and binary asset

Every file in this repository that is not source code, where it comes from, who made it, under which
licence it travels, and how that was checked. Written for The Firmware Hub's review and for anyone
redistributing a package.

Checked on 2026-11-12 against the tree at the commit that carries this file. **Nothing here is
reconstructed from memory**: each row names the file, the tool that produced or reads it, and the check
that was run. Where a claim could not be verified it says so and is not counted as verified.

Method: `git ls-files` (tracked files only — a build product that is not committed cannot ship) filtered
by extension, then each class read down to its origin.

```
$ git ls-files | grep -icE '\.(png|jpe?g|webp|ttf|otf|woff2?|wasm|syx|wav|fwsc|bin)$'
341
$ ... | awk -F. '{print tolower($NF)}' | sort | uniq -c | sort -rn
    303 png
     26 fwsc
      8 ttf
      2 woff
      1 wasm
      1 syx
```

---

## 1. Audio: there is no audio file in this repository

```
$ find . -path ./build -prune -o -type f \( -name '*.wav' -o -name '*.mp3' -o -name '*.ogg' -o -name '*.flac' \) -print | wc -l
0
```

Every sound the instrument makes is synthesised at run time: oscillators, FM operators, wavetables
computed by `tools/gen_tables.py` into `build/gen/*.h`, and drums that are **modelled** (the DRUM-X kit
in `firmware/src/eng_drum.c`, eight lanes of synthesised kick/snare/tom/rim/clap/hats), not played back.
The engines that would have needed sample memory are **retired and marked reserved** in
`firmware/src/engines.c` (slots 4 SAMPLE, 8 GRAIN, 13 SLICE), so no code path reads sample data from
flash.

Consequence for the Hub: a package carries no third-party recordings, loops, kits or ROM samples, and
there is no sample licence to review.

## 2. The one distributed patch asset: `web/banks/gw1-keys.syx`

| | |
|---|---|
| File | `web/banks/gw1-keys.syx`, 4 104 bytes |
| What it is | a 32-voice Yamaha-style DX7 SysEx bank: this firmware's 11 FM6 KEYS voices (`SOFT EP` … `CELESTA`, `firmware/src/eng_fm6.c`) and the INIT voice in the slots left |
| Author | this project (the voices are its own FM6 presets) |
| Origin | **generated**, not collected: `python3 tools/gen_fm6_patches.py --syx <path>` packs the same table the firmware's FM6 presets come from |
| Licence | GPL-3.0-only, like the rest of the tree |
| Permission | yes: it is this project's own data, and the file format is Yamaha's published SysEx layout (a protocol, not licensed content) |
| Use | the web editor's SEND BANK button and any six-operator FM synth a user wants to import it into |
| Verified | the committed file **regenerates byte for byte** from the generator today: |

```
$ python3 tools/gen_fm6_patches.py --syx /tmp/gw1-keys.syx
fm6 bank: 11 voices -> /tmp/gw1-keys.syx (4104 bytes)
$ sha256sum /tmp/gw1-keys.syx web/banks/gw1-keys.syx
7c39bf4b92d19b2f4afdcfe1e742febe3a3c6bb1f491a8af5285d6508f47b216  /tmp/gw1-keys.syx
7c39bf4b92d19b2f4afdcfe1e742febe3a3c6bb1f491a8af5285d6508f47b216  web/banks/gw1-keys.syx
```

No Yamaha ROM patch, no DX7 factory bank and no third-party `.syx` is included.

## 3. Fonts

All four families are under the SIL Open Font License 1.1 or MIT, with their licence texts committed;
the firmware **rasterises** them at build time (`tools/gen_aa_font.py`), so the generated tables ship and
the font files themselves are not part of a package.

| File(s) | Author | Licence | Text in the tree | Used by |
|---|---|---|---|---|
| `assets/fonts/ChakraPetch-{Bold,Medium,SemiBold}.ttf` | The Chakra Petch Project Authors | SIL OFL 1.1 | `LICENSES/OFL-ChakraPetch.txt`, `assets/fonts/OFL-ChakraPetch.txt` | the UI's labels (rasterised) |
| `assets/fonts/IBMPlexMono-SemiBold.ttf` | IBM Corp. | SIL OFL 1.1 | `LICENSES/OFL-IBMPlexMono.txt` | the UI's values (rasterised) |
| `assets/fonts/Michroma-Regular.ttf` | The Michroma Project Authors | SIL OFL 1.1 | `LICENSES/OFL-Michroma.txt` | big titles (rasterised) |
| `assets/fonts/InterTight[wght].ttf` | The Inter Project Authors | SIL OFL 1.1 | `LICENSES/OFL-InterTight.txt`, `assets/fonts/OFL.txt` | the `inter-tight` preset (rasterised) |
| `web/emu/fonts/DotGothic16-subset.woff` | The DotGothic16 Project Authors | SIL OFL 1.1 | `LICENSES/OFL-DotGothic16.txt`, `web/emu/fonts/OFL.txt` | the browser emulator's knob labels (served with its page) |
| `web/fukiai.ttf` | Leo Kuroshita / Hügelton Instruments | MIT | `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` | the firmware's icons (`tools/gen_aa_icons.py`) and the web editor's |

The reservation notices are kept as the OFL requires; no font is offered for download as a font from
this site.

## 4. Interface art and shapes

| File(s) | Author | Licence | Verified |
|---|---|---|---|
| `assets/ui-art/art/*` (20 panels), `assets/ui-art/README.md` | JIANT FM's interface art, by JIANT's author (Juan Jiant) | **GPL-3.0-only**, like JIANT | named in `LICENSING.md` under "What is GPL-3.0-only" |
| `assets/ui-shapes/plates.svg` | same tree | GPL-3.0-only | same |

These are drawn assets from the upstream lineage, redistributed under the same licence as the code that
uses them, with the credit line kept in `LICENSING.md`, the in-app credits (`firmware/src/ui_menu.c`) and
the README.

## 5. Screens and captures (all produced here)

| Directory | Count / size | What it is |
|---|---|---|
| `assets/screenshots/` | 34 files, part of `assets/` 2.4 MB | 240 × 240 renders of the firmware's own screens |
| `assets/branding/` | 2 files | this project's banner, drawn for it |
| `assets/icons.json` | 1 file | the icon map used by the generator |
| `artifacts/` | 147 files, 31 MB | audit captures (emulator and site) from the phases that produced them |
| `.shots/` | 9 files, 72 KB | screenshots taken while designing the screens |
| `docs/**/*.png` | part of `docs/` 5.5 MB | figures embedded in the audit documents |

All of them are **renders of this firmware's own graphics**, produced by the firmware's drawing code:

> "Las 20 pantallas de referencia … son **renders reales píxel a píxel a 240 × 240** generados
> directamente desde el motor gráfico embebido del firmware Quantum GammaWave (QG-1). No son fotografías
> de una unidad física; la galería completa se regeneró en esta rama con `tools/generate_screenshots.py`."
> — `assets/ui-art/README.md`

No photograph of a device, no M-VAVE artwork, no product shot and no third-party illustration is
distributed. The card picture the Hub shows (`assets/screenshots/quantum_01_home.png`) is one of these
240 × 240 renders.

## 6. Firmware packages (`.fwsc`) and the emulator build

| File(s) | What | Author / licence | Notes |
|---|---|---|---|
| `release/*.fwsc` (25 files, 15 MB) | earlier QG packages, `v2.0.0` … `v2.7.4.3` | this project, GPL-3.0-only | each identity is `FM-1_965`; kept as the project's own release history |
| the package on a release (today `felucca-v2.8.4.fwsc`) | the firmware as the Hub serves it | this project, GPL-3.0-only | 610 959 B, identity `FM-1_965`, loader `ota-FM-1_965` (`tools/check_package_identity.py`) |
| `tests/fixtures/felucca-v1.9.0.fwsc` | **upstream Felucca's** package, used as a test donor | Hügelton Instruments, GPL-3.0 | test input only: it is never published as this project's package, and `docs/IDENTITY.md` records the identity it carries |
| `site/emulator/felucca.wasm` | the browser emulator, built from `web/emu/` in this tree | this project, GPL-3.0-only | a build product; rebuild with `sh web/emu/build.sh` (Emscripten 6.0.11). Reproducibility is asserted by `release/build-manifest-*.json` (it records the emulator's hash) |

The `.fwsc` **asset name** still carries `felucca-`, inherited from `tools/build.py`; the identity inside
is this project's own. Why it was kept is written down in `docs/FM1_HUB_READINESS.md` §2.1.

## 7. Code that keeps someone else's licence

Stated here because a package is a derivative work, and the Hub asks for a redistributable licence:

| Part | Origin | Licence | Text |
|---|---|---|---|
| the whole firmware | derivative of Felucca by Leo Kuroshita (Hügelton Instruments) via JIANT FM, plus GW-1's additions | GPL-3.0-only | `LICENSE`, `LICENSING.md` |
| `firmware/src/fm6_core.c` | msfa (the six-operator FM core) | Apache-2.0 | `LICENSES/Apache-2.0-msfa.txt` |
| the JIANT lineage (sequencer, song chain, generator, screens) | JIANT FM by Juan Jiant | GPL-3.0-only | per-file SPDX headers and `docs/JIANT_FEATURE_MATRIX.md` |
| SLOOP's arranger structures (`firmware/src/song_chain.c`) | SLOOP by isod89 | GPL-3.0-only | cited in the file header |
| the loader | Felucca's update loader | GPL-3.0-only | `firmware/loader/` |

The GPL-3.0-only whole is compatible with redistribution through the Hub: the Hub mirrors a package
**with its source**, which is exactly what the licence requires.

## 8. What is not here, and why it matters

* **No samples, no ROM waves from another instrument, no factory patch banks** from Yamaha, Roland,
  Korg, Casio or anyone else. The FM6 preset table is this project's own, and the one `.syx` is
  generated from it (§2).
* **No audio recordings**: no demo tracks, no loops, no impulse responses. The reverb is four combs and
  two all-passes of numbers (`firmware/src/fx.c`), not a recorded space.
* **No third-party binary without its licence and its text** in `LICENSES/` (§3, §7).

## 9. How to re-check all of it

```bash
git ls-files | grep -iE '\.(png|jpe?g|webp|ttf|otf|woff2?|wasm|syx|wav|fwsc|bin)$'   # the inventory
python3 tools/gen_fm6_patches.py --syx /tmp/b.syx && cmp /tmp/b.syx web/banks/gw1-keys.syx  # §2
python3 tools/check_package_identity.py <a release>.fwsc                                   # §6
python3 tools/check_fmhub.py                                                              # the card
grep -rn "OFL\|MIT\|Apache\|GPL" LICENSING.md                                             # §3, §7
```
