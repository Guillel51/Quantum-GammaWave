# M-Flasher: the first backup on a stock FM-1, and the first install

M-Flasher is the only tool that reads the whole flash of a **stock V15 unit over its own USB cable**,
without opening the case, an SOIC clip or a separate programmer. That makes it the answer to the two
questions this project could not answer on its own: how a stock unit is backed up before anything is
written, and how the first QG-1 image gets onto it at all.

It is a third-party program: MIT, (c) jvitkauskas, <https://github.com/jvitkauskas/m-flasher>. This
project does not redistribute it, does not shell out to it and does not depend on it at runtime. What
this repository has is an importer (`tools/fm1_mflasher.py`) that takes the two files M-Flasher writes
and proves, from their bytes, what they are.

- Version audited: **0.9.1**, tag `v0.9.1`, commit `9da825b2000143d89e8ca7ac016f35abe304158c`, 2026-11-06.
- **0.9.4 re-checked on 2026-11-09**: `src/ops.rs` is byte-identical between `v0.9.1` and `v0.9.4` (the
  tag comparison lists 27 commits and 35 files between them, none of them `ops.rs`), so the record format,
  the digest, the read checks and the install/restore plans are the same code. What changed in 0.9.4 is the
  bundled m-bootloader (2.1 -> 2.3) and a web build, neither of which touches this page.
- The `0.9.x` line is accepted by `tools/fm1_mflasher.py` through
  `TOOL_RE = re.compile(r"^m-flasher (\d+)\.(\d+)\.(\d+)$")` with version >= 0.9.0.
- Source read: `src/jl.rs` (ROM protocol and loader), `src/ops.rs` (backup, restore, install plans),
  `src/formats.rs` (boot and app tables, CRCs, ENC), `src/boards.rs` (model evidence, flash ids),
  `src/midi.rs` (soft key), `src/scsi.rs` (transport), `docs/safety.md`.
- What that source does, as read: it reads 1 MiB through ROM UBOOT SCSI mode with a loader that lives in
  RAM (`0x01C20000`) and checks the read against the CRC32 the chip computes over the whole part.
- Nothing in this page was confirmed on hardware. It is an audit of the source plus a check of the
  public stock dump against our own decoder; entering ROM download mode is not something this project has
  done to a unit.

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

### What the `sha256` in that record is — read, not assumed

`Session::save_backup` writes `&self.flash` to the `.bin` and hashes **that same buffer**:

```rust
std::fs::write(&path, &self.flash)?;
let sha: String = Sha256::digest(&self.flash)...
```

`Session::open` fills `self.flash` from address 0 for `FLASH_SIZE`, and `boards.rs` defines
`FLASH_SIZE = 0x100000`. So the field is the **SHA-256 of the whole 1 MiB read image** — not of the app
area, not of a file name, not of the chip's whole physical part if it were larger. Two consequences
that matter for this project's gate:

- A dump whose SHA-256 equals the record's is *that read*, byte for byte. That is a check of the file
  against the record, and nothing more: it is what `web/fm1record.js` can re-derive in the page.
- It is **not** a proof that the read was complete or consistent. That proof is the chip's CRC32 (or a
  second read), and M-Flasher performs it *inside the tool*, before it ever writes the `.json`; the
  value is not stored. `tools/fm1_mflasher.py import` therefore demands a second, byte-identical backup
  (`--second`) or the CRC32 (`--crc32`) before it calls an archive verified.

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

## 6. The first install, and what the browser installer does instead of it

The transports behind this section — why the ROM mode is reachable from a host program and not from
the page, what the record's `sha256` covers, and what the m-bootloader RAM-stage route costs — are
laid out with their evidence classes in `docs/FM1_MODES.md`.

The QG-1 installer writes through a loader it stages with `UPGRADE`, and the chip gate that protects that
write is answered by `0x12`. Two facts, both from this repository's own firmware, decide what that means:

- A **stock** unit's application does not answer `0x12`: the command is this project's extension. Its own
  update mode is the vendor's program (`ota-FM-1_015`), and nothing here has seen that program accept a
  Felucca image. Feeding it one is the one action that could leave the app area half-written by a loader
  that validates a different format.
