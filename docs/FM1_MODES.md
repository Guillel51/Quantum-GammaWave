# FM-1 modes, transports, backup and recovery — what each path can actually do

This document exists because the installer's gates were being argued without a map of the machine.
It consolidates what the community projects have published, separates it from what *this* repository
has measured, and states the one thing a browser cannot do. Nothing here is a claim about the owner's
unit: his unit has answered exactly twice (the M-Flasher record in §10 and nothing else), and both
answers are quoted, not generalised.

Every statement carries its class, as `AGENTS.md` §4 requires:

| class | meaning |
|---|---|
| `[code]` | read in the sources of the project named, not executed here |
| `[3rd-hw]` | reported as tested on a physical FM-1 by another project's author |
| `[doc]` | asserted by documentation only, no code or test behind it |
| `[host]` | executed in this repository's host tests |
| `[my-FM1]` | read from or written to the owner's unit — **never written, so far never read beyond the record** |

---

## 1. The four USB identities, and the one thing WebMIDI will not tell you

| mode | VID:PID | what it enumerates as | can the browser use it? |
|---|---|---|---|
| stock app, V15 | `4C4A:C755` `[doc]` `[3rd-hw]` | USB-MIDI, port named `FM-1` | yes: MIDI + SysEx |
| stock update loader (`ota-FM-1_0NN`) | `4D4A:4155` `[doc]` `[3rd-hw]` | USB-MIDI, port named `ota-FM-1` | it answers MIDI, but it is the vendor's program: no Felucca image is fed to it (`foreignloader`) |
| this project's loader (`ota-FM-1_900`) | as above `[code]` | USB-MIDI, port named `ota-FM-1` | yes: `resume()` is a documented, tested path |
| ROM download mode | `4C4A:8057` `[doc]` `[3rd-hw]` | **mass storage / SCSI, not MIDI** | **no** |
| m-bootloader MIDI mode | `1209:0002` `[code]` | USB-MIDI | not part of this installer (a third-party binary; see §5) |

**WebMIDI exposes no VID or PID.** `MIDIPort` carries `id`, `name`, `manufacturer`, `version` and a
`state`; the USB ids are not in the spec and no browser provides them. So a page in this repository
can never classify a unit by `4C4A:C755` versus `4C4A:8057`, and any code that claims to do so is
inventing a field. What the page *can* do, and does (`web/fm1verify.js`, `web/fm1state.js`), is
classify by the port name plus the identity answer `F0 00 32 45 … 7F F7` → `FM-1_<n>`:

- `FM-1_015` → stock V15; `FM-1_9NN` → a Felucca firmware; `ota-FM-1_900` → this project's loader;
  any other `ota-FM-1_<n>` → the vendor's update mode; nothing → not connected.

The ROM mode is therefore **outside this transport**: when the stock app obeys `7D` it re-enumerates
as a SCSI device, disappears from WebMIDI's port list, and no amount of retrying will show it. That
is why `web/fm1state.js` declares `ENTERING_DOWNLOAD_MODE` and `ROM_DEVICE_DETECTED` unreachable from
this page instead of faking them, and why the backup procedure in §4 is not a browser feature.

## 2. `7D` and `7F` are not the same command

Both are single-byte USB-MIDI frames to the running app, and treating them as interchangeable is how
an installer sends the wrong thing:

- `F0 22 24 35 7D F7` — *enter download/ROM mode*. The stock app reboots into the SOC's ROM loader,
  which enumerates as mass storage. Nothing is written by this frame; the unit leaves MIDI behind
  `[doc]` `[3rd-hw]`.
- `F0 22 24 35 7F F7` — *begin an update session* with the program already running (the app stages
  the loader, or a loader stages the image). This is the frame the update path uses `[code]`,
  `[host]` — `web/test_update_mode.mjs` and `web/test_verify_only.mjs` count every frame sent.

This installer sends `7F` only inside the documented install/resume path, and `7D` **never**: the
first install is M-Flasher's job (`docs/M-FLASHER.md` §6), not this page's.

