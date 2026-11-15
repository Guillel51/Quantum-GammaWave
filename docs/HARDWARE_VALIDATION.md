# Hardware validation — the physical units

This page is the record of what has been run on **physical M-VAVE FM-1 units**, as opposed to what is
proven by the host suite, the AC791N build or the browser emulator. It exists because those are different
claims and this project does not mix them (`AGENTS.md` §4).

## What is reported

**Three M-VAVE FM-1 units have run Quantum GammaWave.** The newer of the three carries the second of the
two SPI NOR parts the loader accepts:

| Part | JEDEC | Accepted by the loader | Seen on a physical unit |
|---|---|---|---|
| P25Q80H | `0x856014` | yes (allowlist) | yes |
| UCUN UC25HQ80 | `0xB36014` | yes (allowlist) | yes, on the newer unit |

The firmware builds for AC791N with the JieLi `pi32v2` toolchain, reports identity `FM-1_965` (accepting the
`FM-1_9xx` family), and the three units ran it: the panel, the encoders, the keys, the audio output, the
presets, the sequencer and the USB-MIDI port behaved as the host and emulator runs describe.

**Source of this record:** the repository owner, who owns and operates the units. The date of this entry is
the 3.4.0 release.

## What this does not prove

Being explicit is the point of the table:

- **No captured artefact from those units is stored in this repository.** There is no flash dump, no scope
  trace, no recording and no per-unit test log in the tree. The report above is a compatibility statement, not
  a measurement anybody can reproduce from a clone.
- **No per-part write campaign.** Both parts are accepted by the allowlist and both have been seen in the
  field, but no stress campaign of erase/write cycles per part is claimed, and the loader's resume path after
  a lost link is proven on the host and in the emulator, not on every part.
- **No electrical measurements.** Rail noise, DAC headroom, audio latency from key to output and thermal
  behaviour are not measured here.
- **No autonomous recovery.** There is no A/B update and no verified independent recovery from a brick; a
  power cut during a write can still leave the unit in update mode.
- **No certification of every combination.** Three units and two flash parts are not "all FM-1 units in
  existence", and a sound that has run on them is not certified for every future board revision.
- **The numbers in this repository stay host numbers.** The draw cost (189 µs for HOME), the render budget and
  the voice-pool figures are host-x86 and static-target measurements, and they are reported as such.

## What the physical units did validate

The compatibility claims this project can now make on top of the software evidence:

1. The AC791N package **boots and runs** on real hardware, including the newer flash part.
2. The **display, the controls and the audio path** work with the shipped UI (240 × 240, 22 palettes,
   the five HOME screens, the sequencer, the FX layer).
3. **Presets can be changed while playing** without the instrument stopping, which is what the header and
   voice-pool work in 3.3.0/3.4.0 is for.
4. The **USB installer's** package is the one those units ran: the same `release/felucca-v3.4.0.fwsc` whose
   SHA-256 the installer checks before it enables INSTALL.

## How to extend this record

A future entry should say, for each unit: the board revision, the flash JEDEC read by the loader, the version
installed, what was exercised, and where the artefact (dump, log, recording) lives. A row without an artefact
is a report, and is labelled as one — the same rule this page follows.
