# Backup: command `0x13` and `tools/fm1_backup.py`

Status: **IMPLEMENTED, HOST TESTED, TARGET COMPILED. Not hardware tested, not restore tested.**
A backup of a unit nobody has read is not a verified backup, and **no restore exists**.

## `0x13`: read only

Request (body 13 bytes): version(1) address(4, LE) length(2, LE) session(4, LE) CRC-16(2).
Reply (body `14 + n`): version(1) status(1) length(2) address(4) session(4) CRC-16(2) then the
bytes. Status values are the same as `0x12`.

- **Read only, by construction:** the handler calls `fl_read_ram` (`0x0B`); there is no path to
  `WREN`, erase, program or a status-register write. `tests/chip_test.py` records every frame
  the backup tool sends and fails if it is anything but `0x13` (after `0x11`/`0x12`).
- **Maximum 256 bytes per request.** 512 would need a 640-byte wire buffer; 256 keeps the reply
  at 319 wire bytes and the `.bss` cost at ~600 B. The host chunks; the device never returns a
  short payload, and refuses `len = 0`, `len > 256` and `address + len > capacity` with a status
  instead of silently clipping.
- **Range:** `0x000000..0x0FFFFF`, the whole part. The application can read it because the raw
  read goes through the SPI driver with XIP off, not through the XIP window. It reads what is
  physically on the chip (encrypted regions read as ciphertext), which is what a backup must
  contain.
- **Stale session, disconnect, read error:** the token must be the latest one; a failed read is
  status 6 with no payload; a dead link surfaces as `lost` on the host.

## Two passes

`tools/fm1_backup.py --out DIR` identifies the unit with `0x12`, then reads the whole part
**twice, independently**, comparing byte for byte and hashing each pass with SHA-256.

| status | meaning |
|---|---|
| `complete` | every byte read twice, passes identical, size == capacity, no unread range |
| `partial` | one or more ranges could not be read (listed), the rest is kept |
| `inconsistent` | the two passes differ; both are written with `-passB.bin`, no final hash |
| `unverifiable` | nothing usable was read |

Only `complete` writes a `.sha256` and prints a hash. `partial` and `inconsistent` exit 1 and
say so: **a backup with holes is not a backup.**

## Manifest (`qg1-backup-1`)

`device` (identity, product, firmware, JEDEC, part, capacity, page, sector, block, the fact that
the identification was two `0x12` readings with the session id, nonce), `passes`, `chunk`,
`size`, `expected_size`, `sha256_pass_a`, `sha256_pass_b`, `sha256`, `identical`, `status`,
`unread_ranges`, `errors`, `commands_sent`, `files`, `seconds`, and
`restore: "not implemented and not verified"`.

## What this does not do

- **No restore.** Restoring writes outside the application area, which means the loader and the
  SPL; both are out of scope and untouched. Until a restore is performed on a bench and the
  unit boots, "recovery" is **BLOCKED** for every level that involves writing.
- **No verification against a known-good image.** The manifest proves the two passes agree with
  each other, not that either is the factory content.
- **No protection bypass.** Protected sectors, if any were ever set, would read as they read; the
  tool never clears protection.
