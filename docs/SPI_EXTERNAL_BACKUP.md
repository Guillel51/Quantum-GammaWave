# Backup over an external SPI reader, and what restoring from it costs

## Why this route exists

`tools/fm1_backup.py` reads the part over USB-MIDI with `0x13`, a command that lives in
[firmware/src/ota.c](../firmware/src/ota.c). A stock unit answers `0x12` (identity) — the Verify Only
page proves that — but it has no `0x13`, so **there is no way to read the flash over USB-MIDI before
the first install**. The only read path that exists before the firmware does is a physical one.

That is not a workaround anyone enjoys. It means opening the unit and clipping an SOIC-8 reader onto
the P25Q80H, or desoldering it. What it buys is a backup that exists *before* the first write, and a
restore path that does not depend on the firmware being intact — which is the only restore that helps
when the firmware is the thing that broke.

## One-time acquisition

```sh
flashrom -p ch341a_spi --flash-name            # the part id, for --jedec below
flashrom -p ch341a_spi -r pass-a.bin           # first read
flashrom -p ch341a_spi -r pass-b.bin           # independent second read
python3 tools/spi_dump.py --a pass-a.bin --b pass-b.bin --out backup/unit-01 \
        --jedec 0x………… --app-end 0x…………
```

`--app-end` is optional and deliberately not defaulted: the end of the application image differs per
layout, and a guessed value turns a good dump into a rejected one. Leave it out until the map is
known; the erased-page check only runs when you supply it.

What the verifier rejects, each with the offset that caused it:

- the two passes disagree (any byte);
- a pass is not the whole part;
- a run of eight or more byte-identical pages that is not a constant fill — the signature of a reader
  re-sending the same buffer. Two equal pages are *not* a fault: flash holds repeated tables and
  zeroed settings for real, and a heuristic that rejects those would reject genuine dumps;
- erased (`0xFF`) pages below `--app-end`;
- a JEDEC outside the allowlist (exit 2), because an unread part is not a part whose geometry is known.

## The dump is an archive, not a tested restore

`complete: true` in the manifest means: two independent reads agreed, the size is the announced size,
no reader fault pattern was found. It does **not** mean a unit was restored from it. No restore has
been performed on hardware, on this project or anywhere else.

## Restoring

```sh
python3 tools/spi_restore.py --dump backup/unit-01
```

That prints the plan and refuses if the manifest is incomplete, the bin changed after the manifest was
written, the part is unknown, or the length does not match. It never executes `flashrom`: there is no
subprocess call, no `--yes`, no write path in the file. The operator runs the commands by hand, on the
unit the dump came from.

The write replaces **every byte, bootloader included**. If the clip shifts or power drops mid-write
the unit will not boot from flash, and the recovery mode that would help has not been demonstrated on
this hardware. In that state the answer is to keep the clip on and re-run the write — not to
power-cycle and hope.

## Status

| step | state |
|---|---|
| verifier and restore plan | implemented, tested on a simulated chip (23 checks) |
| real dump | not taken: needs physical access to a unit |
| restore from a real dump | never performed |
| recovery by USB-MIDI after a failed write | documented only |
