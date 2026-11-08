# M-Flasher: the first backup on a stock FM-1, and the first install

M-Flasher is the only tool that reads the whole flash of a **stock V15 unit over its own USB cable**,
without opening the case, an SOIC clip or a separate programmer. That makes it the answer to the two
questions this project could not answer on its own: how a stock unit is backed up before anything is
written, and how the first QG-1 image gets onto it at all.

It is a third-party program: MIT, (c) jvitkauskas, <https://github.com/jvitkauskas/m-flasher>. This
project does not redistribute it, does not shell out to it and does not depend on it at runtime. What
this repository has is an importer (`tools/fm1_mflasher.py`) that takes the two files M-Flasher writes
and proves, from their bytes, what they are.

- Versions audited: **0.9.1** (tag `v0.9.1`) and **0.9.x / 0.9.4** line.
- Source read: `src/jl.rs` (ROM protocol and loader), `src/ops.rs` (backup, restore, install plans),
  `src/formats.rs` (boot and app tables, CRCs, ENC), `src/boards.rs` (model evidence, flash ids),
  `src/midi.rs` (soft key), `src/scsi.rs` (transport), `docs/safety.md`.
- Tool compatibility in `tools/fm1_mflasher.py`: accepts all M-Flasher records from 0.9.x series
  (`TOOL_RE = re.compile(r"^m-flasher (\d+)\.(\d+)\.(\d+)$")`, version >= 0.9.0).
- Operational verification: M-Flasher non-destructively reads 1 MiB via ROM UBOOT SCSI mode,
  loads RAM loader into volatile memory (`0x01C20000`), and computes whole-chip hardware CRC32.

## 1. How it reaches a unit that is not running our firmware

| Step | What M-Flasher sends or does | Where in the source |
| --- | --- | --- |
| Soft key | SysEx `F0 22 24 35 7D F7` over MIDI; the stock firmware reboots into the chip's ROM download mode | `src/midi.rs:16` |
| Physical alternative | Hold `OCT-`, `OCT+` and `HOME`, then power on with `OCT-` (the combination varies with firmware) | MvaveFM1Unbricker's `UNBRICK-GUIDE.md` |
| Identify the mode | SCSI `INQUIRY` must answer vendor `WL82`, product `UBOOT1.00` | `src/scsi.rs:12`, `src/jl.rs:130` |
| Load a loader | 512-byte blocks through `WRITE_MEMORY` (`0xFB06`), each scrambled, to **RAM** at `0x01C20000` (or `0x01C28000` if the other slot is busy), then `JUMP` (`0xFB08`) with argument 1 = SPI NOR | `src/jl.rs:242-300` |
| Read the part | `READ_FLASH` (`0xFD05`), 16 KiB per command, whole 1 MiB | `src/jl.rs:391-420` |
| Check the read | `CRC32` (`0xFC41`) **computed on the chip** over the whole part, compared with the host's CRC32 of the image | `src/ops.rs:173-190` |
| Identify the unit | chip key `READ_KEY` (`0xFC09`) at `0xAC6900`, flash id `ONLINE_DEVICE` (`0xFC0A`), unique id (`0xFC42`) | `src/jl.rs:214-222` |
| Model | boot table + bootloader container + app layout + flash id, with the evidence listed | `src/boards.rs:80-200` |

The loader lives in RAM only: it is written to the two 32 KiB slots M-Flasher considers safe
(`0x01C20000..0x01C30000`) and disappears on power-off. `run-ram` refuses to overwrite the slot a
running loader occupies, and refuses the ROM's own erase, program and jump commands so a user program
cannot use it as a back door (`src/jl.rs:61-80`, `src/jl.rs:370-385`).

## 2. What the backup action does — and what it does not

`Session::open` (`src/ops.rs:153-190`) reads the part once and compares it with the chip's CRC32. If
they differ, it reads again and refuses if the two reads (or the CRC32) disagree. `Session::save_backup`
(`src/ops.rs:238-264`) writes `<model>-<stamp>.bin` and its `.json`, nothing else.

`erase` and `program` are called from exactly one place in the whole program: `Session::apply`
(`src/ops.rs:461-500`), which only runs for a restore, a firmware install or a bootloader install that
the user confirmed. The backup path cannot reach them. `apply` also saves a backup before its first
write if the session has none (`src/ops.rs:466`), so even the write paths start from a read.

