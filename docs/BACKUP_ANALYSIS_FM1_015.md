# Stock FM-1 1 MiB Flash Partition Analysis & Recovery Requirements

## 1. Executive Summary & Hardware State

This document specifies the technical architecture, partition layout, and structural validation requirements for 1 MiB SPI NOR flash dumps of the M-VAVE FM-1 (AC791N SoC).

**Status of User Hardware:**
- **Unit Identity:** `FM-1_015` (verified via read-only SysEx handshake).
- **Physical Flash Chip:** Puya P25Q80H (`0x856014`) or UCUN UC25HQ80 (`0xB36014`) documented in supply chain; direct on-chip verification pending ROM UBOOT probe.
- **Physical Backup (Gate 7):** Pending physical execution by the user using M-Flasher 0.9.x (`NOT VERIFIED`).
- **Physical Installation (Gate 10):** Strictly **BLOCKED** (`INSTALL BLOCKED`). No write or erase operations are performed.

---

## 2. 1 MiB Flash Memory Map & Partitions

The M-VAVE FM-1 utilizes a 1 MiB (8 Mbit = 1,048,576 bytes) SPI NOR flash mapped via XIP (eXecute-In-Place) at base address `0x02000000`.

The standard stock partition layout decoded by `tools/fm1_image.py` comprises:

| Region | Physical Offset Range | Size | Purpose | QG Write Access |
|---|---|---|---|---|
| **Boot Area** | `[0x000000, 0x004000)` | 16 KiB (4 sectors) | Flash header, `uboot.boot`, `isd_config.ini`, `app_dir_head` | **NEVER WRITTEN** (Immutable) |
| **App Area** | `[0x004000, 0x093000)` | ~572 KiB | Firmware application binary (`app.bin`) & configuration | Overwritten by QG firmware during install |
| **Virtual Memory (VM)** | `[0x093000, 0x0EA000)` | ~348 KiB | System state, calibration, and song/pattern storage | Retained or formatted according to install profile |
| **User Storage (USR)** | `[0x0EA000, 0x100000)` | ~90 KiB | User sound presets and sample tables | Preserved / migrated to QG preset structure |

---

## 3. Structural Validation Requirements for Golden Stock Backup

To qualify a dump as an authentic, restorable golden backup without endangering the hardware, `tools/fm1_mflasher.py` and `tools/fm1_image.py` enforce:

1. **Exact Geometry & Capacity:**
   - File length must equal exactly `1,048,576` bytes (`0x100000`).
   - Sector alignment must match 4 KiB (`0x1000`) and block alignment 64 KiB (`0x10000`).

2. **Boot Area Integrity:**
   - First 32 bytes decrypted with key `0xFFFF` must pass CRC16 (`head == crc16(header[2:])`).
   - File entries starting at `0x20` must define valid JieLi boot records (`uboot.boot`).

3. **App Area Encryption & Decryption:**
   - App entries decrypted block-by-block using the chip key `0x980F ^ ((offset >> 2) & 0xFFFF)`.
   - File table must contain `app.bin` with valid CRC16/XMODEM matching its header.
   - Internal identity string inside `app.bin` must match regular expression `FM-1_(\d{3})` (e.g. `FM-1_015` or `FM-1_016`).

4. **Corroboration (Integrity vs Authenticity):**
   - SHA-256 confirms byte transfer integrity.
   - Whole-chip CRC32 computed by the JieLi on-chip hardware engine during SCSI read confirms non-tampered physical reading.

---

## 4. Recovery Guarantees

Because Quantum GammaWave never touches the Boot Area `[0x000000, 0x004000)`, an interrupted install or bad app build leaves the JieLi SoC able to reboot into ROM UBOOT mode (via key combination `OCT-` + `OCT+` + `HOME` upon USB insertion), allowing full restoration of the stock firmware via M-Flasher.
