# FM-1 physical validation guide — v2.9.4

The one thing this repository cannot prove from software: that the package boots, sounds and can be left
again on a real unit. Everything below is for the owner to run; nothing here has been executed in this
workspace, and no step writes to a device by itself.

Read `FM1_INSTALLATION_SAFETY_AUDIT.md` and `docs/QG_RELEASE_BLOCKERS.md` first.

## 0. Before touching anything

1. Read the flash ID with a programmer or the loader: it must be `0x856014` (Puya P25Q80H) or `0xB36014`
   (UCUN UC25HQ80). Anything else: stop.
2. Make a full 1 MiB backup and hash it (`tools/spi_dump.py`, `tools/fm1_flash_read.py`). Keep two copies.
3. Note the origin firmware and its version, and keep its package file.
4. Install only from the published release (`felucca-v2.9.4.fwsc` + `SHA256SUMS`), and check the hash.

## 1. Install

1. Open the installer in Chrome or Edge with a data cable.
2. It re-hashes the package, checks the version floor and the firmware images, reads the real JEDEC and
   refuses anything outside the allowlist.
3. INSTALL stays disabled unless every check passes. If it refuses, the reason is shown — record it.

## 2. First boot

Record, with a phone or a stopwatch:

| What | Expected | Measured |
| --- | --- | --- |
| First frame on the panel | immediately after power-on | |
| Splash starts moving | as soon as the first frame is drawn | |
| HOME interactive | about 4 s | |
| Audio on a key press | immediately at HOME | |

**Known issue to look at:** the splash is diagnosed as repainting only at init stage boundaries
(`docs/QG_UI_AND_SOUND_DESIGN_REFINEMENT.md` §1), so a still picture for part of the boot is expected in
v2.9.4. Write down how long it stays still — that number is the input for the cooperative-boot fix.

## 3. Checks on the unit

* HOME: walk the five screens forward and back, five times each; confirm BPM, battery and the top panels
  are intact when you return to MAIN.
* Keys, all 27; all 14 buttons; all 7 encoders; the MASTER pot.
* Audio: load a preset per engine, play six tracks, check for dropouts and for clipping on the master.
* USB-MIDI in and out, and the Web Studio connection (the editor's own page).
* Save a preset, a pattern and a project; power-cycle; load them back.
* Both flash IDs if a second unit is available.

## 4. Leaving Quantum GammaWave

1. Return to UBOOT with OCT- + OCT+ held for 5 s (the on-screen countdown), or by whatever path the origin
   firmware documents.
2. Write the origin package you kept in step 0.
3. Verify the unit boots the origin firmware and that its own sounds, sequencer and MIDI work.
4. If the origin firmware cannot be restored, restore the full backup from step 0 with the programmer.

## 5. Report

Send back: flash ID, origin version, the four timings, the results of §3, and whether §4 restored the
origin. With those, `docs/VALIDATION_MATRIX.md` can move the physical rows from `PENDIENTE` to `GO`, and
only then can the installer gate be discussed.
