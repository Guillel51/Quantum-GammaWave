# Hardware Validation Plan — M-VAVE FM-1 / QG-1

Status: **PLANNED, NOT EXECUTED**. No physical FM-1 was available, no flash was read or written, and
no claim in this document is a measurement.

This plan is deliberately conservative. A simulated test, a successful USB update or a booting
emulator does not establish that an FM-1 can be recovered. The first goal is to prove a way back,
not to install the newest build.

## 0. Safety gates

Do not proceed past a gate until its result is recorded with the unit's serial number, date,
operator, tool version and logs.

1. **Two units preferred.** Use one sacrificial unit for recovery/cut tests and keep a second unit
   untouched as a reference. A single unit is acceptable only if a full flash image and a verified
   USB_KEY recovery path already exist.
2. **Read-only first.** Read the JEDEC, the flash and the boot area before writing anything.
3. **Full backup.** Two independent full-flash reads must have the same SHA-256; the image must be
   stored off-device and checked by a second operator.
4. **Recovery proof.** USB_KEY recovery must be demonstrated on the sacrificial unit before a
   deliberate power cut or a boot-area test. Recovery with a damaged boot area is **not** assumed.
5. **Never write the head.** Do not use a tool that writes `[0x00000, 0x04000)` unless the exact
   procedure has been demonstrated on that unit. Do not copy another unit's full flash.
6. **Power.** Use a current-limited, isolated 5 V USB supply with a physical switch for cut tests.
   Never feed VBUS into the USB_KEY dongle.
7. **Stop conditions.** Stop on any JEDEC mismatch, any full-read hash mismatch, a device that does
   not enumerate, a reset loop, a watchdog reset, smoke, or a current above the documented limit.
8. **AGENTS.md.** No physical flashing is performed by this repository or its CI. A human operator
   owns every write in this plan.

## 1. Unit identity and JEDEC

Equipment: USB power meter/current limit, USB isolator, known-good USB-C data cable, camera,
magnifier, and a read-only flash/JEDEC tool that does not write.

1. Photograph the board, chip markings and the flash package. Record the SoC marking
   (`C156211-11B8` / `C188612-11B8` or another variant) and the flash marking.
2. Read the JEDEC ID through a read-only path (ROM download mode or the vendor read tool). Record
   the exact 24-bit value; expected for the documented Puya P25Q80H is `0x856014` (1 MiB).
3. If the ID is not `0x856014`, stop. Do not use a 1 MiB layout assumption; re-audit the package
   and the flash map.
4. Record the ROM/USB identity (`4C4A:8057`, `WL82/UBOOT1.00`), the SPL and bootloader versions and
   the stock firmware version. Do not update yet.

Pass: marking, JEDEC and the map agree with the repository's 1 MiB assumption. A mismatch is a
**NO-GO**, not a reason to try a different layout.

## 2. Baseline and recovery proof

1. Read the complete flash twice. Compare byte-for-byte and SHA-256. A single differing byte is a
   failure; repeat both reads before continuing.
2. Save the raw image, the two read logs and the unit identity off-device.
3. Verify that the unmodified unit boots and that the stock V15 package is available locally.
4. Demonstrate the documented update-mode entry (Felucca: OCT− + OCT+ held 5 s at power-on;
   fm1-base/m-bootloader: REC) without writing.
5. Demonstrate recovery on the sacrificial unit using the exact **USB_KEY** dongle and wiring from
   `docs/SECURITY.md`: ROM download mode, full read first, write only `[0x4000,0x93000)`, final
   full read equal. Do not proceed if the dongle needs VBUS.
6. Only after a successful recovery: test a known-good update and then a second recovery. Record
   the time to recover and the exact key/button sequence.

Pass: two matching full reads, a booting unit, and a demonstrated recovery to a known image.

## 3. Memory, stacks, interrupts and DMA

The repository has a static ELF map and host guard tests; it does not have a target stack
high-water measurement.

1. Record the ELF symbols (`_data_start`, `_bss_end`, `_pool_end`, `_c1_stack_hi`, `_guard0`,
   `_ustack_lo`, `_sstack_lo`, `_sstack_top`, `_hot_end`, `.noinit`). Re-run
   `tests/memory_map_test.py` against the exact image being tested.
2. Exercise the audio stress preset (six parts, ten voices, FX/reverb) for at least 30 minutes at
   44.1 kHz while USB MIDI and the display are active. Record underruns, resets and fault reports.
3. Exercise the UI and the largest project, save/load and the FM6 bank. Watch the guard/fault
   screen and the serial/UART diagnostic output, if the unit exposes it without a hardware mod.
4. For stack high-water, use a diagnostic build that paints the user/system stacks and reports the
   high-water mark. That build does **not** exist yet; do not infer stack headroom from the four
   unused bytes or from the host.
5. Check the second core's mailbox and the audio DMA under load. The host tests inject scheduling
   but cannot prove the real interrupt latency, cache or DMA arbitration.

