# Reading the JEDEC and the flash geometry of an FM-1 without writing anything

`tools/fm1_backup.py` is already the read-only path: it sends **0x11** (handshake), **0x12**
(identification, which returns the JEDEC) and **0x13** (read). It never sends 0x30, never sends
UPGRADE, never erases, never programs and never touches a status register —
`tests/backup_tool_test.py` asserts exactly that against a simulated FM-1. Nothing below changes
that; this page is the procedure and what each result means.

## 0. Before touching anything

- The unit must be on the stock firmware and answering `Verify Only`. If it is not identified there,
  stop: the read is not the problem, the link is.
- One USB cable, no hub, no other MIDI app open (a second client holding `FM-1 Midi` is the most
  common cause of a failed pass).
- Nothing in this procedure writes. If any command below offers `--write`, `--restore`, `--erase`
  or `--force`, it is not this procedure.

## 1. Identify, and read the JEDEC

```sh
python3 tools/fm1_backup.py --out backup/read-01
```

- The first frame is the handshake; the second is `0x12`, and its answer carries the identity
  (`FM-1_015` on a stock V15) and the three JEDEC bytes.
- Expected on this project's allowlist: `0x856014` (OEM P25Q80H) or `0xB36014` (the newer variant).
  **Any other value is an unknown part** and stops here: unknown IDs are rejected on purpose, and a
  guessed geometry is how a write lands on the wrong sector later.
- The manifest that appears next to the image records `"jedec": "0x…"`, the identity and the size —
  that file, not your memory of the terminal, is the evidence.

## 2. What the geometry comes from

There is no separate "geometry" command on this protocol: the capacity is what the read returns.
`fm1_backup.py` reads the part from `0` to the size the identification announced, so

- a **1 MiB** read that completes twice bytes for byte is the confirmation that the part is 1 MiB;
- a read that ends short, repeats a block, or returns `0xFF` where the boot area must be, is a
  **failed** read — not a smaller chip.

The manifest marks a backup `complete` only when every byte was read twice and the two passes are
byte-identical.

## 3. Keep the evidence

```sh
sha256sum backup/read-01/*.bin          # the hash to compare against a second read
python3 tools/compare_build.py --help   # unrelated to flash: the firmware comparison, for contrast
cp backup/read-01/*.bin /media/<external>/   # outside the repository, then a second copy elsewhere
ls -l backup/read-01/                   # size must be exactly the announced size
```

Do not edit, compress in place or rename the original image. A second copy is a second *copy*, not a
second read: for a comparative read, repeat step 1 into `backup/read-02` and compare the hashes.

## 4. What this does *not* prove

- It does not prove the image can be restored: that needs a write, and a write needs a proven
  recovery route on a unit that is not your only one.
- It does not prove the flash map of QG matches the stock layout: that comparison needs the read
  above **and** `docs/`'s map, done by hand, sector by sector.
- It does not identify the SoC or the bootloader configuration; the protocol does not expose them.

## 5. Why it has not been run here

The cloud session has no USB access to the unit: the run above has to happen on the machine the FM-1
is plugged into. Everything up to the first frame is prepared; nothing has been sent to a physical
device from this repository's CI or from this session.
