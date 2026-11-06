# Reading the JEDEC and the flash geometry of an FM-1 without writing anything

**This is not a way to make the first backup of a stock unit.** `0x12` and `0x13` are answered by the QG
application (`firmware/src/ota.c`, `ota_qry_ident` / `ota_qry_read`), and `docs/IDENTIFICATION.md` says
what that means in one sentence: *"Not hardware tested: no FM-1 has been used, and no unit here has
answered `0x12`."* The stock V15 firmware is a different program with the vendor's own frames, and
nothing in this repository shows it answering `0x13`. Read §1 before touching anything.

Two tools and nothing else: `tools/fm1_backup.py` reads, `tools/fm1_flash_evidence.py` checks what came
back. What the read may send, in order:

| frame | when | count |
|---|---|---|
| vendor handshake `F0 00 32 45 00 00 00 40 7F F7` | discovery (`install.Updater.find`), before anything else | 1–3, the same bytes each time |
| `0x12` identification | first, twice, with one fresh nonce | exactly 2 |
| `0x13` read | two full passes over the announced capacity | `2 × capacity / chunk` |

No `0x30`, no UPGRADE, no erase, no program, no status-register write. The manifest's frame log starts
after discovery — it records the read phase, which is the part that touches flash contents — and the tool
refuses to finish if that log holds anything but `0x12` and `0x13`. `tests/chip_test.py` asserts the
whole set against a simulated unit, including that a unit which answers the vendor handshake and not
`0x12` is refused with `no-query` after sending `0x12` only and zero UPGRADE frames.

## 0. Before touching anything

- The unit must be **running QG**, or `0x12` will not be answered. There is no configuration of a stock
  V15 unit that makes this procedure work: see §1.
- One USB cable, no hub, no other MIDI app open. A second client holding `FM-1 Midi` is the most common
  cause of a failed pass.
- Nothing here writes. If a command offers `--write`, `--restore`, `--erase` or `--force`, it is not this
  procedure.

## 1. Who answers `0x13`, and what that costs

`0x13` is `ota_qry_read()` in the QG application: it calls `ota_rawread()`, which runs `fl_read_ram` from
`.ram_text` with interrupts off for one 256-byte read. That code is compiled into the application
(`#if !FELUCCA_LOADER`); the update loader is built with `FELUCCA_LOADER=1` and excludes the query code
on purpose. So the answer to "what answers `0x13` on stock V15" is: **nothing this project has verified.**
The frames belong to a program that the unit is not running yet, and the vendor firmware's own command
set is not documented here.

Two consequences, and they matter more than any of the tooling below:

1. **The procedure cannot produce the first backup of a stock unit.** Against a stock V15 the tool now
   says exactly that — `refused: FM-1_015 answered the vendor handshake but not 0x12 … only an external
   SPI reader can` — instead of leaving a timeout that reads like a bad cable.
2. **The only first backup is a hardware one:** an external SPI NOR reader on the part, with the image
   taken off the chip directly. That is outside this repository and outside this session; nothing here
   can substitute for it, and no amount of host testing turns a simulated answer into a real one.

After QG is installed, this read is exactly what it claims to be: a two-pass, read-only dump used to
record the unit's state and to corroborate the geometry on later sessions.

## 2. Identification, JEDEC, 1 MiB

```sh
python3 tools/fm1_backup.py --out backup/read-01
```

- It asks once and prints what it found; in a non-interactive shell add `--yes`.
- The first two frames are `0x12`; their answer carries the identity and the three JEDEC bytes. The rest
  are `0x13`, two full passes over the announced capacity.
- Expected end of the output: `complete: backup/read-01/FM-1_982-<UTC stamp>.manifest.json`, then the
  SHA-256 and the image path.
- Expected JEDEC: `0x856014` (Puya P25Q80H) or `0xB36014` (the newer variant). **Any other value stops
  here.** Unknown parts are rejected on purpose: a guessed geometry is how a write lands on the wrong
  sector later.

Three files appear, and they are the evidence, not the terminal scrollback:

| file | what it is |
|---|---|
| `FM-1_982-<stamp>.bin` | the image: exactly the announced capacity, 1048576 bytes |
| `FM-1_982-<stamp>.manifest.json` | identity, firmware, JEDEC, part, page/sector/block, both pass hashes, the ordered frame log, errors, status |
| `FM-1_982-<stamp>.sha256` | one line: the image hash and the image name (written only when `complete`) |