## 3. Flash map and the single-bank window

The map this project works from (`docs/FLASH_MAP.md`, `[doc]` from FM-1-RE and the vendor packages):

```
0x0000 .. 0x4000   boot area: stock SPL + boot head           never written by QG or by M-Flasher
0x4000 .. 0x92FFF  application (the image the package carries) what an install rewrites
0x93000 .. ...     VM (user voice/preset data)
0xE0000 .. 0xE5000 staging: ota.bin, request record            written by the update route
0xE5000 .. 0xE8000 autosave A/B + FM6 banks (user data)        not part of an install
0xE9000 ..         BTIF / USR
```

The stock V15 updater is **single-bank**: it receives the image and writes it in address order, so
sector `0x4000` — the app's entry point — is rewritten near the start of the write, and every sector
after it is unprotected until the write finishes `[doc]` (FM-1 Developers' Wiki, *Update Protocol and
Installers*; FM-1-RE's `docs/io/11-ota-protocol.md`). For this package that window is the whole
`0x4000..0x92FFF` pass: 1167 chunks as the host counts them (`74 + 1167` in the page's progress bar),
i.e. **the entire write, not a corner of it**.

What protects the unit during that window:

- the loader checks each sector as it writes and does not commit on a mismatch `[code]`;
- a dropped cable leaves the unit in its loader, so the same package can be installed again
  (`RECOVERY_REQUIRED`, not a brick) `[code]` `[host]`;
- nothing in QG touches `0x0000..0x3FFF`, so the SOC's ROM and the boot head survive and the ROM
  download mode remains available for a host tool `[code]` `[doc]`.

What does **not** protect it: a power cut. If the unit loses power inside the pass, the app area holds
a partial image, the unit boots nothing, and the way back is a host program over ROM mode — with the
unit's own dump, not another unit's (§4).

TransitFM1 (`kurogedelic/FM-1-transporter`'s sibling, `Quixotic7/TransitFM1`) attacks exactly this:
a boot stub at `0x4120..0x4FFF`, a marker at `0x91FE0`, the commit sector `0x4000` written **last**, so
an interrupted write leaves a recovery entry point intact `[code]` `[doc]`. Its own first install from
stock V15 has no such protection (17 cut points, `[doc]`; one real cable-pull from ChoralRoot came
back in update mode, `[3rd-hw]`). Adopting it would close the window for *updates*; it cannot close it
for the *first* install, which is the one that matters here. It also consumes Felucca `.fwsc`
packages, so the layout is compatible in principle — but this repository has no hardware evidence for
the stub and will not ship one it cannot test.

## 4. Backup: the two real routes, and what each costs

### A. M-Flasher over ROM download mode — the route with the best guarantees

`[doc]` `[3rd-hw]` `[code]` (m-flasher 0.9.4):

1. the stock app receives `7D` and reboots into ROM mode (`4C4A:8057`, mass storage);
2. the host loads M-Flasher's **flash loader into RAM** and talks to it over that transport;
3. it reads the whole 1 MiB, reports JEDEC / chip key / size, and writes a dump plus a JSON record.

The `sha256` in such a record is the SHA-256 of **the complete 1 MiB image read in that run**
(`m-flasher/src/ops.rs`, the record is built from the buffer that was read) — not of "the backup" as a
package, and not a proof of internal consistency. Consistency comes from a second independent read
and from the chip's CRC32; neither is in the JSON. That is why `web/fm1record.js` accepts a record
only together with the dump it names, re-hashes the dump in the page, and treats the record as
`modelOnly` — it can bind a model and a chip, never authorise a write by itself.

### B. m-bootloader's RAM stage — read-only, no permanent install

`[code]` `[doc]` (m-bootloader 2.3, `docs/SPEC.md` §10, §11.7):

- the stage runs from retained RAM and reads raw flash in 1–512 byte chunks (command `0x23`),
  including the boot and key area; `SPEC.md` states nothing in it writes flash;
- two launch routes: `run-ram` over ROM mode (SCSI, so **not** from a browser), or the **firmware
  update route**, where the stage is delivered as `ota.bin` (~`0xE3000`, request at `0xE7F00`) and the
  stock boot loader jumps to it in RAM at `0x01C0A800`;
- cost of route B: the staging area is overwritten **before** the stage runs, so a dump taken through
  it cannot show those sectors as they were. On the FM-1 map that region abuts the autosave/FM6
  user-data banks (`0xE5000..0xE8000`), which is a real, bounded loss of user data and not a
  read-only operation in the strict sense;
- it is a third-party binary with its own licence; this repository does not vendor it.

### What a backup must satisfy (unchanged, `docs/RECOVERY_GUIDE.md` §2)

Exactly 1 048 576 bytes, two independent reads compared byte for byte, SHA-256 over the file, unit
identity, chip key, JEDEC, region map, tool and version, errors, and two copies off the repository.
The dump is personal data (presets, songs, unit id) and is never published.

## 5. Recovery: what exists, and what each path cannot do

| path | reads | writes | keeps stock boot loader | needs extra hardware | tested on a physical unit | cannot recover |
|---|---|---|---|---|---|---|
| ROM mode + M-Flasher | 1 MiB `[3rd-hw]` | any sector it is told to `[3rd-hw]` | yes (it is the SOC's own mode) | no | yes, by its author | nothing by itself: it is the tool, the dump is the safety |
| MvaveFM1Unbricker | 1 MiB, twice, compared `[code]` | only `0x4000..0x92FFF`, byte-verified `[code]` | yes | no | `[3rd-hw]` (soft-bricked units) | a boot area damaged by something else; it does not restore a full dump |
| fm1_rescue / wiki recipe | — | — | — | — | `[doc]` | untested here; treat as documentation |
| FM-1 Transporter (RP2040/RP2350) | depends on the firmware `[code]` | ditto | ditto | yes: a Pico, wiring, USB_KEY sequence | `[3rd-hw]` | the board and pinout must match; a "Zero" is not automatically the same board |
| m-bootloader stage | 1 MiB `[code]` | nothing `[code]` | yes | no (ROM route needs a host with SCSI) | `[3rd-hw]` | — (it is a reader) |
| TransitFM1 stub | — | app area, commit last `[doc]` | yes | no | `[host]` + one cable-pull `[doc]` | an interrupted *first* install |
| this installer's browser path | identity + chip only | nothing over a stock unit; app area over QG | yes | no | never (no write has been executed) | the ROM mode it cannot see |

The wiki's warning is the summary of the table: once the first sector of the app area is rewritten,
no rescue code is left on the unit, and a unit that loses power there comes back dark unless a host
tool reaches it through ROM mode.

## 6. What this means for the installer

- **The backup cannot be produced by the page.** Both real routes need either a SCSI host or a
  third-party binary; WebMIDI reaches neither. The page therefore accepts a backup it can *verify*
  (`web/fm1record.js`: 1 MiB, SHA-256 recomputed, model and chip bound) and refuses to pretend it
  made one.
- **The first install stays a host operation** (M-Flasher `Install firmware`, unchanged decision,
  `VALIDATION_MATRIX.md` §28/§30). The page's `stockfirst` message is not a false gate: it points at
  the program that has actually done it, and the package it needs is the same signed `.fwsc`.
- **The page's state names are observations**, not hopes: four of the seventeen are declared
  unobservable over WebMIDI (`ENTERING_DOWNLOAD_MODE`, `ROM_DEVICE_DETECTED`, and — because the loader
  erases and checks inside its write pass — `ERASE_IN_PROGRESS` and `VERIFYING`). `web/test_state.mjs`
  pins that boundary.
- **The chip gate is unchanged**: a live `0x12` on the same link, the same attempt's app-link reading,
  or a record whose dump the page hashed. A stock unit can supply only the last, which is why the
  record is `modelOnly` and cannot authorise a write.