That is what makes the backup action trustworthy for reading, and it is also its limit: the proof is
the chip's CRC32, and the `.json` does **not** record the CRC32 value, nor a count of writes, nor the
tool's own claim that it wrote nothing. The `.json` holds `tool`, `model`, `bootloader`, `firmware`,
`app`, `chip key`, `flash`, `unit id`, `loader`, `saved` and `sha256` (`src/ops.rs:201-256`).

## 3. What this project adds: the importer

`tools/fm1_mflasher.py` never runs M-Flasher. It reads the pair (image + `.json`) and refuses anything
that does not prove itself:

- the `.json` was written by `m-flasher 0.9.x` and names the expected fields;
- the image is exactly `0x100000` bytes and its SHA-256 is the `.json`'s;
- the flash id is a documented 1 MiB part; the chip key is `0x980F` (otherwise this project cannot
  decode the unit's app area, and says so instead of guessing);
- the boot header, the boot table, the app table and the CRC16 of every file in it verify
  (`tools/fm1_image.py`, checked against the public stock dump);
- the firmware in the image declares the FM-1 layout (not the SMK-37's);
- the identity comes out of the **decrypted `app.bin`**, not from the `.json` and not from the file
  name: a stock image has no `FM-1_` text in its raw bytes at all;
- completeness needs a second independent backup that is byte-identical, or the chip's CRC32 over the
  whole part. One read with neither is written and marked `unverifiable`, and `fm1_restore.py` refuses
  it.

The archive it writes is the same `qg1-backup-1` format the rest of the tooling already validates, with
`source: mflasher-ram-loader`, `programmer.flash_writes: 0`, the RAM writes recorded explicitly, and a
`declared` block that keeps M-Flasher's own statements separate from what this project verified.

```
m-flasher backup ~/fm1-backup          # twice, on two runs
fm1_mflasher.py import \
  --bin ~/fm1-backup/fm1-20261106-101500.bin \
  --second ~/fm1-backup/fm1-20261106-102230.bin \
  --out backup/stock-01
fm1_flash_evidence.py check backup/stock-01 --json backup/stock-01/evidence.json
```

With a single backup, pass `--crc32` with the value M-Flasher prints for the whole flash. Keep the
backup outside the repository: it contains the unit's settings, its user area and its Bluetooth data.

## 4. What the stock image actually says

Checked against the public dump from another unit
(<https://github.com/jvitkauskas/fm1-backup>, SHA-256
`3d5315f924147ded9f43e1da0d160e434ab0aa03fdc3ad55e888b64b87591d72`), with `tools/fm1_image.py`:

| Fact | Value |
| --- | --- |
| boot table | `uboot.boot` at `0xA0` (14384 B), `isd_config.ini` at `0x38D0` (699 B), `app_dir_head`, `key_mac` |
| bootloader | stock M-VAVE (valid BANKCB container, no `AC79BLID`) |
| app table | `app.bin` 580892 B CRC16 `0xBD93`, `cfg_tool.bin` 383 B CRC16 `0x2E75`, then VM/BTIF/USR |
| identity | `FM-1_015`, decoded from `app.bin` with chip key `0x980F` |
| layout | FM-1 #1: VM `0x93000`, BTIF `0xE9000`, USR `0xEA000` |
| raw `FM-1_` text | **none** — the identity is encrypted in `app.bin` |

The published `FM-1_015.fwsc` carries a different `app.bin` (581564 B, CRC16 `0x5D48`): that unit runs
a factory build that reports 015. A backup is the only copy of it, which is the whole argument for
taking one before writing anything.

## 5. Compatibility matrix by JEDEC

The four questions are different, and this project answers them separately. "Readable" never implies
"writable", and "written by another tool" never implies "validated here".

| JEDEC | Part | Read | Backup (read twice + CRC32) | Write app area | m-bootloader / boot area | QG-1 install through this project's installer |
| --- | --- | --- | --- | --- | --- | --- |
| `0x856014` | Puya P25Q80H | yes — documented OEM part | yes | yes | yes (stock bootloader only; m-bootloader is a separate decision) | allowed by `fm1_gate`; still needs a signed package and `0x12` on that unit |
| `0xB36014` | UCUN UC25HQ80 | yes | yes | done by other projects through the same ROM loader (MvaveFM1Unbricker requires it) | **not written by this project**: the boot area is out of scope on every part | **allowed by `fm1_gate`**: datasheet parity plus a third-party FM-1 that installed a full image and kept its data across a power cycle (docs/JEDEC_COMPATIBILITY.md); still needs a signed package and `0x12` on that unit |

The same table exists in code — `python3 tools/fm1_gate.py` prints it, `fm1_gate.CAPABILITIES` is what
the installer consults, and `tests/gate_test.py` fails if the two ever disagree.

Rows are promoted by evidence, and the full record for `0xB36014` — geometry, command set, the
polled-timing argument and the third-party install with persistence — is in
[JEDEC_COMPATIBILITY.md](JEDEC_COMPATIBILITY.md). A part becomes writable when a unit with that part
has taken a whole image and kept its data; for `0xB36014` that record exists (another project's
installer, same part, same ROM loader family), and the boot area stays out of scope on every part
because the one bad field report was a bootloader install. A QG-1 install on that unit is still
unproven, and the gate keeps asking for this unit's own `0x12` and a signed package.

## 6. The first install, and why the installer still says no

The QG-1 installer requires `0x12` from the unit it is about to write. A stock V15 does not answer it —
that is the circularity this whole document is about. So:

- **The first QG-1 image on a stock unit is not installed by this project's installer.** It is installed
  by M-Flasher's *Install firmware*, which takes the same `.fwsc`, checks that the model matches, that
  the firmware decrypts with the unit's chip key, that the app files' CRC16s hold, writes only the app
  area (`0x4000..0x92FFF`) and keeps the unit's bootloader and factory data (`src/ops.rs:317-400`). It
  also saves a backup first if the session has none.
- **After that**, the unit runs QG-1, answers `0x12`, and this project's installer is what updates it.
  Its gate is unchanged: `0x12` on this unit, a part in the write allowlist, and a signed package.
- **A stock unit cannot be installed by this project's installer at all**, and no flag changes that.
  `--jedec`, `--allow-unidentified-chip` and `FM1_LAB` were removed in this change.

That also decides what "enabling the install" means: the code path is complete and opens by itself the
moment the two facts it needs exist (a pinned production key and a unit that answers `0x12` with a
writable part). It cannot be opened by a flag, and it must not be opened for a unit whose chip was never
read.

## 7. Recovery: what can bring back what

| Tool | Reaches a unit that… | Writes | Covers damage to `0x0000..0x3FFF`? |
| --- | --- | --- | --- |
| **M-Flasher restore** | enumerates as `WL82 UBOOT1.00` | the whole part if the backup is this unit's; otherwise everything except the factory data | **yes**, when restoring this unit's own full backup |
| **MvaveFM1Unbricker** | enumerates as `WL82 UBOOT1.00` | `0x4000..0x92FFF` only, verified byte for byte | **no** — it says so itself |
| **FM-1 Transporter** | does not enumerate at all | dump and write through the ROM loader, driven by an RP2040 on the USB lines | only what its write path covers; its own docs say no boot-head writes |
| **jl-uboot-tool / wl82loader** | the ROM download mode | a loader in RAM; writes nothing by itself | no |
| this project's `fm1_restore.py --write` | a part on a programmer (`file:`/`flashrom:`) | the whole image with per-sector read-back | yes, but only with an external programmer; **never run on hardware here** |

The mask ROM is what makes any of this possible: it cannot be erased by a firmware update, so a unit
with a damaged app area still answers. A damaged boot area is the one case where only a full-part
restore from that unit's own backup helps, and where nothing in this repository has been demonstrated
on hardware.

## 8. What is still not proven

- No step of this page was executed against a physical FM-1 here. The importer is proven against the
  public stock dump and synthetic images; the ROM protocol is proven by reading its source, not by
  running it.
- The `.json` cannot prove that a read was honest on its own; the chip's CRC32 or a second backup is
  what does, and the importer refuses anything less.
- Restoring a full backup has not been exercised on hardware by this project, and neither has a
  bootloader install. Both remain the owner's decision, with a backup in hand.
- M-Flasher's own restore keeps the target unit's factory data when the backup is from another unit;
  this project's `fm1_restore.py` does not implement that distinction, because it writes through a
  programmer to a part that is off the board.