- This project's loader (`ota-FM-1_900`) is built with `FELUCCA_LOADER=1`, which compiles the query out on
  purpose (`firmware/src/ota.c`, `docs/IDENTIFICATION.md`). It cannot answer `0x12` either — and asking it
  to is what made "the unit is in update mode and the installer does not recognise it" a dead end.

So the installer now does three different things, and says which one it did:

| Unit answers as | What the page does |
|---|---|
| `FM-1_9NN`, answers `0x12` | normal update: `0x12` twice on the app link, `UPGRADE`, the loader, the write. The loader link takes the chip fact from **that same attempt's app-link reading** (a `0x12` is still tried first, so a future loader that answers one decides). |
| `ota-FM-1_900` (this project's loader, an interrupted install) | resumes. With no chip fact available it refuses (`loaderchip`) and asks for the M-Flasher record + dump, which is enough: the part identity does not depend on which program is running. |
| `ota-FM-1_015` (the unit's own update mode) | refuses (`foreignloader`) with zero frames sent: a Felucca image must not be offered to it. |
| `FM-1_015` (stock application) | refuses (`stockfirst`), opens the M-Flasher panel, and sends nothing. |

**The first install is M-Flasher's.** `Install firmware` takes the same `.fwsc`, checks the model, the chip
key and every app file's CRC16, plans the write sector by sector (§11 traces those checks on this
release, one by one), and touches `[0x4000, len)` only — the
bootloader, the SPL head and `isd_config` stay. It is a third-party program this project does not
redistribute; what this project did is audit it and wrap its *record* (this page), not its writer.
`plan_firmware` defaults to `[BOOT_AREA, len)`, so the boot area is kept unless the package itself carries a
bootloader — and the factory ranges (`0xFF000..0x100000` on the FM-1) are **not** masked out of an ordinary
firmware install: they are masked only when a backup identified as *another unit's* is restored. A QG
package does not reach `0xFF000`, and that is a property of the package, checked here, not of the tool.

After that first install the unit runs QG, answers `0x12`, and this page is its updater. That is the
division of labour the evidence supports today; a browser first install is not "a flag away", it is a
different program on the unit that nobody has read.

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

## 9. The ten questions about a full backup, answered from the source

Answered about **M-Flasher 0.9.4's own code** and this repository's flash map. Nothing below was
executed against a unit here; the one reading of a physical unit this project has is the record the
owner's own M-Flasher wrote (`fm1-20261108-161613.json`), which proves the tool opened that unit's ROM
mode and identified the part — not that a `.bin` was kept or is consistent.

1. **Can it read all 1 MiB on a stock unit?** Yes, by construction: `Session::open` reads
   `0..FLASH_SIZE` (`0x100000`) with `READ_FLASH` through the ROM's UBOOT SCSI mode and a loader it puts
   in RAM. The stock application's own command set is not used at all.
2. **Without modifying the bootloader?** Yes. The loader is written to RAM (`0x01C20000`, or
   `0x01C28000`), and `apply()` — the only function that erases or programs — is reached from restore and
   install only. The backup path cannot reach it.
3. **Can it enter download mode over USB-MIDI?** Yes: SysEx `F0 22 24 35 7D F7` (`src/midi.rs`) asks the
   running stock firmware to reboot into the chip's ROM mode, where the unit enumerates as `WL82
   UBOOT1.00` (SCSI). The physical alternative is the documented key combination at power-on.
4. **Which operations are strictly read-only?** `INQUIRY`; `WRITE_MEMORY` (`0xFB06`) and `JUMP`
   (`0xFB08`) *to RAM only*; `READ_FLASH` (`0xFD05`, 16 KiB per command); the chip's `CRC32` (`0xFC41`);
   `READ_KEY` (`0xFC09`), `ONLINE_DEVICE` (`0xFC0A`) and the unique id (`0xFC42`).
5. **Which must be avoided?** The loader's erase and program paths (only a restore or an install plan
   reaches them), anything that writes `0x0000..0x3FFF` (SPL head, `isd_config`, bootloader) — no tool in
   this repository writes it except a full-part restore of the unit's own backup — and the vendor flash
   commands sent by *this* project, which it never sends.
6. **What does the 1 MiB contain?** Per [FLASH_MAP_AND_RECOVERY_FORENSICS.md](FLASH_MAP_AND_RECOVERY_FORENSICS.md):
   head and bootloader `0x0000..0x3FFF`; application `0x4000..0x92FFF`; VM `0x93000`; autosave A/B and the
   FM6 voice banks `0xE5000..0xE8000`; BTIF `0xE9000`; USR `0xEA000`; factory data `0xFF000..0x100000`;
   the undocumented `0xFC000..0xFF000` window.
7. **What personal data does it contain?** The user area: songs, presets, the FM6 banks, the autosave
   state, and the Bluetooth pairing block. That is why `backup/` is git-ignored and why the images are
   never attached to an issue.
8. **How is a file known to be complete?** By the chip's CRC32 over the whole part, compared with the
   host's CRC32 of the read; when the CRC is unavailable or disagrees, by a second full read that must be
   byte-identical (`Session::open`). The `.json` stores neither the CRC32 nor a write count.
9. **How are two independent reads compared?** Run `backup` twice with the unit replugged in between, then
   compare byte for byte and by SHA-256. `tools/fm1_mflasher.py import --second` is exactly that check, and
   `--crc32` is the other way to satisfy it. A copy of one file is not a second read.
10. **What are the limits of restoring it?** A restore of *this unit's* backup can write the whole part; a
    backup identified as another unit's keeps the target's factory ranges. It needs the unit to enumerate
    in ROM mode, and a damaged boot area is recoverable only from the unit's own full dump — a case nothing
    in this repository has demonstrated on hardware.

The practical order that follows, for a stock unit: two `backup` runs (read-only, no bootloader change),
compare them, import the pair, and only then decide about any write.

## 10. The Windows 10 procedure, step by step, and what each step proves

Written for the unit whose record is `fm1-20261109-102008.json` / `fm1-20261109-102035.json`: stock
FM-1, firmware 015, JEDEC `0x856014`, unit id `4150353835313708`. Nothing here writes flash. The labels
are the ones M-Flasher 0.9.4 uses; a later build may rename a button without changing the operation.

0. **What you need.** M-Flasher 0.9.4 (the version that produced both records — there is no reason to
   change tools in the middle of a backup). Get it from its releases page as the single Windows
   executable, not from a mirror. The unit, its USB-C data cable, a USB-A port directly on the machine,
   mains power, and about 2 GB free. Close every DAW, editor and browser tab that can open MIDI.
1. **Read the unit while it is still normal.** Open M-Flasher and connect. It shows `FM-1_015`,
   bootloader stock, app `OK`, chip key `0x980F`, flash `0x856014`. This is the state the record
   describes, and if any field differs, stop: the rest of this page is written for this state.
2. **`DOWNLOAD MODE`.** The tool sends SysEx `F0 22 24 35 7D F7` and the running firmware reboots into
   the chip's ROM loader. **The FM-1 stops being a MIDI device at this point** — that is the expected
   result, not a failure, and no file was written.
3. **Confirm the ROM mode before reading anything.** In Windows, Device Manager (or `Get-PnpDevice`)
   shows the unit as `WL82 UBOOT1.00`; M-Flasher reconnects to it by itself. Two checks: the MIDI port
   list is empty for this unit, and the storage/UBOOT device is present. If the device shows up with a
   warning sign, the ROM loader is there but the driver is not — install the tool's own driver
   instructions (WinUSB) before continuing. Do not read "the port disappeared" as a dead unit: in ROM
   mode there is no MIDI port by construction.
4. **`Back up the whole flash`.** It reads `0x000000..0x0FFFFF` — app, boot head, factory data, user
   regions — and writes two files: the `.bin` (exactly 1 048 576 bytes) and the `.json` next to it. The
   `.json` carries the part's identification and the SHA-256 of the image it just read. Neither file is
   a write: `apply()`, the only function that erases or programs, is unreachable from this action (§2).
5. **Leave ROM mode with `restart`.** The unit reboots into stock 015 and answers MIDI again. Nothing
   was written; if it does not come back, power-cycle it — the ROM loader is in the mask ROM and always
   answers.
6. **Second, independent reading.** Close M-Flasher, unplug the cable, replug it, and repeat steps 1–5
   in a new session, saving to a different filename. Two reads in one session share too much state to
   count as independent.
7. **Compare, on the machine, before trusting either file.**
   ```
   certutil -hashfile fm1-20261109-102008\fm1.bin SHA256
   certutil -hashfile fm1-20261109-102035\fm1.bin SHA256
   fc /b fm1-20261109-102008\fm1.bin fm1-20261109-102035\fm1.bin
   ```
   `fc /b` prints nothing when the files are identical. Two identical SHA-256 values *are* byte
   identity, so a CRC32 is only needed when you have a single read and no second one to compare —
   then read the chip's CRC32 with the tool's own verification and record it next to the dump.
8. **What makes the backup valid** (all of these, or it is not a backup yet): both files are exactly
   1 048 576 bytes; `fc /b` reports no differences; the two SHA-256 values are equal; the record says
   `0x980F` / `0x856014`; the record's own `sha256` equals the value both reads produced; the `.json`
   is kept beside each `.bin`.
9. **Keep it everywhere.** One copy outside the repository (an external disk, a cloud folder you
   control), one on different physical storage. Do not publish the dump or the unit id: it holds the
   unit's presets, its factory data and a stable identifier.
10. **Only then** is an install a decision rather than a gamble. On this unit that is M-Flasher's
    `Install firmware` with the release `.fwsc` — the same container this project signs — which writes
    `[0x4000, 0x93000)` and leaves `0x0000..0x3FFF` alone. The browser installer offers the honest
    alternative: it reports the unit, the package and the backup, and still writes nothing to a stock
    app (there is no `UPGRADE` path in stock 015 to write through).

If step 2 or 3 fails on this unit — no ROM mode, no `WL82 UBOOT1.00` — then M-Flasher cannot read it,
and `jl-uboot-tool` is the documented fallback: it loads the same kind of loader into RAM over the ROM
protocol, so a read stays a read. Do not mix the two tools in one session.

### What the two records already establish

`fm1-20261109-102008.json` and `fm1-20261109-102035.json` name the same unit, the same part, the same
tool and the **same `sha256`** (`38c66ae7…`), with two different `saved` stamps 27 seconds apart. That
is two independent reads of the same 1 MiB that agree, which is the corroboration step 8 asks for. It
is not a hash this project computed: the dumps are on the owner's disk. The installer reports the pair
as `corroborated` and says so, and a dump dropped next to a record upgrades it to `pass` because then
the page hashes the bytes itself.

## 11. What M-Flasher checks before its first erase, applied to this release

`Install firmware` is the first install on a stock unit (§6), so the question "will it take this
package" has a precise answer: the refusals in `Session::plan_firmware` (`src/ops.rs`) and the
container rules in `Package::parse` (`src/formats.rs`), in that order. `tools/fm1_mflasher_accept.py`
re-implements exactly those and applies them to a `.fwsc` here, printing each rule with its class:

- **`[SRC]`** — the rule as read in M-Flasher 0.9.4, tag `v0.9.4` (`src/formats.rs`, `src/ops.rs`,
  `src/boards.rs`). It is quoted, not executed: the tool that performs the install is M-Flasher's.
- **`[HOST]`** — the value computed in this repository from the package's bytes, now.

```
python3 tools/fm1_mflasher_accept.py release/felucca-v1.9.0.fwsc
  ok   the bytes this asks about          610806 B, sha256 80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2
```

| # | Rule M-Flasher applies | Where in it | This release | Class |
| --- | --- | --- | --- | --- |
| 1 | at least `0x1000` bytes | `Package::parse` | 610 806 B | `[HOST]` |
| 2 | the identity is woven into byte 47 of the first 48-byte blocks, one character per block | `Package::parse` | `FM-1_982`, 20 blocks | `[HOST]` |
| 3 | header CRC16 over bytes 2..63 | `Package::parse` | matches | `[HOST]` |
| 4 | file-list CRC16, stored in the header | `Package::parse` | matches (2 entries: `flash.bin`, `ota.bin`) | `[HOST]` |
| 5 | `flash.bin` is type 0 and its CRC16 matches the list | `Package::parse` | 602 112 B (`0x93000`), CRC16 `0xCC18`, matches | `[HOST]` |
| 6 | the image is longer than the boot area and a whole number of 4 KiB sectors | `Package::parse` | 147 sectors | `[HOST]` |
| 7 | the package's model is one the identified board accepts | `plan_firmware` | `FM-1` ∈ {`FM-1`, `MiniFM`} | `[SRC]`+`[HOST]` |
| 8 | the image is not larger than the flash | `plan_firmware` | 602 112 B ≤ 1 048 576 B | `[SRC]`+`[HOST]` |
| 9 | every `attr 0x82` app file decrypts with the unit's chip key and its CRC16 matches | `plan_firmware` → `app_files_status` | `app.bin` 581 564 B CRC16 `0xE643`; `cfg_tool.bin` 383 B CRC16 `0x2E75`, under `0x980F` | `[SRC]`+`[HOST]` |
| 10 | the write is `[0x4000, len)` and no changed sector may fall outside it | `plan_firmware` → `Plan::new` | `0x04000-0x92FFF`, 143 sectors, boot area kept | `[SRC]`+`[HOST]` |
| 11 | nothing reaches the unit's factory data | `boards.rs` `FM1.factory` | the image ends at `0x92FFF`; factory is `0xFF000+` | `[HOST]` |
| 12 | before the first erase of a session, a backup is saved; each written run is verified by the chip's CRC32, retried once | `ops.rs` `apply` | not reached by this trace — M-Flasher's own run | `[SRC]` |

Every one of those passes on `release/felucca-v1.9.0.fwsc`. What that establishes is narrow and worth
stating: **the package will not be refused**. The model and the chip key are answered again by the unit
when M-Flasher opens it (`Analysis::new` reads the image it took, `READ_KEY` returns the key), so a
package that passes here can still be refused on a unit that turns out to be another model or to
report another chip key. Nothing in this trace writes, and nothing in it is a substitute for the
backup of §10 or for the signature check the browser performs.

Negative controls, so the trace is not a rubber stamp — `tests/mflasher_accept_test.py` mutates the
release and requires each refusal, in M-Flasher's own wording:

| Mutation | Refusal it must produce |
| --- | --- |
| a byte of `app.bin`, every container CRC recomputed so the package is consistent with itself | `does not decrypt with this unit's chip key (0x980F)` |
| a byte of the package header | `damaged one (header CRC)` |
| a byte of the flash image | `damaged firmware package (flash.bin CRC)` |
| the first identity character (`F` → `S`) | `this package is for the SM-1, the connected unit is a FM-1` |
| the same package under chip key `0xFFFF` | `does not decrypt with this unit's chip key (0xFFFF)` |
| the image cut to a non-sector size | `damaged firmware package` / `unexpected flash image size` |

```
python3 tools/fm1_mflasher_accept.py release/felucca-v1.9.0.fwsc \
    --dump backup/stock-01/fm1.bin --unit-model "FM-1"           # exact sector plan
python3 tests/mflasher_accept_test.py                           # 14 checks, 0 FAILED
```

With `--dump` the plan is computed against the unit's own image (the sectors that actually differ, and
the `runs` M-Flasher will erase); without it the trace reports the bound, which is the whole app area.
`--unit-model` is the model the unit's installed firmware reports (`Analysis::new` decodes it out of
`app.bin`); it only drives the cross-model warning, which M-Flasher prints when the unit runs another
model's firmware. This unit's stock firmware reports `FM-1`, the same model as the package, so that
warning does not apply to it.
