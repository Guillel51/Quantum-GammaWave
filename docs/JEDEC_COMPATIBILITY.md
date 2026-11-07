# Flash compatibility: the two documented 1 MiB parts

The FM-1 has shipped with two SPI NOR parts. Both are 1 MiB, and this project writes both of them; a
third identifier is refused. This page says exactly what each row rests on, so a future change can be
argued from evidence instead of from memory.

`python3 tools/fm1_gate.py` prints the same table, `web/fm1ota.js` refuses the same identifiers, and
`tests/gate_test.py`, `tests/flash_evidence_test.py` and `web/test_web.mjs` fail if the three drift
apart.

| JEDEC | Part | Read | Backup | Write app area | Boot area / bootloader | QG-1 install |
| --- | --- | --- | --- | --- | --- | --- |
| `0x856014` | Puya P25Q80H | yes | yes | yes | never written by this project | allowed by the gate (still needs a signed package and `0x12` on that unit) |
| `0xB36014` | UCUN UC25HQ80 | yes | yes | yes | never written by this project | allowed by the gate (same conditions) |
| anything else | — | refused | refused | refused | refused | refused (`chip_unknown`) |

"Never written by this project" is not a chip limitation: `fl_erase4k_ram`/`fl_prog_ram` are only
reached for the app and data ranges, and no installer in this repository has a path that writes
`0x00000..0x03FFF`.

## Why `0xB36014` is writable here

Three independent pieces of evidence, in increasing strength:

1. **Geometry and commands (UCUN datasheet, v1.2).** 1 MiB array, uniform 256 B page, 4 KiB sector,
   32/64 KiB block; `9F` JEDEC, `05`/`35` status, `06` write enable, `02` page program, `20` sector
   erase, `D8` block erase, `03`/`0B` read; `WIP` bit 0 and `WEL` bit 1 in SR1; `BP0..BP4` blocking
   program and erase; 2.3–3.6 V; 100k cycles, 20 years. Maximum times: page program 3 ms, sector
   erase 10 ms, block erase 10 ms.
2. **The driver in this repository is timing-agnostic.** `firmware/hal/fm1_flash.h` polls `WIP`
   (`fl_wait_ready`) with 500 ms for a 4 KiB erase and 10 ms for a page program instead of assuming a
   fixed delay, and `fm1_flash_id.h` requires two agreeing `0x9F` reads, a 1 MiB capacity byte and a
   status register that is idle and unprotected before any write. The two parts' timing differences
   are therefore not a correctness input: both fit inside the same polled budget.
3. **A physical install on this part, on an FM-1.** In
   [Felucca #320](https://github.com/hugelton/Felucca/issues/320) a user with a newer unit reports
   M-Flasher identifying `flash 0xB36014` on `FM-1_015` (stock), then a full `fm1_install.py` install
   of a beta image that writes 100 %, reboots and runs, and then — after a complete power cycle —
   settings and a saved project surviving, with both the restore-last and explicit-save paths. That
   is a whole-image erase/program followed by persistent user data on the same part, not a read.

It is Felucca's image, not this project's: the third-party record proves the **part** takes a full
write and keeps data, which is the question this page answers. It does not prove QG-1 runs on that
unit; that is requirement 8 and 9 of the install matrix and it is still open (see
[FM1_INSTALL_SAFETY.md](FM1_INSTALL_SAFETY.md)).

## Why the boot area stays out

The one field report that made this project cautious was a UCUN unit that did not boot after an
**m-bootloader install** — a write to the boot area, by another project. Nothing here writes
`0x00000..0x03FFF` on any part: the update loader writes the app area, settings and objects, and the
installer's own map refuses anything below `0x4000`. So that report does not gate the app-area row,
and the `bootloader` column stays `False` for `0xB36014` and `True` for the OEM part only in the sense
that the stock bootloader is the one that stays.

## What is still unmeasured

- No unit with `0xB36014` has been read, backed up or written **by this project**. The gate therefore
  requires the same evidence on the owner's unit (`0x12` in-session, two reads, signed package) as on
  a Puya unit; it does not carry the third-party result over.
- The datasheet's `BP3`/`BP4` bits live in SR2, outside the `BP0..BP2` mask this firmware checks. A
  part protected there would silently ignore erase/program; the update path's per-sector readback is
  what catches it, and it aborts before the next sector.
- Write endurance under the update pattern (erase a 4 KiB sector, program 256 B pages, verify) is
  specified by the vendor, not measured here.

## If a third part appears

Add it to `tools/fm1_gate.py` (`PRODUCTION_JEDEC` and `CAPABILITIES`), to `web/fm1ota.js`
(`KNOWN_JEDEC`, `QRY_PARTS`), to `tools/fm1_flash_evidence.py` (`WRITABLE`) and to `firmware/hal/`
(`fm1_flash_id.h`, `src/ota.c`). `tests/gate_test.py` compares the first three; `tests/flash_id_test.c`
and `tests/ota_test.c` cover the firmware. A new identifier with no record stays refused by every one
of them.
