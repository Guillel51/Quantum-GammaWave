# FM-1 hardware specifications — evidence and corrections

Status: **documentary and code review only. No physical FM-1 was opened, measured or flashed.**
This file records the specification set used for this pass and separates what the repository can
prove from what is reported by the SDK or by reverse-engineering projects.

## 1. Reference specifications

| Item | Reference used in this audit | Evidence in this repository | Status |
|---|---|---|---|
| SoC | JieLi AC791N, WL82 family | Linker, loader, SDK build and `firmware/hal/` target the AC791N/WL82; the exact die marking is not read | **REPORTED / NOT MEASURED** |
| CPU | `pi32v2`, 32-bit, little-endian, dual core | `CPU_CORE_NUM 2` in the SDK; `memory_map_test.py` reserves the second core's 7168 B stacks; `app.ld` splits core 0 and core 1 | **IMPLEMENTED / TARGET COMPILED** |
| FPU | single-precision hardware; `double` is software | The build does not claim a double-precision FPU; no cycle or precision measurement was made | **REPORTED / NOT MEASURED** |
| SRAM | 578 KB according to the SDK; firmware uses the `0x01C00000–0x01C7FFFF` window | Linker sub-windows: RAM `0x01C08000–0x01C20000`, pool to `0x01C70000`, RAMTEXT and stacks up to `0x01C7C000`. The project's `.data+.bss` budget is 96 KiB, not the whole SRAM | **IMPLEMENTED / TARGET COMPILED** |
| Flash | 1 MiB SPI NOR, XIP at `0x02000000` | `LDR_FL_MAX = 0x100000`, `LDR_APP_HI = 0x93000`, package image `0x93000`, emulator `nor[0x100000]` | **IMPLEMENTED / TARGET COMPILED; JEDEC NOT READ** |
| Flash IDs | `0x856014` (Puya P25Q80H, documented) and `0xB36014` on newer units | `firmware/hal/fm1_flash_id.h` accepts both documented 1 MiB IDs; `project.c` rejects unknown IDs and leaves the firmware RAM-only | `0x856014`: **DOCUMENTED, NOT VERIFIED**; `0xB36014`: **DOCUMENTED, NOT VERIFIED**; unknown: **REJECTED BY CODE** |
| Display | 240 × 240 TFT, ST7789-class, RGB565 | `lcd_st7789_test.c`, `lcd_st7789.c`, 240×240 framebuffer and RGB565 UI tests | **IMPLEMENTED / HOST TESTED; PANEL NOT VERIFIED** |
| Musical keys | 27 keys in two rows | `27 micro-switches` in `AGENTS.md`; keyboard/panel code | **REPORTED / PANEL NOT VERIFIED** |
| Buttons | 14 physical buttons | `enum { B_FX, ..., B_OCTUP, NB }` in `panel.c`: **NB = 14** | **IMPLEMENTED / HOST TESTED; PANEL NOT VERIFIED** |
| Encoders | 7 rotary encoders plus MASTER potentiometer | `enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE }`: **NE = 7**; MASTER is the analog pot in `master.c` | **IMPLEMENTED / HOST TESTED; PANEL NOT VERIFIED** |
| Audio | stereo output through ALNK0/I2S and the device output stage | HAL audio path and target build; no electrical or acoustic measurement | **IMPLEMENTED / NOT VERIFIED ON HARDWARE** |
| USB | USB 1.1 Full Speed device, USB-MIDI, MIDI TRS input by UART | USB-MIDI host tests; installer uses USB-MIDI SysEx; UART MIDI test | **IMPLEMENTED / HOST TESTED; USB NOT VERIFIED** |
| Radio | integrated Wi-Fi and Bluetooth, subject to SDK/firmware support | SDK libraries; the firmware does not expose a network feature | **SDK-DEPENDENT / NOT VERIFIED** |
| Clock | external reports of 360 MHz with the original bootloader and 320 MHz configurations | No clock measurement exists in this repository; `AGENTS.md`'s old 160–240 MHz claim was not supported by a measurement | **NOT FIXED / NOT MEASURED** |

### Important distinction

The four **HOME mini-knobs** are UI controls for KNOB 1–4. They are not the total number of physical
encoders. The panel has seven encoder roles; HOME only displays the four assigned to KNOB 1–4.

## 2. Flash map used by the audit

| Range | Documented use | Our write policy |
|---|---|---|
| `[0x00000, 0x04000)` | Header, configuration and bootloader (`uboot.boot`, `isd_config.ini`, chip key) | **Never written** by this project's safe path |
| `[0x04000, 0x93000)` | Application / firmware | Only authorized range; sector-aligned writes |
| `[0x93000, 0xE9000)` | VM / configuration and calibration | Preserve; backup only |
| `[0xE9000, 0xE9FFF)` | BTIF / Bluetooth pairing data | Preserve; backup only |
| `[0xEA000, 0xFBFFF)` | USR / resources and user data | Preserve; backup only |
| `[0xFC000, 0xFEFFF)` | Region used by some firmwares for their own data | Treat as occupied; preserve |
| `[0xFF000, 0xFFFFF)` | Factory reserve / `key_mac` | **Never written** |

The package produced here is `0x93000` bytes plus its wrapper; it does not claim the VM, BTIF, USR,
factory reserve or boot area. Gaps are **not** assumed free: other firmwares relocate their VM or
store their own data there.

## 3. Corrections made to repository documentation

- `AGENTS.md` now states the SDK's 578 KB SRAM together with the firmware's 96 KiB `.data+.bss`
  budget, instead of calling 96 KB the total SRAM.
- It now states **14 buttons, 7 encoders and a MASTER pot**, not 12 buttons and 8 encoders.
- The clock is no longer presented as a fixed 160–240 MHz value. The 320/360 MHz reports are
  explicitly marked as measurements to reproduce on the actual unit.
- The flash line records `0x856014` and `0xB36014` as the two documented 1 MiB IDs. The firmware
  accepts both through an explicit allowlist and rejects unknown IDs. Neither has been read from a
  physical unit; installing on either variant remains blocked until its own backup and recovery proof
  exists.
- `docs/TECHNICAL_OVERVIEW.md` follows the same distinction between SDK SRAM and the linker budget.

## 4. What is still unknown

- The exact AC791N die variant and package marking on the FM-1 PCB.
- The real JEDEC ID, chip key and write-protect/OTP state of the unit.
- The actual clock, PLL configuration, FPU behavior and measured `double` cost.
- SRAM high-water marks, stack usage, ISR latency, DMA arbitration and cache behavior.
- LCD controller variant and timing on the physical panel.
- Encoder direction/contact bounce on the physical panel; the logical mapping is tested only in the
  host and emulator.
- Wi-Fi/Bluetooth availability and regulatory behavior in the shipped firmware.

These are not cosmetic unknowns: the installation verdict remains **NO VERIFICADO EN HARDWARE — NO
APTO PARA INSTALACIÓN REAL** until the identity, backup and recovery checks in
`FM1_INSTALLATION_SAFETY_AUDIT.md` are performed on hardware.
