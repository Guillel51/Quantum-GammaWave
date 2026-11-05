# EMULATOR FIDELITY — what runs, and what does not

There are four different things people call "the emulator". Keeping them apart is the difference
between evidence and a demo.

| Kind | What it is | In this project |
|------|-----------|-----------------|
| Host simulator | the firmware's C compiled for the PC, with stub peripherals (`tests/hostsim.c`, `build/host/`) | used for DSP, UI renders and golden hashes |
| Host WASM | the same C compiled with Emscripten (`web/emu/`), running in a browser worklet | **this is what the public "Web Emulator" is** |
| Full-system emulator | an emulator of the SoC that executes the original `.fwsc`/bootloader | **not used, not claimed** (`joakimeriksson/fm1-emu`, `fm1-emulator` are third-party work) |
| Physical hardware | an FM-1 | not used in this work |

The Web Emulator therefore runs *the same `firmware/src` C*, not the AC791N binary: no instruction
decoding, no exceptions, no mask ROM, no bootloader, no real SRAM. It is a faithful **DSP and UI**
execution and an unfaithful **hardware** one.

---

## 1. Fidelity matrix

| Element | Host simulator | Web Emulator (WASM) | Full-system emulator | Hardware |
|---------|----------------|---------------------|----------------------|----------|
| CPU instructions (pi32v2) | n/a (native) | n/a | third-party | real |
| Exceptions / interrupts | approximated | approximated | partial (third-party) | real |
| Two cores / shared RAM | one core or `mc_host` on the same thread | same | partial (third-party) | real, unit-dependent |
| Cache / XIP | stub (`fm1_xip.h`) | stub | partial | real |
| SPI NOR | simulated array (`nor[0x100000]`) | same | model | real part, two documented IDs |
| Timing | not cycle-accurate | not cycle-accurate | — | real |
| DMA | simulated ring | simulated ring | — | real |
| Audio 44.1 kHz stereo | bit-exact vs the same C | bit-exact vs native (`web/emu/emu_test.mjs`) | — | measured only on a board |
| Display 240×240 RGB565 | real drawing code, PNG out | real drawing code, canvas | partial | real |
| Keys / encoders / buttons | injected | injected | — | real |
| LEDs / MIDI / USB | stubs | stubs | — | real |
| Flash persistence | array, survives instances in the test | same | — | real |
| `.fwsc` package parsing | real (`web/fm1pkg.js`, node) | real | — | real |
| Power loss / brown-out | simulated cut points | simulated | — | **not simulated here** |
| Bootloader / recovery | not present | not present | third-party | real |

"Partial (third-party)" means: described by that project's own documentation, not verified here.

## 2. Synchronisation of the published Web Emulator

The WASM is built from the same tree and pinned by hash: `tools/package_emu.py` writes
`build/emu/manifest.json` with version, commit and the WASM's SHA-256, and the page pins that hash,
requests `?v=<sha256>` with `cache: "no-store"` and verifies the bytes with `crypto.subtle` before
running them. `tests/emu_sync.py` fails if page, manifest and binary disagree.

In this session **the WASM was not rebuilt**: `emcc` is not installed in the workspace
(`[NOT-VERIFIED]`). Firmware sources changed, so the published Web Emulator in `build/` does not
contain this change until `sh web/emu/build.sh` runs and the manifest is regenerated. That is a
build step, not a claim.

## 3. What an emulator cannot validate

* Flash erase/program behaviour, wear, or a part's protection bits.
* Voltages, brown-out, USB electrical behaviour, the second core's real shared RAM.
* The mask ROM, the SPL, the bootloader, the update record's survival across a real power cut.
* Any claim about recovery. A simulator cannot unbrick a device.

## 4. Third-party projects evaluated (not integrated)

| Project | Use | Why not integrated |
|---------|-----|--------------------|
| <https://github.com/joakimeriksson/fm1-emu> | fuller boot-chain understanding | it emulates the SoC, not this firmware's build; integrating it would not increase coverage of the DSP or the UI, and it must be checked for licence and accuracy first |
| <https://github.com/joakimeriksson/fm1-emulator> | reference | same |
| `m-wave-fm1-browser-emulator` | could not be confirmed to exist at a stable URL during this work | not referenced |

The rule applied: **do not replace a working emulator with a less proven one**, and do not call a
host simulator a full-system emulator.
