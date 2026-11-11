# FM-1: what is proven about rescue, what is only reported, and what may be touched

This project has no FM-1 on the bench. Everything below is either (a) something this repository can
demonstrate on a host, (b) a primary vendor document, or (c) a third-party report about this exact
product. The classes are kept apart on purpose: a report is not a recovery path, and an untested
recovery path is not a reason to flash the only unit.

The question this file answers is narrow: **before anyone writes to an FM-1, what is known about
getting it back?**

## 1. Audit trail

| Claim | Class | Where it comes from |
|---|---|---|
| The FM-1 is an M-VAVE product on a JieLi AC791N / WL82 SoC, 1 MiB SPI NOR | product/architecture | this repository, `docs/HARDWARE_SPECS_FM1.md`, `docs/TECHNICAL_OVERVIEW.md` |
| The official update channel is MIDI SysEx over USB; the app switches the unit into an `ota-*` personality with `F0 22 24 35 7F F7` | vendor + this repo | `web/fm1ota.js` (`UPGRADE`), `tools/fm1_install.py`; echoed by the community RE notes below |
| JieLi AC79 boards normally enter download mode with a **update button + reset** strap, and then enumerate as a mass-storage device `WL82 UBOOT1.00 USB Device` | vendor | JieLi AC79 SDK documentation, "2.3. USB 下载程序": <https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/getting_started/preparation/update.html> |
| Without a button, JieLi documents four ways in: blank flash (auto), shorting the NOR **MISO to GND at power-up**, the vendor's forced-upgrade tool, or a firmware that enables USB slave mode | vendor | same page as above |
| The FM-1 has no update/UBOOT button and no exposed JTAG/SWD/UART debug pads | third-party report | `aroum/fm1-custom-fw` `REVERSE_ENGINEERING.md`; `AL-255/FM-1-RE` `tools/README.md` |
| The mask ROM watches the USB data lines for a 16-bit key `0x16EF` at power-up; a "USB key" dongle on D+/D− puts the chip into UBOOT1.00 | third-party report, mechanism | `ip2k/lunar-modulator` `docs/10-usb-key-dongle.md`, which cites kagaimiq's `jielie/isp/usb/usb-key.md` |
| In UBOOT1.00 the chip speaks a JieLi SCSI vendor protocol and can only read/write **RAM** and jump; flash access needs `wl82loader.bin` loaded at `0x1C02000` | third-party, tool author | `kagaimiq/jl-uboot-tool` README (<https://github.com/kagaimiq/jl-uboot-tool>), which lists WL82 as "unknown" |
| FM-1 units have been seen as `4C4A:8057 WL80UBOOT1.00`, SCSI `WL82/UBOOT1.00`, and dumps/backups have been reported on them | third-party reports, **not reproduced here** | `ip2k/lunar-modulator` (as above), `FM-1-transporter`, `fm1-nes` |
| The normal MIDI protocol answers every flash-region read with error status 1, so the flash **cannot** be dumped through the ordinary firmware | third-party, verified by that author | `marco-jardim/mvave-irbox-tools` `docs/firmware.md` |
| `isd_download.exe` (JieLi's own tool) needs UBOOT mode, so it does not apply to an FM-1 in its normal state | third-party + vendor docs | `AL-255/FM-1-RE` commit `509d057`; JieLi download documentation |

## 2. What this means for the only FM-1 in the room

* **Reading is safe and does not need any of the above.** Plugging the unit in, enumerating USB, and
  pressing **Verify Only** sends a handshake and at most one INFO read (see
  `web/test_verify_only.mjs`): no upgrade command, no write, no mode change, and a timeout ends in a
  safe state. That is the only physical step this repository endorses without a second unit.
* **A full flash backup is not currently possible**, and not for lack of software: the running
  firmware refuses region reads, and the mask-ROM path needs (i) a hardware USB-key dongle and (ii)
  loading a loader blob into RAM, on a chip family the tool's own author lists as untested. Reported
  successes on FM-1s exist, but they were not produced here and must not be treated as a plan.
* **Sending `F0 22 24 35 7F F7` to explore is forbidden.** It reboots the unit into the `ota-*`
  personality, and the only documented way out is to finish an update. On a unit whose recovery
  path is unproven, that is the single most expensive command in the protocol.

## 3. Classification of operations

| Operation | Direction | Reversible? | Allowed today |
|---|---|---|---|
| USB enumeration, `lsusb`, `dmesg` | read | n/a | yes |
| MIDI handshake (`F0 00 32 45 … 7F F7`) | read | n/a | yes |
| INFO read on the backup channel (`F1 7D 46 4C 01 F7`) | read | n/a | yes (Verify Only) |
| JEDEC / chip probe (`0x12`) | read | n/a | yes, but only from the installer's own admission path |
| Project / preset backup (SysEx object reads) | read | n/a | yes |
| Flash write of a verified `.fwsc` | write | yes **only if** a mask-ROM or a stock package exists to go back to | **no**: no backup, no proven return path |
| `UPGRADE` (enter OTA personality) | state change | only by completing an update | **no** |
| Mask-ROM entry, loader load, `erasechip`, `write` at `0x0` | write / irreversible | no | **no** |

## 4. The first physical session, in order

1. **Second unit first.** Do the irreversible work on a unit that can be lost. Until then, step 2 is
   the whole session.
2. **Read-only reconnaissance** (no writes, no mode change):
   * Linux: `dmesg -w` in one terminal, plug the cable, note every enumeration: `4C4A:8057` means
     WL82 mask ROM, an `FM-1`/`FELUCCA` composite MIDI device means the app is running.
   * Installer → **Verify Only**; record what it prints (identity, Jiant/Felucca, version).
   * `python3 tools/fm1_install.py --help` for the CLI equivalent of the same read.
   * Write down the exact identity string, the version and the enumeration. That record is what makes
     a later "it changed" claim testable.
3. **Only with a backup in hand**: install, then verify, then verify again after a power cycle.

## 5. NO-GO conditions

Any one of these keeps the FM-1 off the flashing path:

* no flash backup that has been read back and hashed;
* no proven way to reach mask ROM on this unit (no dongle, or the dongle never tried);
* no second unit, and no stock `FM-1.fwsc` V15 package whose SHA-256 the installer knows;
* the installer, the CLI and Verify Only disagree about the identity or the version;
* the pin file has no key (`tools/fm1_keys.json`), i.e. every package is refused by design.

## 6. What would change the answer

* A read-back, hashed dump of the unit's 1 MiB NOR produced by the owner, with the geometry and
  JEDEC recorded (`docs/BACKUP.md` describes the format the project would accept).
* A proven mask-ROM entry on an FM-1 **second** unit, documented with the enumeration it produced.
* A stock V15 package whose digest matches the one the installer pins.

Until then the honest state is: **read-only is GO, flashing is NO-GO**, and the reason is hardware,
not software.
