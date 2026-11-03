# FM-1 installation safety audit

**Date:** 2026-11-03
**Branch:** `perf/security-hardening`
**Base commit:** `4f6b6e6`; this pass is on `1e85888` plus the uncommitted audit patch (the final commit is recorded in the delivery message)
**Package:** `build/felucca.fwsc`, identity `FM-1_982`, **610897 B**
**Package SHA-256:** `932d37a0d5c5d74124713555f678804589ba42b8e129bce03e1d0762ac36be8d`
**Application:** `build/felucca.bin`, **549028 B**; **application hash:**
`0dcd80f23747832d7d440f9e992b032db7c40be297155c5ebe257c063512e67f`

## Verdict

# NO VERIFICADO EN HARDWARE — NO APTO PARA INSTALACIÓN REAL

There is no physical M-VAVE FM-1 in this workspace. No full flash backup was taken, no USB_KEY /
Mask-ROM recovery was demonstrated, and no package was written to hardware. A host build, emulator
run or simulation cannot establish that a real unit will boot, keep its calibration data or recover
from an interrupted single-bank update.

This is not a claim that the package is known to be malicious or incompatible. It is the required
classification when identity, backup and recovery evidence are missing. **No installation on a
physical FM-1 is authorized by this audit.**

## 1. Scope and sources

Audited:

- target build, linker map, package wrapper and application slot;
- `firmware/loader/ldr_core.c` and the simulated loader tests;
- `web/fm1pkg.js`, `web/fm1ota.js` and `tools/fm1_install.py`;
- `web/test_backup.mjs`, `tests/install_test.py` and `tests/backup_test.c`;
- the documented flash map, boot chain, chip-key handling and recovery ladder;
- the new HOME encoder → mini-knob path and its regression test.

Not available:

- a physical FM-1, a known-good full-flash image or a second unit;
- a USB_KEY dongle / Mask-ROM recovery setup;
- a power-cut test fixture, current probe, logic analyzer or thermal camera;
- GitHub credentials for opening or merging pull requests.

Secondary sources are marked as such. Community reverse-engineering is evidence of what those
projects observed, not a manufacturer guarantee and not a substitute for measuring this unit.

## 2. Build and static compatibility evidence

