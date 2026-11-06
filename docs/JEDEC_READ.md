# Reading the JEDEC and the flash geometry of an FM-1 without writing anything

Two tools and nothing else: `tools/fm1_backup.py` reads, `tools/fm1_flash_evidence.py` checks what came
back. The only frames the read may send are **0x11** (handshake), **0x12** (identification, which carries
the JEDEC) and **0x13** (read). No 0x30, no UPGRADE, no erase, no program, no status-register write —
`tools/fm1_backup.py` refuses to finish if its own frame log holds anything else, and the checker
re-checks that log against the file on disk. `tests/chip_test.py` asserts the frame set against a
simulated unit (no 0x30, no UPGRADE, two `0x12` first), and `tests/flash_evidence_test.py` asserts that
the checker accepts a real bundle and rejects a tampered one.

## 0. Before touching anything

- The unit must be on the **stock firmware** and answering `Verify Only`. If it is not identified there,
  the read is not the problem: the link is. Fix that first.
- One USB cable, no hub, no other MIDI app open. A second client holding `FM-1 Midi` is the most common
  cause of a failed pass.
- Nothing here writes. If a command offers `--write`, `--restore`, `--erase` or `--force`, it is not this
  procedure.

## 1. First read: identification, JEDEC, 1 MiB

```sh
python3 tools/fm1_backup.py --out backup/read-01
```

- It asks once and prints what it found. In a non-interactive shell add `--yes`; the confirmation only
  exists so nobody starts a read on the wrong port.
- The first two frames are `0x12`, twice; their answer carries the identity (`FM-1_015` on a stock V15)
  and the three JEDEC bytes. The rest are `0x13` reads, two full passes over the announced capacity.
- Expected end of the output: `complete: backup/read-01/FM-1_015-<UTC stamp>.manifest.json`, then the
  SHA-256 and the image path.
- Expected JEDEC: `0x856014` (OEM P25Q80H) or `0xB36014` (the newer variant). **Any other value stops
  here.** Unknown parts are rejected on purpose: a guessed geometry is how a write lands on the wrong
  sector later.

Three files appear, and they are the evidence, not the terminal scrollback:

| file | what it is |
|---|---|
| `FM-1_015-<stamp>.bin` | the image: exactly the announced capacity, 1048576 bytes |
| `FM-1_015-<stamp>.manifest.json` | identity, firmware, JEDEC, part name, page/sector/block, both pass hashes, the ordered frame log, errors, status |
| `FM-1_015-<stamp>.sha256` | one line: the image hash and the image name (written only when `complete`) |

## 2. Second read, so the geometry is corroborated

A capacity is only what the read returns, so the confirmation is a second, independent read that lands
byte for byte:

```sh
python3 tools/fm1_backup.py --out backup/read-02
```

Unplug and replug the unit between the two, so the second read is a new session and not a repeat inside
the same one. A second read that ends short, repeats a block, or returns `0xFF` where the boot area must
be is a **failed** read, not a smaller chip.

## 3. Check the bundle before you believe it

```sh
python3 tools/fm1_flash_evidence.py check backup/read-01 --second backup/read-02 --json backup/evidence.json
```

This talks to no device. It holds the manifest to the bytes: the image's size and SHA-256 against the
manifest, a second-pass file when one exists, the frame log against `{0x11, 0x12, 0x13}`, the JEDEC
against the parts this project documents, the geometry against that part's table, and the identity
against the stock-donor rule (`FM-1_0NN`). With `--second`, the two independent reads must agree.

- Exit `0` and `verified read`: every check passed.
- Exit `1`: it prints the checks that failed, by name. Fix the read, do not fix the record.

`backup/evidence.json` is the record: the device fields, the image hash and size, the read-only verdict,
every check with its result, and its own `record_sha256` over the canonical JSON. Read it before sharing
it; the `limits` list is the honest half.

If a check fails, it means one of these, and each has one answer:

| failure | meaning | answer |
|---|---|---|
| `the result is complete` | the read was `partial`, `inconsistent` or `unverifiable` | read again, better cable, nothing else on the port |
| `the image's SHA-256 is the manifest's` | the image changed after it was written | restore your copy, never the edited file |
| `every logged frame is 0x11, 0x12 or 0x13` | something other than a read was sent | stop; that run is not evidence of anything |
| `the JEDEC id is a part this project documents` | unknown part | stop; the geometry cannot be trusted |
| the two reads disagree | the part or the link is not stable | stop; the unit is not a candidate for a write |
| `the unit identified as a stock FM-1` | not a donor the installer may start from | stop |

## 4. Keep it

- The image is 1 MiB of **your** unit: it contains your presets and objects. `backup/` is in
  `.gitignore` for that reason. Do not commit it, do not attach it to an issue, do not paste it.
- Keep `read-01`, `read-02`, both manifests, both `.sha256` files and `evidence.json`. Copy the images to
  two places outside the repository (two different disks), and never edit, compress in place or rename
  the originals.
- To hand the result to someone else, share the manifests and `evidence.json` — hashes, sizes, JEDEC,
  identity — not the images.
- A second *copy* is not a second *read*: for corroboration, the second directory has to come from step 2.

## 5. What this proves, and what it does not

It proves: this unit identified itself as `FM-1_0NN` on stock firmware, the part answered `0x12` with a
documented JEDEC id and a 1 MiB geometry, two passes read every byte identically, and the run sent read
frames only.

It does **not** prove:

- that the image can be **restored**: that needs a write, and a write needs a recovery route proven on a
  unit that is not your only one;
- that the QG flash map matches the stock layout: that is a manual, sector-by-sector comparison between
  this image and the map in `docs/`;
- that the SoC, the clock or the bootloader are what the documentation says: this protocol does not
  expose them;
- that an install may run. The record sets `authorises_install: false` on purpose. The installer reads
  `0x12` on the live link at install time and refuses on anything it does not recognise; there is no path
  that feeds this file into the gate, and adding one would weaken the check rather than strengthen it.

`0xB36014` is a special case: the read verifies, the record marks `write_capable_part: false`, and the
write path refuses that id (see `web/fm1ota.js`). A verified read is about a unit and a part; it is not a
green light for that part.

## 6. Why it has not been run here

This session has no USB access to the unit, so the read has to happen on the machine the FM-1 is plugged
into. Everything up to the first frame is prepared and tested against a simulated unit; nothing has been
sent to a physical device from this repository's CI or from this session.