## 3. Second read, so the geometry is corroborated

A capacity is only what the read returns, so the confirmation is a second, independent read that lands
byte for byte:

```sh
python3 tools/fm1_backup.py --out backup/read-02
```

Unplug and replug the unit between the two, so the second read is a new session and not a repeat inside
the same one. A read that ends short, repeats a block, or returns `0xFF` where the boot area must be is a
**failed** read, not a smaller chip.

## 4. Check the bundle before you believe it

```sh
python3 tools/fm1_flash_evidence.py check backup/read-01 --second backup/read-02 --json backup/evidence.json
```

This talks to no device. It holds the manifest to the bytes: the image's size and SHA-256 against the
manifest, a second-pass file when one exists, the frame log against `{0x11, 0x12, 0x13}`, the JEDEC
against the parts this project documents, the geometry against that part's table, and the identity
against the stock-donor rule. With `--second`, the two independent reads must agree.

- Exit `0` and `verified read`: every check passed.
- Exit `1`: it prints the failed checks by name. Fix the read, do not fix the record.

`backup/evidence.json` is the record: device fields, image hash and size, the read-only verdict, every
check with its result, and its own `record_sha256`. Read it before sharing it; the `limits` list is the
honest half.

| failure | meaning | answer |
|---|---|---|
| `the result is complete` | the read was `partial`, `inconsistent` or `unverifiable` | read again, better cable, nothing else on the port |
| `the image's SHA-256 is the manifest's` | the image changed after it was written | use your copy, never the edited file |
| `every logged frame is 0x11, 0x12 or 0x13` | something other than a read was sent | stop; that run is not evidence of anything |
| `the JEDEC id is a part this project documents` | unknown part | stop; the geometry cannot be trusted |
| the two reads disagree | the part or the link is not stable | stop; the unit is not a candidate for a write |
| `the unit identified as a stock FM-1` | not the identity the installer may write over | stop |

## 5. Keep it

- The image is 1 MiB of **your** unit: it contains your presets and objects. `backup/` is in
  `.gitignore` for that reason. Do not commit it, do not attach it to an issue, do not paste it.
- Keep `read-01`, `read-02`, both manifests, both `.sha256` files and `evidence.json`. Copy the images to
  two places outside the repository (two different disks), and never edit, compress in place or rename
  the originals.
- To hand the result to someone else, share the manifests and `evidence.json` — hashes, sizes, JEDEC,
  identity — not the images.
- A second *copy* is not a second *read*: for corroboration, the second directory has to come from §3.

## 6. What this proves, and what it does not

It proves: this unit identified itself, the part answered `0x12` with a documented JEDEC id and a 1 MiB
geometry, two passes read every byte identically, and the read phase sent `0x13` only.

It does **not** prove:

- that a stock unit can be read this way, or that `0x13` exists on stock firmware at all (§1);
- that the image can be **restored**: that needs a write, and a write needs a recovery route proven on a
  unit that is not your only one;
- that the QG flash map matches the stock layout: that is a manual, sector-by-sector comparison;
- that the SoC, the clock or the bootloader are what the documentation says: this protocol does not
  expose them;
- that an install may run. The record sets `authorises_install: false` on purpose; the installer reads
  `0x12` on the live link and refuses on anything it does not recognise, and there is no path that feeds
  this file into the gate.

One limitation of the protocol itself, stated rather than papered over: the `0x12` reply does not echo
the host nonce, so freshness rests entirely on the device's session token. A `0x13` reply is bound to the
session and a stale one is refused (`stale`), and `probe()` refuses two identifications that reuse a
session token — but a *replayed pair* of identifications recorded from a valid session, with two
different tokens, cannot be told apart from a fresh one by this host code. That is a property of a
protocol with no signing, not a bug the tools can fix on their side.

`0xB36014` is a special case: the read verifies, the record marks `write_capable_part: false`, and the
write path refuses that id (`web/fm1ota.js`). A verified read is about a unit and a part; it is not a
green light for that part.

## 7. Why it has not been run here

This session has no USB access to the unit, so the read has to happen on the machine the FM-1 is plugged
into. Everything up to the first frame is prepared and tested against a simulated unit; nothing has been
sent to a physical device from this repository's CI or from this session.
