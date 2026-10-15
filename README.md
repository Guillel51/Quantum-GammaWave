# POCKET GAMMAWAVE (GW-1) — M-VAVE FM-1 Firmware & Web Studio

**Pocket Solar Hearth · Live FFT Spectrogram Filter · Wraith Voice Mode Ribbon · 4-Track Analog Tape · 49-Genre Song Generator (incl. Trap ATL, UK Drill, Dark Trap, Rage)** — the open-source hardware & browser companion to the flagship **[GammaWave Desktop Synthesizer VST](https://gammawaveaudio.com/)**.

[![Buy GammaWave VST](https://img.shields.io/badge/🔥%20BUY%20GAMMAWAVE%20VST-shadowtune.itch.io%2Fgammawave-ffb800?style=for-the-badge)](https://shadowtune.itch.io/gammawave)
[![Official Website](https://img.shields.io/badge/🌐%20OFFICIAL%20SITE-gammawaveaudio.com-00d4ff?style=for-the-badge)](https://gammawaveaudio.com/)
[![Web Studio & Emulator](https://img.shields.io/badge/🎛️%20WEB%20STUDIO-fvalente.github.io%2FGammaWave--FM1-ff5500?style=for-the-badge)](https://fvalente.github.io/GammaWave-FM1/webapp/editor/)
[![USB Web Installer](https://img.shields.io/badge/⚡%20USB%20INSTALLER-Flash%20FM--1-00e5a3?style=for-the-badge)](https://fvalente.github.io/GammaWave-FM1/webapp/installer/)
[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg?style=for-the-badge)](LICENSE)

> 🔥 **Love the sound & visual workflow of Pocket GammaWave?** Unlock the full studio **[GammaWave Desktop VST3/AU Synthesizer (PC / Mac)](https://shadowtune.itch.io/gammawave)** with multi-oscillator Solar Hearth wavetables, high-resolution FFT spectrogram filter, unlimited polyphony, and full DAW automation:
> - **🛒 Buy the Desktop VST Plugin:** **[https://shadowtune.itch.io/gammawave](https://shadowtune.itch.io/gammawave)**
> - **🌐 Official Website:** **[https://gammawaveaudio.com/](https://gammawaveaudio.com/)**
> - **🎛️ Live Web Studio (4-Track Tape & HUD):** **[https://fvalente.github.io/GammaWave-FM1/webapp/editor/](https://fvalente.github.io/GammaWave-FM1/webapp/editor/)**
> - **⚡ 1-Click USB Web Installer:** **[https://fvalente.github.io/GammaWave-FM1/webapp/installer/](https://fvalente.github.io/GammaWave-FM1/webapp/installer/)**

---

## Hardware 240×240 LCD Screen Showcase

![Pocket GammaWave (GW-1) Hardware Screens](docs/gw1-screens.png)

*Authentic 240×240 RGB565 hardware frames rendered by Pocket GammaWave (`v1.0-GW`): **1. HOME** (Solar Hearth core + orbiting Wraith moon + 4-track tape deck), **2. WAVES** (Dual Oscillator scopes + 6-mode Wraith Voice Ribbon), **2B. FILTER** (Live 24-band FFT Spectrogram + Orange Filter / Cyan Mod curves + Vowel Formants), **4. TAPE** (4-Track Analog Tape Reels + Master Bus), **5. MASTER FX** (Drive / Reverb / Delay), and **DRUM-X** (8-Pad Chamfered Drum Forge).*

---

## What's Inside Pocket GammaWave (`v1.0-GW`)

- **☀️ Solar Hearth & Spectrogram HUD (`1.HOME`, `2.WAVES`, `2B.FILTER`, `4.TAPE`, `5.MASTER FX`)**
  Mirrors the visual language of the **[GammaWave Desktop VST](https://gammawaveaudio.com/)** on both the M-VAVE FM-1's 240×240 IPS screen and the browser Web Studio:
  - **Solar Hearth Core (`1. HOME`)**: Audio-reactive blazing yellow/orange solar core with an orbiting cyan Wraith satellite moon (`GRAVITY`, `SINGULARITY`, `PHOTOSPHERE`) and a 4-track tape deck (`T1..T4`).
  - **Dual Oscillator + Wraith Voice Ribbon (`2. WAVES` / `EDIT 1`)**: Side-by-side `OSC 1` and `OSC 2` waveform scopes above a continuous `0..100%` (`0..127`) **Wraith Voice Mode Bar** (`FLU` · `WHI` · `VOX` · `CHR` · `BEL` · `SCR`).
  - **Live FFT Spectrogram Filter (`2B. FILTER` / `EDIT > FILTER`)**: 24-band thermal spectrum analyzer with real-time logarithmic filter response curves (`LP`, `HP`, `BP`, `COMB`, `VOWEL`, `SCREAM`) and 5-band vowel formant morphing (`OO` · `OH` · `AH` · `EH` · `EE`).
- **🎲 49-Genre Multi-Track Song Generator (`[OCT-] + [PLAY]` or `SELECT` on `HOME`)**
  Generates complete, musical 2-bar (32-step) multi-track arrangements across 49 styles—including dedicated **Trap & Drill** production engines:
  - **`TRAP ATL`** (140 BPM half-time bounce, deep 808 sub, crisp hi-hat ratchet rolls `x2..x4`, dark minor bell lead)
  - **`UK DRILL`** (142 BPM signature `3-3-2` tresillo hi-hat bounce, delayed snare on step 7/13, sliding 808 bass `808GLD`)
  - **`DARK TRAP`** (136 BPM Phonk/Memphis 808 cowbell melody, crushed 808 sub, ominous choir pad)
  - **`RAGE TRAP`** (152 BPM hyper-detuned saw leads, heavy distortion, relentless 808 drive)
  - Plus **Synthwave, Outrun, Cyberpunk, Darksynth, Acid Techno, Dub Techno, IDM, House, Breakbeat, D&B, Lo-Fi Hip-Hop, Ambient, and Neo-Classical**.
- **🧬 1-Click `SEQ > MUTATE` (`[OCT-] + [ENV]`)**
  Bar-synced autonomous pattern and sound evolution:
  - **`ORBIT`**: Mutates notes and timbre every 1–8 bars while pulling back toward your original anchor pattern so the groove never gets lost.
  - **`DRIFT`**: Evolves continuously into new melodic and timbral territory.
  - **`FREEZE` & `KEEP`**: Lock a mutated bar on the fly (`FREEZE`) or commit the current mutation as your new home pattern (`KEEP`).
- **📼 Browser 4-Track Analog Tape Studio & 1-Click `.WAV` Export**
  In the **[Web Studio](https://fvalente.github.io/GammaWave-FM1/webapp/editor/)**, record or overdub up to 4 stereo tape tracks (`T1..T4`) with analog tape saturation and wow/flutter, or export any generated loop directly to a 16-bit 48 kHz `.WAV` file for your DAW.
- **🌌 Hidden 60 FPS Hyperspace 3D Visualizer (`[OCT-] + [HOME]`)**
  4 demoscene visualizers reacting to live audio: **`1/4 SUPERNOVA`**, **`2/4 WRAITH 3D`**, **`3/4 WARP TUNNEL`**, and **`4/4 MATRIX FFT`**.

---

## ⚡ GammaWave `[OCT−]` Shift Deck (1-Hand Hardware Shortcuts)

Hold **`[OCT−]`** first, then tap any button from any screen:

| Shortcut | Action |
| :--- | :--- |
| **`[OCT−] + [ARP]`** | 🔥 **Cycle 6 Solar Hearth & Eclipse Macros** (`1/6 HEARTH GRAVITY` → `2/6 SINGULARITY` → `3/6 PHOTOSPHERE` → `4/6 WRAITH SCREAM` → `5/6 FORMANT VOWEL` → `6/6 SOLAR ECLIPSE!`), sweeping across all 6 Wraith Voice Modes (`FLU`..`SCR`) |
| **`[OCT−] + [PLAY]`** *(or `SELECT` on `HOME`)* | 🎲 **Open 49-Genre Song Generator (`GEN`)** — turn `KNOB 2` to select `TRAP ATL`, `UK DRILL`, `DARK TRAP`, `RAGE TRAP`, etc., and press `OCT+` to roll |
| **`[OCT−] + [ENV]`** *(or `[OCT−] + [SCL]`)* | 🧬 **1-Click `SEQ > MUTATE`** — jumps straight to the Mutate page and enables `ORBIT` (`SOUND = 2B`, `PATTERN = 4B`, `AMOUNT = 50%`) |
| **`[OCT−] + [HOME]`** *(or `[OCT−] + [SAVE]`)* | 🌌 **Launch 4-Mode Hyperspace 3D Visualizer** (`OCT+` cycles `1/4..4/4`, `KNOB 1..4` control Warp, Twist, Orbit & Bloom) |
| **`[OCT−] + [SEQ]`** | 🎲 **Dice Sequence** on the active track |
| **`[OCT−] + [EDIT]`** | 🎲 **Dice Sound Patch** on the active track |
| **`[OCT−] + [FX]`** / **`[OCT−] + [LFO]`** | 🎲 **Dice Track FX** / **Dice Track LFO** |
| **`[OCT−] + [GLO]`** *(or `GLO + G4`)* | ↩️ **Instant Undo / Redo** |
| **`[EDIT] + [Key 1–6]`** | 🎹 **Select Track `T1..T6`** from any screen (tap `[EDIT]` repeatedly to cycle `EDIT 1 (WAVES)` → `EDIT 2` → `FILTER (SPECTROGRAM)` → `VOICE`) |

---

## Printable Cheat Sheets

Click either sheet to view full size, or download the high-resolution printable PDF:

| Controls & `[OCT−]` Shift Deck | Sound · Spectrogram Filter · FX · Mod |
| --- | --- |
| [![Pocket GammaWave Controls Cheat Sheet](docs/cheatsheet-controls.png)](docs/cheatsheet-controls.png) | [![Pocket GammaWave Sound & FX Cheat Sheet](docs/cheatsheet-sound.png)](docs/cheatsheet-sound.png) |
| Every knob, button, `[OCT−]` Shift Deck shortcut, held keymap layer (`FX`, `GLO`, `SAVE`, `EDIT`, `LFO`, `SCL`, `SEQ`, `REC`), and page knob assignment. **[Download PDF](docs/cheatsheet-controls.pdf)** | All 8 synthesis engines, the Spectrogram Filter (`LP`, `HP`, `BP`, `COMB`, `VOWEL`, `SCREAM`), Wraith Voice Modes, 49-Genre Generator, `MOD` matrix, `MSEQ`, `MUTATE`, and Master FX. **[Download PDF](docs/cheatsheet-sound.pdf)** |

Full user manual: **[English Manual (`docs/MANUAL.md`)](docs/MANUAL.md)** · **[Manual en Español (`docs/MANUAL.es.md`)](docs/MANUAL.es.md)**.

---

## Synthesis Engines & Sound Architecture

### 1. Eight Real-Time Synthesis Engines (Zero Samples)
- **`ANALOG`**: Dual virtual-analog oscillators (`SAW`, `SQR`, `TRI`, `SIN`, `PWM`, `SYNC`, `RING`, `SAW3`) with interval offset (`INT ±24 st`), detune, sub/noise mix, and per-voice resonant filter (`CUT`, `RES`, `DRIVE`).
- **`FM6`**: 6-operator DX7 FM synthesis (compatible with `.syx` cartridges via the Web Studio), 32 algorithms drawn live on screen, plus full on-device per-operator editing (`OPERATOR`, `OP ENV`, `OPERATOR 2`) and 32-slot user voice bank (`SLOT B1–B32`).
- **`PHASE`**: Phase distortion synthesis (`CZ`-style) with phase feedback (`FB`) morphing from warm brass to aggressive digital growl.
- **`LOFI`**: 1-bit, 4-bit, and 8-bit chiptune engine plus 32-formula **Bytebeat (`BYTE`)** and **Floatbeat (`FLOAT`)** procedural synth engines tuned to the keyboard scale.
- **`VOICE`**: Formant vocal synthesizer (`VOWEL`, `VOWEL2`, `TALK`, `SHIFT`, `BUZZ`, `BREATH`) that drives the 5-band Vowel Formant Ribbon (`OO / OH / AH / EH / EE`).
- **`WHEEL`**: Tonewheel drawbar organ with rotary speaker simulation, key click, and harmonic percussion.
- **`NOISE`**: Tuned and free-running resonant/metallic noise generator for risers, snares, and industrial textures.
- **`DRUM-X`**: 8-pad real-time synthesized drum machine (`BD`, `SD`, `CP`, `CH`, `OH`, `TM`, `RS`, `CB`) with `A/B` kit morphing (`MORPH`), wavefolder (`FOLD`), harmonic FM (`FM`), 808 long pitch-drop mode (`PMOD LONG`), and tempo-synced `X-MOD`.

### 2. Spectrogram Filter & Wraith Voice Modes (`EDIT > FILTER`)
Every track runs through the **GammaWave Forge Filter** (`LP`, `HP`, `BP`, `COMB`, `VOWEL`, `SCREAM`):
- **`WRAITH VOICE BAR`**: Displays 6 character zones (**`FLU` · `WHI` · `VOX` · `CHR` · `BEL` · `SCR`**) or 5 formant vowels (**`OO` · `OH` · `AH` · `EH` · `EE`** when `VOWEL`/`SCREAM` or `VOICE` engine is active). Turning `KNOB 1` on `EDIT 1` or `CUT` (`KNOB 2`) on `FILTER` sweeps the playhead needle across the full `0..127` bar.
- **`COMB`**, **`VOWEL`**, & **`SCREAM`**: Key-tracked physical-modeling comb resonators and triple-formant vocal filters for talking basslines and screaming leads.

### 3. Punch-In FX, Slicer Bus, 5 Reverbs & Master Leveler
- **Playable Punch-In FX (`FX` held + Keys)**: Quantized 1/16th-note performance effects (`REPEAT 1/8 · 1/16 · 1/32`, `LPF`, `HPF`, `OCT-/+`, `1/2 TEMPO`, `DEC-/+`, `STUTTER 1/8 · 1/16`, `ATK+`, `ARP`, `RANDOM`) recordable into a 4-bar automation lane per section.
- **5 Reverb Models**: `ROOM`, `SPRING`, `SHIMR` (octave-up shimmer), `RESO` (4 scale-tuned sympathetic strings), and `CLOUD` (pitch-granulated freeze wash).
- **Master Bus (`FX > MASTER`)**: `CLIP` (level-compensated soft clipper), `PUNCH` (drum transient saturator), `DUCK` (kick-triggered sidechain pump), and an always-on 2:1 auto-leveler.

### 4. Dual-Core Audio Engine (JieLi AC791N)
Pocket GammaWave splits synthesis and stereo FX buses across both cores of the FM-1's AC791N processor, with automatic per-block checksum and watchdog protection (falling back seamlessly to single-core if a hardware read fault ever occurs). Hold **`REC`** at power-on to force single-core mode.

---

## Installing on M-VAVE FM-1 Hardware

1. **Open the USB Web Installer** in Chrome, Edge, or Opera:
   👉 **[https://fvalente.github.io/GammaWave-FM1/webapp/installer/](https://fvalente.github.io/GammaWave-FM1/webapp/installer/)**
2. Connect your **M-VAVE FM-1** directly via a USB data cable and switch it on.
3. **Coming from another custom firmware (Felucca, baudgirl, etc.)?** Click **Return to official V15 first** (which also backs up your flash), then click **Install**.
4. **Coming from Official V15, JIANT, or an earlier Pocket GammaWave build?** Click **Install** directly. Do not unplug the cable while flashing completes.

*Note: Pocket GammaWave repurposes the FM-1's user sample flash (`USR1–3`) to store 8 full multi-section songs and 32-voice DX7 `.syx` banks. Back up any custom user WAV samples from stock Felucca before flashing.*

---

## Building from Source

See **[BUILDING.md](BUILDING.md)** for full details:

```bash
# Linux / macOS / WSL2 Ubuntu:
python3 tools/build.py                                           # Builds build/felucca.fwsc
sh web/emu/build.sh                                              # Builds WebAssembly emulator in build/emu
python3 web/make_site.py build/felucca.fwsc v1.0-GW build/site   # Packages Web Studio + USB Installer

# Windows (via WSL2 Ubuntu + local Python):
python tools/build_windows.py
```

---

## Changelog

### `v1.0-GW` — Pocket GammaWave (`GW-1`) Release
- **GammaWave Solar Hearth & Wraith Hardware HUD (`ui_being.c`, `ui_tech.c`)**:
  - Replaced legacy screens with the **GammaWave HUD**: `1. HOME` (Solar Hearth core, orbiting Wraith moon, 6-track chamfered ingot deck), `2. WAVES` (dual `OSC 1`/`OSC 2` waveform scopes + continuous `0..127` Wraith Voice Ribbon), `2B. FILTER` (live 24-band FFT Spectrogram + Orange Filter / Cyan Mod response curves), `4. TAPE` (4-track analog tape reels), `5. MASTER FX`, and `DRUM-X` (8-pad chamfered drum forge).
  - Custom `GAMMAWAVE // WRAITH` boot splash screen with animated solar corona and `gammawaveaudio.com` signature.
- **Full `0..127` Wraith Voice Mode Bar (`FLU · WHI · VOX · CHR · BEL · SCR` & `OO · OH · AH · EH · EE`)**:
  - Unified cutoff/formant tracking across `ANALOG` (`P_E4`/`P_E5`) and all digital/formant engines (`P_FCUT`/`P_FRES`/`P_E0`) so turning `KNOB 1` (`WAVE`) or `CUT` sweeps smoothly across all 6 Wraith Voice Modes on hardware and in the Web Studio.
- **49-Genre Song Generator with Trap & Drill Engines (`gen.c`, `web/editor.html`)**:
  - Added **`TRAP ATL`**, **`UK DRILL`** (`3-3-2` tresillo hi-hat bounce & delayed snare), **`DARK TRAP`** (Memphis 808 cowbells), and **`RAGE TRAP`**, plus 808 portamento glide (`808GLD`) and hi-hat ratchet rolls (`x2..x4`), defaulting to 32-step (2-bar) patterns.
- **`[OCT−]` Hardware Shift Deck (`ui_input.c`)**:
  - Added 1-hand hardware shortcuts: `[OCT−] + [ARP]` (6 Solar Hearth & Eclipse Macros), `[OCT−] + [PLAY]` (49-Genre Song Generator), `[OCT−] + [ENV]` / `[SCL]` (1-click jump to `SEQ > MUTATE` with automatic `ORBIT` activation), `[OCT−] + [HOME]` / `[SAVE]` (4-mode 60 FPS Hyperspace 3D Visualizer), `[OCT−] + [SEQ/EDIT/FX/LFO]` (instant track dice), and `[OCT−] + [GLO]` (instant undo).
- **GammaWave Web Studio (`web/editor.html`)**:
  - Added interactive 4-Track Analog Tape Recorder (`T1..T4`), 1-click stereo `.WAV` export, clickable LCD screen tabs (`1. HOME` .. `5. MASTER FX`), interactive `👻 WRAITH VOICE BAR` (`FLU..SCR` buttons + continuous `0..127` slider + direct LCD canvas drag), and `🧬 SEQ MUTATE` (`🎲 MUTATE NOW`, `ORBIT`, `DRIFT`, `AMOUNT`, `FREEZE`, `KEEP ANCHOR`).

---

## Credits & License

**Pocket GammaWave (`GW-1`)** is free and open-source software released under **[GPL-3.0-only](LICENSE)**, created as the hardware & browser companion to the **[GammaWave Desktop VST](https://gammawaveaudio.com/)** ([Buy on Itch.io](https://shadowtune.itch.io/gammawave)).

Built upon the work of:
- **[Felucca](https://github.com/hugelton/Felucca)** by [Hügelton Instruments](https://hugelton.com) (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)) — core FM-1 hardware abstraction, bootloader, engines, and [Fukiai](https://github.com/hugelton/Fukiai) icon font ([MIT](LICENSES/MIT-Fukiai.txt)).
- **[JIANT FM](https://github.com/juanjiant-bit/JIANT)** — DRUM-X synthesis engine, dual-core audio scheduler, procedural generator foundation, and thermal font pipeline.
- **[SLOOP](https://github.com/isod89/sloop-fm1)** by isod89 — multi-section song arranger and live `SONG REC` architecture.
- **[Melodee](https://github.com/keremimo/melodee)** (keremimo) & **[fm1-x0x](https://github.com/charlesvestal/fm1-x0x)** (Charles Vestal) — AC791N second-core startup, memory-read guard, and WebAssembly browser emulator architecture.
- **DSP & Fonts**: `msfa` 6-op FM from [Dexed](https://github.com/asb2m10/dexed) ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt)); formant synthesis after [klattsch](https://github.com/tgies/klattsch) (MIT); [Chakra Petch](LICENSES/OFL-ChakraPetch.txt), [IBM Plex Mono](LICENSES/OFL-IBMPlexMono.txt), [Michroma](LICENSES/OFL-Michroma.txt), and [DotGothic16](LICENSES/OFL-DotGothic16.txt) (SIL OFL 1.1).

*M-VAVE and FM-1 are trademarks of their respective owners. Pocket GammaWave is an independent open-source project and is not affiliated with or endorsed by M-VAVE or Hügelton Instruments.*