| Check | Evidence | Classification |
|---|---|---|
| Target architecture / linker | JieLi toolchain build completed; no calls or loads into the chip ROM; RAM code and XIP sections fit | **COMPILADA PARA TARGET** |
| Application size | `549028 B`; slot `0x8DFBC` / `581564 B`; unused **32536 B**; app area ends `0x920DC <= 0x93000` | **COMPILADA PARA TARGET** |
| Package size | `610897 B`; `flash.bin` image `0x93000` / `602112 B`; wrapper **8785 B**; identity `FM-1_982` | **COMPILADA PARA TARGET** |
| Static RAM | `.data+.bss` `94588 B` of the project's `98304 B` budget; pool spare `9248 B`; RAMTEXT spare `2404 B`; NOINIT spare `124 B` | **COMPILADA PARA TARGET** |
| Hot DSP path | `perf_block 569/522` (the project's existing +10 % tolerance); `DRUM_PATH 1189/1191` | **COMPILADA PARA TARGET** |
| UI render | 172 screens × 17 palettes, 0 lint, 0 alignment findings; splash 425 steps, 200 changing, 225 identical rewrites skipped | **TESTEADA EN HOST** |
| Encoder mapping | New test walks every available engine and every KNOB 1–4; each changes only its own HOME parameter and mini-knob | **TESTEADA EN HOST** |
| Emulator | WASM and native render the same audio; heavy song 2.2 % of real time | **SIMULADA** |
| Installer/backup/package bounds | `tests/groups.sh security`, `web`, `presets` pass; package CRC, model, SHA-256, disconnect/timeout and lost-final-ACK paths exercised against mocks | **SIMULADA** |
| Loader power-cut and ACK path | Loader tests simulate cuts at five write offsets, a wrong final ACK, record retention and later resume; this is a RAM-loader model, not the SPL or real erase/write timing | **SIMULADA** |
| Flash-ID allowlist | `tests/flash_id_test.c` accepts `0x856014` / `0xB36014` and refuses unknown IDs; `project.c` uses the same helper | **TESTEADA EN HOST** |
| Previous-package compatibility | `tests/ldr_test build/host/old.fwsc build/felucca.fwsc` updates an `FM-1_900` package, including five simulated power cuts | **SIMULADA** |
| Bootloader / clock / JEDEC / panel / audio / USB | No physical measurement | **NO VERIFICADA** |

The static budgets are not a measurement of board headroom. `perf_block` has only a few units of
margin under the existing ceiling; no new DSP work was added in this pass.

### Reconciliation of the reported sizes

- **`581564 B` is not the application size.** It is `APP_SLOT = 0x8DFBC`, the maximum XIP
  `app.bin` the linker/package format reserves. The current application is **549028 B**, leaving
  **32536 B** of the slot unused. The app area head is `0x120`; the app starts at `0x02000120`, ends
  at `0x920DC`, and the package image ends at `0x93000`.
- **`548708 B` was an earlier application baseline**, not this HEAD and not a second simultaneous
  build. The later HOME/splash pass produced `549012 B`; the JEDEC allowlist/ACK work in this pass
  produced **`549028 B`**.
- **`610876 B` was the previous `.fwsc`**; the current `.fwsc` is **`610897 B`**. The increase is
  +16 B in `app.bin`; the remaining +5 B is in the packaged loader/wrapper. The layout and the slot
  did not change.
- **`0x93000` is the packaged flash image**, not the physical 1 MiB part. The physical part is
  `0x100000`; the loader caps `flash.bin` at `LDR_FL_MAX = 0x100000`. The 8785 B above the image are
  the `.fwsc` wrapper/identity bytes.
- All figures above come from one `tools/build.py` run at this HEAD. The previous values are kept
  only as labelled history; they are not mixed into the current budget.

## 3. Package and write-boundary analysis

### Implemented checks

- SHA-256 is required by the host installer; a missing or mismatched hash is refused.
- The model identity is checked before writing and again after the restart.
- The package parser checks container and entry CRC-16 values, header fields, body lengths and
  bounds; malformed, truncated or oversized packages are rejected.
- The update loader writes only the application range `[0x4000, 0x93000)`, in 4 KiB sectors, and
  reads back each written sector.
- The loader refuses a package image larger than `0x100000` and an application beyond its slot.
- The loader accepts the final `0xF0000000` acknowledgement only when it is exactly `success\0`.
  A timeout, a missing reply or any other bytes return `-14` **before** the RAM update record is
  cleared; a later session can therefore resume instead of committing an unconfirmed update.
- Disconnect, lost link and silent-loader paths return an error instead of reporting success.
- Both host installers return `lost`/`noreturn` when the final ACK cannot be sent or the device does
  not come back; neither reports `done` in that case.
- The host no longer treats a failed `success` SysEx ACK as a completed step.

### What those checks do **not** provide

- **No cryptographic signature.** SHA-256 and CRC-16 detect accidental corruption; they do not prove
  who produced the package. A compromised site can replace the package and its published hash.
- **No anti-rollback and no authenticated boot.** A valid older package is still accepted.
- **No atomic update.** The FM-1 uses a single application bank. A cut during erase/write can leave
  the application incomplete. The stock loader writes in ascending order; once the first application
  sector is rewritten, the remaining one-to-two-minute window has no rescue code in the unit.
- **A success ACK is not an authorization.** The loader retries the final handshake, then returns
  `-14` unless it receives exactly `success\0`; a successful handshake still does not provide a
  signature or an end-to-end attestation.
- **The boot area is out of scope for a safe write.** `[0x00000, 0x04000)` must never be touched by
  this project's normal installation path.

### Write order and exposure window

The loader writes `[0x4000, 0x93000)` in ascending 4 KiB sectors. It first verifies the whole
package and every section CRC, skips sectors that already match, and re-reads a sector if the second
read disagrees with the checked pass. The first application sector contains the app-area head; once
it is rewritten, a power loss can leave no complete application bank.

I evaluated, but did **not** implement, moving the first sector to the end. That could keep the old
head valid longer, but the stock SPL's choice between the update record and the app head has not been
verified on hardware. Without that evidence it could strand the unit in the loader or commit a mixed
image. A/B banks, rollback and atomic commit would require bootloader support this package does not
have. The only host-verifiable reduction is the existing preflight/skip/verify work; the one-to-two
minute single-bank window is unchanged and remains a hardware risk.

### JEDEC and chip-key gate

`firmware/hal/fm1_flash_id.h` is the explicit allowlist used by `firmware/src/project.c`:
`0x856014` (documented P25Q80H) and `0xB36014` (documented newer variant) pass; any other 24-bit ID
leaves the firmware RAM-only. Both accepted IDs describe a 1 MiB part, but **neither has been read
from a physical FM-1 in this workspace**. The two parts are not assumed interchangeable for
write-protect, OTP, timing or persistence behavior until each is measured. The chip key used by the
package is documented as `0x980F`; it was not read from a physical unit or independently verified
here.

## 4. Mandatory backup and write protocol (pending)

This is the minimum procedure before any installation on a real unit. It is **not** a claim that it
has been executed.

1. **Identify:** record the model, PCB revision, SoC marking, flash marking, JEDEC ID, chip key and
   USB descriptors. Stop on any mismatch.
2. **Read twice:** take two complete, independent 1 MiB reads with separate tool sessions. A single
   read is not a backup.
3. **Compare:** compare the two images byte for byte; compute SHA-256 and CRC-32/CRC-16 identifiers
   and store them next to the images.
4. **Preserve:** keep at least one copy outside the instrument, with the unit's identity and tool
   versions. Do **not** publish a full dump: it contains calibration, MAC/pairing and user data.
5. **Write only authorized regions:** only `[0x04000, 0x93000)`, sector-aligned, and only sectors that
   differ. Never erase the whole flash as a normal update method. Never write
   `[0x00000, 0x04000)`, VM, BTIF, USR or the factory reserve.
6. **Read back:** read every modified sector again and compare it byte for byte with the intended
   image. A whole-image hash alone is not enough to detect a stale sector.
7. **Verify preservation:** compare the protected regions against the backup. If any byte differs,
   stop and recover before booting.

## 5. Recovery analysis

| Situation | Mechanism | Status |
|---|---|---|
| Application runs but is broken | Device update mode, then reinstall a known package | Documented; **NO VERIFICADA** on this unit |
| Application crashes twice | Family-specific safe mode with USB alive | Documented for related firmwares; **NO VERIFICADA** |
| Device enumerates as `4C4A:8057` / `WL82 UBOOT` | Mask-ROM download mode; flash a full backup or a known image | Documented; **NO VERIFICADA** |
| No USB response, boot area intact | USB_KEY dongle forces Mask-ROM download | **NO VERIFICADA**; requires a dongle and a known-good image |
| Bootloader area damaged | No demonstrated recovery | **NO VERIFICADA / no rollback** |
| Power loss during the write | Single-bank write can leave no bootable application | **FALLIDA COMO GARANTÍA**; no automatic rollback |

Entering update mode from a working application is not equivalent to forcing Mask-ROM mode when the
application no longer responds. The OTA/SysEx protocol is an update mechanism, not a recovery
mechanism. A full backup restore still depends on a working independent download path.

Community tools referenced by the audit:

- [M-Flasher](https://github.com/jvitkauskas/m-flasher) — backup/restore and firmware installation
  over USB.
- [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) — RP2040 USB_KEY transport.
- [MvaveFM1Unbricker](https://github.com/Quixotic7/MvaveFM1Unbricker) — soft-brick recovery over a
  plain USB cable.
- [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) — general JieLi UBOOT tooling; its
  device table is not a guarantee for the FM-1.
- [FM-1 reverse engineering](https://github.com/AL-255/FM-1-RE) and the
  [FM-1 developers' wiki](https://github.com/sbegueria/fm1-dev-wiki).

These are secondary community sources. Their successful procedures do not transfer automatically to
this unit, this package or this bootloader revision.

## 6. Risks

### Critical

- **No physical recovery demonstrated.** A bad write can leave the unit dark; no USB_KEY recovery was
  exercised here.
- **Single-bank update.** There is no A/B bank, atomic commit or automatic rollback.
- **No signature/anti-rollback.** Integrity hashes are not provenance or boot authorization.
- **`0xB36014` is accepted by the code but not validated on hardware.** The allowlist removes the
  artificial `0x856014`-only rejection, but it does not prove that this batch's geometry,
  write-protect state or timing match. Do not install on it until its own backup and recovery proof
  exists; an unknown ID is still rejected.

### High

- **Boot area exposure.** A tool that rewrites the header/SPL can brick the unit even if the
  application sectors are written correctly.
- **Power loss / disconnect** during the write leaves a variable, hardware-dependent window.
- **No measured stack, ISR, DMA or cache behavior.** Static margin is not a timing guarantee.
- **`perf_block` is close to its established ceiling.** Do not add DSP work without target
  measurement.
- **User data loss.** VM, BTIF, USR and factory calibration must be preserved; a full-flash restore
  from another unit is unacceptable.

### Medium

- The exact clock (320/360 MHz reports) is not fixed by this audit.
- The ST7789-class panel, encoder bounce/direction and USB behavior are unverified on metal.
- The bootloader and ROM revision may differ between production batches.
- A successful host/emulator test cannot validate analog audio, latch-up, current draw or thermal
  behavior.

## 7. Classification of tests

| Test | Classification |
|---|---|
| Target build and linker/memory checks | **COMPILADA PARA TARGET** |
| UI, encoder mapping, splash render, presets, web, security and stress hosts | **TESTEADA EN HOST** |
| WASM emulator and native bit-identical audio | **SIMULADA** |
| Installer, backup, package-bounds and loader power-cut harnesses | **SIMULADA** |
| JEDEC/chip-key read on a physical unit | **NO VERIFICADA** |
| Cold boot, LCD, buttons, encoders, USB, MIDI, audio | **NO VERIFICADA** |
| Two independent full-flash reads and comparison | **NO VERIFICADA** |
| Controlled power-cut run with demonstrated recovery | **NO VERIFICADA** |
| USB_KEY / Mask-ROM recovery | **NO VERIFICADA** |
| Installation on `0xB36014` flash | **NO VERIFICADA** (accepted by the static allowlist) |

## 8. Conditions to change the verdict

The verdict can move to **APTO** only after all of the following are evidenced on a real FM-1,
preferably a sacrificial test unit:

1. Identity and JEDEC are recorded; `0x856014` and `0xB36014` are each validated on their own unit
   and path (the shared code allowlist is not a hardware validation).
2. Two independent 1 MiB reads match byte for byte and their SHA-256 is archived.
3. USB_KEY / Mask-ROM recovery is demonstrated from a deliberately non-booting application state.
4. A known-good backup is restored and verified byte for byte in the protected regions.
5. The package is installed, the unit cold-boots, and screen, encoders, keys, USB, MIDI and audio
   pass the hardware plan.
6. Controlled power cuts at multiple write offsets are recovered, with the boot area intact.
7. A long soak, current/thermal check and DSP load measurement pass.
8. The package is signed or otherwise authenticated, or the residual no-signature risk is explicitly
   accepted for a non-critical unit.

Until then the only correct classification is **NO VERIFICADO EN HARDWARE — NO APTO PARA
INSTALACIÓN REAL**. There is no "100 % safe" claim in this repository, and no emulator result is a
hardware safety proof.

## 9. Evidence files

- `docs/SECURITY.md` — flash map, boot chain, OTA protocol, installer checks and sources.
- `docs/HARDWARE_TEST_PLAN.md` — physical test order and stop conditions.
- `docs/HARDWARE_SPECS_FM1.md` — specification contrast and corrected hardware facts.
- `docs/PERF_SECURITY_AUDIT.md` — host/target measurements and residual performance risk.
- `docs/AUDIT_FINAL_FM1.md` — prior audit, updated with this pass.
- `firmware/loader/ldr_core.c` — application-only write path.
- `firmware/hal/fm1_flash_id.h` and `firmware/src/project.c` — explicit 1 MiB JEDEC allowlist.
- `web/fm1pkg.js`, `web/fm1ota.js`, `tools/fm1_install.py` — package and host validation.
- `tests/ui_test.c` — HOME encoder/mini-knob regression.
- `tests/ldr_test.c`, `tests/install_test.py`, `web/test_backup.mjs`, `tests/flash_id_test.c` — simulated failure paths and the static JEDEC allowlist.
