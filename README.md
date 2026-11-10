<div align="center">

![Quantum GammaWave banner](assets/branding/quantum_banner.png)

# QUANTUM GAMMAWAVE

**Twelve voices. Six tracks. Eight synthesis engines. One M-VAVE FM-1.**

Open-source firmware for the M-VAVE FM-1 groovebox, with a browser emulator, a Web Studio editor and a
USB installer that verifies what it writes.

<a href="https://guillel51.github.io/Quantum-GammaWave/"><img alt="WEB SITE" src="https://img.shields.io/badge/WEB%20SITE-QUANTUM%20GAMMAWAVE-3effff?style=for-the-badge&labelColor=000410" height="40"></a>
<a href="https://guillel51.github.io/Quantum-GammaWave/emulator/"><img alt="EMULATOR" src="https://img.shields.io/badge/EMULATOR-BROWSER-00e5a3?style=for-the-badge&labelColor=000410" height="40"></a>
<a href="https://guillel51.github.io/Quantum-GammaWave/webapp/editor/"><img alt="WEB STUDIO" src="https://img.shields.io/badge/WEB%20STUDIO-EDITOR-1f6feb?style=for-the-badge&labelColor=000410" height="40"></a>
<a href="https://guillel51.github.io/Quantum-GammaWave/webapp/installer/"><img alt="USB INSTALLER" src="https://img.shields.io/badge/USB%20INSTALLER-ff9424?style=for-the-badge&labelColor=000410" height="40"></a>

`v2.4.1` · identity `FM-1_965` · GPL-3.0-only · AC791N (JieLi WL82) · 240 × 240

</div>

---

## What it is

Quantum GammaWave replaces the stock firmware of the **M-VAVE FM-1** with an instrument: six tracks, twelve
voices and eight synthesis engines, sequenced and played entirely on the unit, with a real DSP path — no
samples, no streaming, no host in the loop.

It is a whole ecosystem built around one firmware:

| | |
|---|---|
| **Firmware** | what the FM-1 runs: engines, sequencer, effects, UI, MIDI |
| **[Web emulator](https://guillel51.github.io/Quantum-GammaWave/emulator/)** | the same sources compiled to WebAssembly: the panel, playable in a browser |
| **[Web Studio](https://guillel51.github.io/Quantum-GammaWave/webapp/editor/)** | edits the real sound over Web MIDI / SysEx, with a live hardware view |
| **[USB installer](https://guillel51.github.io/Quantum-GammaWave/webapp/installer/)** | checks SHA-256, the Ed25519 signature and the flash chip before it enables INSTALL |
| **[Site](https://guillel51.github.io/Quantum-GammaWave/)** | the portal: current screens, download, documentation, preview channel |

![HOME](web/site/assets/fw-home.png)

*HOME, rendered by the firmware's own sources at 240 × 240 (×3): the track, its four edit parameters, the
spectrum with the filter curve, the orbital system, the six track buttons and the engine/sound line.*

## Highlights of `v2.3.0`

- **Twelve voices.** The voice storage is a single pool that every part draws a window from, so six parts no
  longer reserve six times the polyphony they can use: 12 voices now occupy **87 584 B** of `.data + .bss`
  with **10 720 B free**, where the previous 10-voice build used 94 924 B with 3 380 B free.
- **A clear voice start.** A note that begins on a silent voice starts from silence: it never inherits the
  oscillators or the filter state of whatever note used that slot before.
- **ECO / BAL / FULL** — the same firmware at four, five or six tracks, with the CPU and the voice budget
  following the setting.
- **The rest of the instrument is unchanged**: eight engines, the 200-sound library, the sequencer, the
  effects, MIDI and SysEx, projects and presets from earlier versions.

## Synthesis engines

Eight engines, all digital, all edited from the panel (`EDIT` page 1 and 2) and from the Web Studio:

| Engine | What it is | Edit |
|---|---|---|
| **ANALOG** | virtual-analog: two oscillators, sub, noise, ring, filter, drift | waveform, shape, detune, filter, envelope |
| **FM6** | six-operator FM, eight algorithms, feedback | algorithm, ratios, levels, envelopes, feedback |
| **PHASE** | phase-distortion / CZ-style, two lines | waveform, harmonics, distortion, envelope |
| **WHEEL** | tonewheel organ: nine drawbars, percussion, click, drive, rotary | registration, bars, percussion, drive, rotor |
| **VOICE** | formant / vowel synthesis | vowel, formant shift, breath, envelope |
| **LOFI** | reduced-rate and reduced-depth digital, retrigger and clock | rate, depth, clock, retrigger, envelope |
| **DRUM-X** | drum synthesis: twelve kit lanes, per-hit tune and decay | kit, tune, decay, snap, mute groups |
| **NOISE** | noise-based percussion and textures | colour, band, decay, tone |

200 factory sounds are spread across them, plus user preset slots. Presets and projects from earlier
versions keep loading.

## Tracks, voices and CPU

| Setting | Tracks | Notes |
|---|---|---|
| **ECO** | 4 | the lightest: fewer parts, no chorus, the slicer off, every reverb a spring |
| **BAL** | 5 | the middle setting |
| **FULL** | 6 | all six parts, the full effects chain |

Twelve voices are shared by the parts, with psychoacoustic stealing: a released tail about to die goes
first, then the quietest note, and the lowest held note of a POLY part is protected. Core 0 renders the
master buses, the UI and tracks 1–3; core 1 renders tracks 4–6.

## Sequencing and performance

- **64 steps a track**, six tracks, with per-step velocity, gate, timing, ratchet, conditions and
  parameter locks (automation events or one-shot locks).
- **Patterns** per engine with `PATTERNS`, and **MUTATE** to vary what is already playing.
- **ARP** with modes, octaves, walk and chord memory; **SCALES / CHORDS**; **STRUM**; **ROLL**.
- **Songs**: sixteen chain rows of sections with their bar counts.
- **Punch-in FX** on the fly: stutter, arp repeat, half tempo, octave, random, decay and more.
- **Live recording**, per-track mute and solo, and a **GLO** layer that recalls a whole performance state.

## Effects

- **Five reverbs**: ROOM, SPRING, SHIMMER, RESO and CLOUD, each with its own decay and mix.
- **Delay** with its own bars, **chorus**, and a **slicer** that plays recorded material back in time.
- **Parametric master EQ** with a graphic display, a **limiter**, and **duck** / **punch** on the master bus.
- Per-track sends, an FX rack per track, and a mixer with six channels.

## Interface

- **HOME**: the track, its four parameters, the spectrum with the live filter curve, and the orbital system —
  a star, three orbits and three planets, each answering a pair of tracks.
- **Full-screen visualizer** (HOME twice): phosphor oscilloscope, phase trace, spectrum bars with peaks, the
  six track meters and the quantum motes.
- **Pages** for each engine, the FX rack, the mixer, the sequencer, the presets, the master bus, MIDI, the
  console and the preferences, on a 240 × 240 panel with fourteen buttons and seven encoders.
- The whole UI is rendered by the firmware sources and checked by the render sweep and its alignment lint.

## MIDI, projects and tools

- **USB-MIDI** in and out, with channel routing, a selectable track, and **SysEx** for the Web Studio and for
  backup / restore.
- **Projects**: song slots, patterns, automation, mixer and sound, saved and loaded on the unit.
- **Web Studio** edits the real sound over Web MIDI, and its hardware view shows the panel live.
- **Backup and restore** read the unit's flash through the loader.

## Hardware

| | |
|---|---|
| Device | M-VAVE FM-1 (desktop groovebox / synthesizer) |
| SoC | JieLi AC791N / WL82, dual-core `pi32v2`, single-precision FPU |
| Memory | 578 KB SRAM reported by the SDK; the firmware keeps `.data + .bss` inside 96 KiB |
| Flash | 1 MiB SPI NOR, XIP at `0x02000000` |
| Panel | 240 × 240 ST7789-class RGB565 |
| Controls | 14 buttons, 7 rotary encoders, the MASTER pot and 27 micro-switch keys |

Only two flash parts are accepted by the loader — P25Q80H (`0x856014`) and UCUN UC25HQ80 (`0xB36014`) — and
an unknown part is refused before anything is erased.

## Installing

The installer is a web page that talks to the unit over USB-MIDI, and it is the only write path:

1. Open the **[installer](https://guillel51.github.io/Quantum-GammaWave/webapp/installer/)** in Chrome or
   Edge, with a data cable (not a charge-only one).
2. It checks the package's **SHA-256**, its **Ed25519 signature** against the pinned `qg1-release-1` key and
   the firmware hashes inside that signature. If anything fails, INSTALL stays disabled and the page says why.
3. The loader reads the **real flash chip** on the unit and refuses anything but the two parts above.
4. The write covers `0x04000–0x92FFF`; factory data is preserved, and the loader is the recovery path.

**Before you write:** keep a backup of your unit, and read
[`FM1_INSTALLATION_SAFETY_AUDIT.md`](FM1_INSTALLATION_SAFETY_AUDIT.md). The installer never writes without
your confirmation, and there is no secure boot on this hardware — a verified package is not a guarantee about
a particular unit.

## Building from source

```sh
# target firmware (JieLi pi32v2 clang + the AC79 SDK)
JIELI_TOOLCHAIN=/path/to/toolchain AC79_SDK=/path/to/AC79_SDK python3 tools/build.py

# the 10-voice profile, and a pool size other than the default
FELUCCA_VOICES=10 python3 tools/build.py
FELUCCA_VPOOL=18 python3 tools/build.py

# the host tests: golden renders, voice / routing checks, UI, CPU budget
NO_PACKAGE=1 sh tests/run_tests.sh

# the browser emulator (needs emcc)
sh web/emu/build.sh
```

`build.sh` produces the `.fwsc` package. See [`BUILDING.md`](BUILDING.md) for the full setup and
[`docs/`](docs/) for the audits, the validation matrix and the memory baseline.

## Documentation

| | |
|---|---|
| [`docs/MANUAL.md`](docs/MANUAL.md) | the instrument, page by page |
| [`docs/VOICE_POOL.md`](docs/VOICE_POOL.md) | how the 12-voice pool works and what it measured |
| [`docs/OPTIMIZATION_BASELINE.md`](docs/OPTIMIZATION_BASELINE.md) | the memory regions and their floors |
| [`docs/VALIDATION_MATRIX.md`](docs/VALIDATION_MATRIX.md) | what is proven, by which test, on what |
| [`docs/HARDWARE_SPECS_FM1.md`](docs/HARDWARE_SPECS_FM1.md) | the unit as the code sees it |
| [`docs/EMULATOR_FIDELITY.md`](docs/EMULATOR_FIDELITY.md) | what the emulator does and does not reproduce |

## What this project does not claim

- No measurement on a physical FM-1 is part of this repository. The firmware is built with the AC791N
  toolchain and its behaviour is checked on the host and in the emulator; stack depth, interrupt timing and
  audio headroom **on the unit** are not measured here.
- Twelve voices are validated by the host suite and the static CPU budget, not by listening on hardware.
- Both flash parts are allowed by the code, not measured on hardware.
- No secure boot, no DRM, no vendor material in the packages.

## Credits and licence

GPL-3.0-only. Quantum GammaWave is an evolution of **Pocket GammaWave (GW-1)** by Leo Kuroshita
(@kurogedelic) / Hügelton Instruments, built on **JIANT FM** and **Felucca**. The original copyright notices,
the credits and the texts in [`LICENSES/`](LICENSES/) are kept; the JieLi SDK files that a package carries are
Apache-2.0, and [`LICENSING.md`](LICENSING.md) says which is which.