Pass criteria: no guard fault, no watchdog reset, no lost DMA half, no underrun during the soak and
a measured stack margin that is documented. The current plan has **no target stack measurement**, so
this gate is open.

## 4. Clocks, reset, watchdog and power

1. Measure the 24 MHz crystal and the CPU clock at the documented PLL setting. Do not change the
   clock configuration as part of this test.
2. Verify reset behaviour: power-on, short reset, the documented 8 s PB01 reset hold, and the
   watchdog after a controlled hang. Record whether the unit returns to the application or the
   loader.
3. Measure idle, display-on, audio-heavy and update currents. Compare with the supply/thermal
   limits; stop on any unexpected rise.
4. Run at the low and high ends of the documented supply range only if the bench supply and the
   unit are rated for it. Do not use a USB charger as a precision source.
5. Repeat a cold boot at least 20 times, including with USB connected and disconnected.

Pass: stable clocks, deterministic reset/watchdog behaviour, no thermal or current anomaly and no
boot loop. Exact limits for this board are **not documented in this repository**, so the operator
must record them from the bench.

## 5. Update, disconnection and power-cut matrix

Run only after Sections 1–2 pass.

| Case | Injection | Expected |
|---|---|---|
| Clean update | normal host | app verifies, reboots, version matches |
| Host disconnect before erase | unplug after handshake | no app write, old app boots |
| Host disconnect during transfer | unplug at 25 %, 50 %, 75 % | loader stops; record remains; resume completes |
| Power cut during erase/program | switch 5 V at sector boundaries | record remains; next boot loads loader; resume completes |
| Power cut after finish, before reset | switch after `success` | record cleared; app boots; no loader loop |
| Corrupt package | flip a payload byte | refused before erase |
| Wrong model/key | foreign package | refused before erase |
| Backup restore interrupted | unplug during a chunk | documented partial restore; no write past object size |
| USB_KEY recovery | after a failed update | ROM download mode, full read, restore known image |

For each cut, record the exact byte/sector, the flash contents before/after, the record at
`0xE4F00`/`0xE8F00`, and whether the unit recovered without USB_KEY. A test that requires USB_KEY is
still a pass for recovery, but it is a **failure of the update path's uninterrupted operation**.

Pass: every cut either resumes to the new app or is recoverable to a known image, with no writes
outside the documented windows. A cut that requires a boot-area write is a stop.

## 6. Audio and user-data checks

1. Play every engine, the 200 factory presets and a user bank through the headphone output; record
   clicks, DC, channel imbalance, noise and level.
2. Check the master EQ, stereo width, clip/punch/duck and the FX sends at their extremes.
3. Save a project, a user preset, settings, a song and an FM6 bank; power-cycle; verify all fields,
   including legacy projects and the retired DRUM indices.
4. Capture a backup, restore it, and compare every object byte-for-byte with the pre-restore image.
5. Interrupt a restore and confirm the documented partial-restore state; do not call it atomic.

Pass: audio is stable, stored data is unchanged after a clean save/restore, and a partial restore is
reported as partial.

## 7. Acceptance and reporting

A build is eligible for **controlled physical validation** only when:

- the JEDEC is read as `0x856014` and the full flash map is backed up twice;
- USB_KEY recovery has been demonstrated on the sacrificial unit;
- the host suite is green or every pre-existing failure is explicitly waived with an issue;
- the target build, `tests/memory_map_test.py`, `tests/ldr_test` and the package parser pass on the
  exact image;
- no test writes the head or a region outside `[0x4000,0x93000)` except the documented record slots;
- the operator accepts that a power cut can still require USB_KEY.

A build is **not** eligible if it changes the bootloader, the flash map, the package head, the
clock/power configuration or a DSP budget without a separate reviewed measurement.

## Sources

Primary/part data:

- Puya P25Q80H datasheet (8 Mbit, 2.3–3.6 V):
  <https://www.puyasemi.com/download_path/%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C/Flash%20%E8%8A%AF%E7%89%87/P25Q80H_Datasheet_V1.7.pdf>
- JieLi AC79 SDK and download documentation: <https://doc.zh-jieli.com/> and
  <https://doc.zh-jieli.com/AC79/zh-cn/release_v1.0.3/getting_started/project_download/download.html>

Community reverse engineering (not vendor guarantees):

- FM-1-RE: <https://github.com/AL-255/FM-1-RE>
- FM-1 developers' wiki, especially Flash-Storage, Boot-Chain-and-Bootloaders,
  Update-Protocol-and-Installers and Recovery: <https://github.com/sbegueria/fm1-dev-wiki>
- lunar-modulator recovery notes: <https://github.com/ip2k/lunar-modulator/blob/main/docs/07-recovery-and-risk.md>
- FM-1 Transporter: <https://github.com/kurogedelic/FM-1-transporter>
- MvaveFM1Unbricker: <https://github.com/Quixotic7/MvaveFM1Unbricker>

Hardware status: **NO VERIFIED**. The plan is not evidence that any of these steps succeeded.
