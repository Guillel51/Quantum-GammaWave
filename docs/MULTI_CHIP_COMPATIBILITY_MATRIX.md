# Multi-Chip SPI NOR Compatibility Matrix (FM-1 / AC791N)

## 1. Overview & Operational Policy

The JieLi AC791N SoC interfaces with an external SPI NOR flash via its Serial Flash Controller (SFC) mapped to XIP space `0x02000000`. Across different production runs of the M-VAVE FM-1 and related JieLi hardware, multiple 1 MiB (8 Mbit) flash parts have been documented.

**Strict Security & Write Policy:**
- **Reading / Diagnostics / Backup (`fl_jedec_readable`):** Permitted for all documented 1 MiB SPI NOR chips with standard command sets (0x9F, 0x03/0x0B).
- **Writing / Erasing / Installation (`fl_jedec_writable`):** Strictly restricted to verified production parts (Puya P25Q80H and UCUN UC25HQ80). Any other chip triggers `FL_ID_UNKNOWN` / `FL_ID_PROTECTED` / `chip_read_only` refusal, protecting unverified units from brick risks.

---

## 2. Chip Compatibility Matrix

| JEDEC ID | Manufacturer | Part Number | Density | Page / Sector / Block | SFC Commands (Read/Prog/Erase) | Read / Diag | Backup | Write App | Bootloader Write | QG Write Authorised |
|---|---|---|---|---|---|---|---|---|---|---|
| `0x856014` | Puya | P25Q80H | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **YES** | **NO** (Never) | **YES** (OEM Proven) |
| `0xB36014` | UCUN | UC25HQ80 | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **YES** | **NO** (Never) | **YES** (Field Proven) |
| `0xC84014` | GigaDevice | GD25Q80E/C | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **BLOCKED** | **NO** (Never) | **NO** (Lab verification needed) |
| `0xEF4014` | Winbond | W25Q80DV/DL | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **BLOCKED** | **NO** (Never) | **NO** (Lab verification needed) |
| `0x5E4014` | Zbit | ZB25VQ80 | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **BLOCKED** | **NO** (Never) | **NO** (Lab verification needed) |
| `0x684014` | BoyaMicro | BY25D80 | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **BLOCKED** | **NO** (Never) | **NO** (Lab verification needed) |
| `0x204014` | XMC | XM25QH80B | 1 MiB | 256 B / 4 KiB / 64 KiB | 03h, 0Bh / 02h / 20h, D8h | **YES** | **YES** | **BLOCKED** | **NO** (Never) | **NO** (Lab verification needed) |

---

## 3. Flash Geometry & Command Standardisation

All admitted chips share:
- Capacity: 1,048,576 bytes ($2^{20}$ bytes, capacity byte `0x14`).
- Page size: 256 bytes (standard SPI page programming).
- Sector size: 4,096 bytes (4 KiB sector erase opcode `0x20`).
- Block size: 65,536 bytes (64 KiB block erase opcode `0xD8`).
- Identification opcode: `0x9F` (JEDEC standard 3-byte response).
- Status Register 1: WIP (bit 0), WEL (bit 1), BP0..BP2 (bits 2..4).
