# The first backup: reading the FM-1's flash with an external programmer

This is the only read path that does not need the unit to boot anything. The MIDI path
(`tools/fm1_backup.py`, docs/JEDEC_READ.md) needs the QG application because `0x12` and `0x13` are its
frames; a stock V15 unit has never answered them here. A programmer reads the part directly, so the
first backup of a stock unit is a hardware read, and it is the input that a proven recovery route needs.

`tools/fm1_flash_read.py` does the software half: two reads with the same programmer operation, a
byte-for-byte comparison, and a `qg1-backup-1` manifest that `tools/fm1_restore.py` already validates.

## What is established, and what is not

| fact | status |
|---|---|
| The FM-1 stores firmware in a 1 MiB SPI NOR part | documented in this repository, **not** measured on a physical unit |
| The part is P25Q80H (or the newer `0xB36014` variant) | documented allowlist; **no unit here has read a JEDEC** |
| P25Q80H supply range 2.3–3.6 V, standard SPI, SOIC-8 pinout | from the Puya datasheet ([P25Q80H_Datasheet_V1.7](https://www.johnny-li.com/download_path/%E6%95%B0%E6%8D%AE%E6%89%8B%E5%86%8C/Flash/P25Q80H_Datasheet_V1.7.pdf)) |
| The package on **your** unit, its marking, its voltage, and whether it can be read in circuit | **unknown**; step 1 below is how it stops being unknown |
| The stock layout matches the QG flash map | **unknown**; needs the image from this procedure and a manual comparison |

Do not treat the allowlist as a measurement. The first thing this procedure produces is evidence, not a
confirmation of what the repository assumed.

## 1. Identify the part without touching it

1. Photograph the board and the flash package: marking, package (SOIC-8 / WSON-8 / USON-8), pin-1 dot.
2. Note the supply: this family is 2.3–3.6 V, but a board can regulate it lower; a 1.8 V part would
   forbid a 3.3 V programmer.
3. Compare the marking with the documented ids: `P25Q80H` (JEDEC `0x856014`) or the newer variant
   (`0xB36014`). A different marking is not a different name for the same part: stop and identify it.

A WSON/USON package has no exposed leads. A SOIC-8 clip does not work on it, and the options become a
WSON probe (unreliable), a socket adapter after desoldering, or a different approach entirely.

## 2. Materials

- A **3.3 V** SPI programmer whose I/O is 3.3 V, not a 5 V CH341A as shipped. Many cheap CH341A boards
  put 5 V on the SPI lines; that can damage a 3.3 V part and, through the clip, the SoC. Verify the
  output with a meter before clipping anything, or use a programmer documented as 3.3 V.
- A SOIC-8 test clip with pin 1 marked (Pomona-style), or a socket if the part is removed.
- `flashrom` on Linux (`apt install flashrom`), or AsProgrammer/NeoProgrammer on Windows.
- A grounded wrist strap, and a second machine or disk to hold the two copies.

## 3. Read, twice, with the part isolated

The SoC drives the same SPI bus. While the board is powered, its CS/CLK/MOSI can fight the programmer and
produce an image that looks plausible and is wrong. Read with the board **unpowered**; if the board
back-feeds the rest of the circuit through the clip and the read is unstable, the part has to come off,
or the SoC has to be held in reset.

```sh
# once, to see what the programmer detects; it does not write
flashrom -p ch341a_spi --flash-name

# the two reads, as two separate operations
flashrom -p ch341a_spi -r stock-pass-a.bin
flashrom -p ch341a_spi -r stock-pass-b.bin
cmp stock-pass-a.bin stock-pass-b.bin && sha256sum stock-pass-*.bin
```

Two identical reads are the minimum evidence that the clip contact and the bus were clean. Two reads that
differ are a failed read, not a chip with two states: reseat, lower the clock, isolate the SoC, repeat.

Then make the archive the project's tools understand, from the *same* programmer, so no step depends on a
file being copied by hand:

```sh
python3 tools/fm1_flash_read.py --out backup/stock-01 \
    --programmer flashrom:ch341a_spi --jedec 0x856014 --identity FM-1_015

python3 tools/fm1_flash_evidence.py check backup/stock-01 --json backup/evidence.json
```

- `--jedec` is the id **on the marking**, and the manifest records it as declared (`jedec_source`), because
  a generic programmer reads the silicon without running the vendor's `0x9F` identification.
- `--identity` is the identity on the unit's label; the tool refuses the read if `FM-1_0NN` does not appear
  in the image. Without `--identity` it uses the single identity found, if there is exactly one, and writes a
  manifest with none otherwise — which `tools/fm1_restore.py` refuses.
- `--programmer file:part.bin` exists for tests and for a part already dumped to a file; it is not a way to
  read hardware.

## 4. What to keep

- `backup/stock-01/*.bin` — the image; **this is your presets and objects**. Never commit it (`backup/` is
  ignored), never attach it to an issue, never paste it.
- The manifest, the `.sha256` sidecar and `evidence.json`.
- Two copies outside the repository, on two different disks, plus a printed or photographed record of the
  hashes and of the marking you declared.

## 5. Restoring from it

`tools/fm1_restore.py` is the counterpart, and the only writer in this repository that does not go through
MIDI:

```sh
python3 tools/fm1_restore.py --backup backup/stock-01 --write \
    --programmer flashrom:ch341a_spi --include-head
```

- It refuses a manifest that is not `complete`, whose two passes differ, whose hashes do not match the file,
  whose JEDEC is not documented, whose identity is missing, or whose programmer receipt is anything but two
  reads.
- It erases sector by sector, programs page by page, reads every sector back, then the whole part, and
  returns non-zero if anything is not identical.
- `--include-head` is required for `flashrom`: the boot header and loader are unit-specific, and a partial
  write is not possible through this backend. Restoring the *same* unit's own image is the only safe
  operation; writing one unit's head onto another is how a second unit becomes brick-like.
- The `flashrom:` path has **never been executed against hardware here**. Its command construction and its
  refusal paths are tested; the erase/program/verify loop is tested against a simulated part only.

## 6. Risks this procedure carries

- **Clip slip.** A misplaced SOIC-8 clip can put VCC on a data pin. Pin 1 first, then clamp, then check the
  seating before powering the programmer.
- **Back-feed.** Powering the part through the clip powers part of the board. That is usually harmless for a
  read and occasionally not; an unpowered board and a current-limited programmer is the safer combination.
- **A plausible wrong image.** The dangerous failure is not a read that errors, it is a read that returns
  plausible bytes from a contended bus. That is what the two-read comparison and the identity scan exist for.
- **No recovery is proven yet.** This procedure gives you the artefact a recovery needs. It does not by
  itself prove that a write will bring the unit back; that needs a write on a unit that is not your only one,
  which is still outside what this repository has done.

## 7. Acceptance criteria for closing the hardware gap

The repository moves from "documented" to "measured" for the FM-1 only when all of these exist, recorded:

1. A photograph of the part's marking and package on the unit.
2. Two byte-identical 1 MiB reads, with hashes, from an isolated part.
3. A manifest whose `identity` was found in the image (so restore accepts it).
4. `tools/fm1_flash_evidence.py check` exits 0 on the bundle, and the record says `restorable: true`.
5. A second, independent copy of the image off-machine.
6. For recovery: one successful write of that image back to the same unit with a verified read-back, on a
   unit whose owner accepts the risk — or a second unit to practise on first.

Until then the install gate stays closed: no JEDEC read from silicon, no verified backup, no demonstrated
recovery.
