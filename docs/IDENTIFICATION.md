# FM-1 identification: command `0x12`

Status: **IMPLEMENTED, HOST TESTED, TARGET COMPILED.** Not hardware tested: no FM-1 has
been used, and no unit here has answered `0x12`.

`0x12` is answered by the **application** (`firmware/src/ota.c`, `ota_qry_ident`), because the
application is what runs during step 1 of an install. The update loader is compiled with
`FELUCCA_LOADER=1`, which excludes the query code entirely: the loader stays byte for byte
what it was, so it cannot identify a chip and must not be relied on to.

## Frame

Request (SysEx, `F0 pack7(00 59 12 len24 body chk) F7`, body = 7 bytes):

| offset | size | meaning |
|---|---|---|
| 0 | 1 | protocol version, `1` |
| 1..4 | 4 | host nonce, little endian |
| 5..6 | 2 | CRC-16/CCITT (poly `0x1021`, init 0) over bytes 0..4 |

Reply (body = 24 bytes):

| offset | size | meaning |
|---|---|---|
| 0 | 1 | protocol version |
| 1 | 1 | status, see below |
| 2..4 | 3 | JEDEC id, as the SPI driver read it (`0x9F`) |
| 5..8 | 4 | capacity in bytes |
| 9..10 | 2 | page size |
| 11..12 | 2 | sector size |
| 13..16 | 4 | block size |
| 17..20 | 4 | session token, 0 when the status is not 0 |
| 21..22 | 2 | CRC-16 over bytes 0..20 |
| 23 | 1 | reserved, 0 |

## Status

| value | meaning | what the host must do |
|---|---|---|
| 0 | identified | may proceed, if two readings agree |
| 1 | the NOR did not answer | refuse, nothing may be written |
| 2 | JEDEC not in the table | refuse |
| 3 | capacity not corroborated | refuse |
| 4 | no or stale session (a `0x13`) | refuse, re-identify |
| 5 | address outside the part | refuse |
| 6 | length 0, more than one chunk, or a read error | refuse |
| 7 | version or request CRC wrong | refuse |

## What is measured and what comes from the datasheet

- **Read from the chip on each request:** the JEDEC id (`0x9F`).
- **Corroborated read-only:** the end of the readable window. The device reads 16 bytes at
  `capacity - 16` twice and requires them to be equal, then requires a read **at** `capacity`
  to fail. A geometry that did not match the part would not survive this.
- **From the datasheet, keyed by that JEDEC id:** page 256 B, sector 4 KiB, block 64 KiB. The
  table in `firmware/src/ota.c` (`OTA_PARTS`) has exactly two entries, `0x856014` and
  `0xB36014` (both writable: docs/JEDEC_COMPATIBILITY.md); anything else is status 2. These numbers
  come from the parts' datasheets, not from a measurement on the unit in front of you, and the
  manifest says so.

## Session and replay

Each answered `0x12` issues a **fresh** token derived from the host nonce, the device clock
and a counter. Two readings with the same nonce must agree on every field and must bring
**different** tokens; a reused token means the reply may be old and is refused
(`tools/fm1_probe.py`, `probeChip` in `web/fm1ota.js`). `0x13` requires the token of the
latest identification, and any failed re-identification clears it.

## Compatibility with older firmware, and the first install

| situation | what the host sees | what happens |
|---|---|---|
| app with `0x12` | status 0, twice | installation may proceed |
| app without `0x12` (older firmware) | silence, then timeout | refused: **absence is not compatibility** |
| app that does not start | handshake silence or a timeout | refused |
| loader only (interrupted install) | identity `ota-FM-1_900` | refused: the loader cannot identify, and its code is unchanged |
| unknown answer, bad length, bad CRC | `frame` / `crc` | refused |
| device unplugged | `lost` | refused |
| another model | identity check | refused before any query is sent |

**The first install of a package that carries `0x12` cannot use `0x12`,** because the firmware
answering is the old one. And that is the end of it now: a typed JEDEC is not evidence, so `--jedec`,
`--allow-unidentified-chip` and `FM1_LAB` were removed and the installer has no way to accept a chip it
did not read. So **a stock V15 unit cannot be installed by this installer at all**, and that is the
honest answer rather than a missing flag: on stock there is no `0x12` to read. The first install is a
different operation with a different tool: a verified backup, then M-Flasher installing the package as a
firmware image (it checks the model, the chip key and the app CRCs itself, and writes the app area only),
and from then on this installer is what updates QG-1 to QG-1. See docs/M-FLASHER.md §6.

## Cost

One `0x12` is one `0x9F` (3 bytes over SPI) plus 32 bytes read twice and one read refused,
all inside `irq_save()` in `firmware/src/ota_hw.c`; it touches no persistent data and does not
stop audio.

Measured on the build: the application image went from 556 152 to 558 200 B and RAM
`.data+.bss` from 95 732 to 96 452 of 98 304 B (spare 1 852). The per-read cost — interrupts
off for one 256-byte SPI read, **estimated** at ~40 µs — is **not measured**: it needs a
logic analyser on a real unit, so audio is "not expected to be affected", not "verified
unaffected".
