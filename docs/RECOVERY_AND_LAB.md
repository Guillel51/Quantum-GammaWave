# Recovery, key custody and the lab plan (FM-1)

Status of every claim here is one of: **IMPLEMENTED**, **HOST TESTED**, **SIMULATED**,
**BLOCKED** (needs a unit), **NOT TESTED**. Nothing in this document has been run on a
physical FM-1, and no restore or recovery has been verified.

## 1. What the software already proves

| Piece | Where | Status |
|---|---|---|
| Physical NOR identity (0x12, two readings, geometry, fresh session) | `firmware/src/ota.c`, `tools/fm1_probe.py`, `web/fm1ota.js` | IMPLEMENTED · HOST TESTED (`tests/chip_test.py`, `web/test_web.mjs`) |
| Read-only flash (0x13: bounds, CRC, session, ≤256 B) | same | IMPLEMENTED · HOST TESTED · FUZZ TESTED |
| Two-pass backup with a byte-identical comparison | `tools/fm1_backup.py` | IMPLEMENTED · HOST TESTED (`chip_test.py`) |
| Ordered command log, identification included | `tools/fm1_backup.py` (`commands`, `command_counts`, `frames`) | IMPLEMENTED · HOST TESTED |
| One write decision shared by CLI and web | `tools/fm1_gate.py`, `web/fm1ota.js` | IMPLEMENTED · HOST TESTED (`tests/gate_test.py`) |
| Signed package (Ed25519) checked by both installers | `tools/fm1_sign.py`, `web/fm1ota.js` | IMPLEMENTED · HOST TESTED |
| Install refused without chip evidence and without a verified manifest | `web/index_pkg.html`, `tools/fm1_install.py` | IMPLEMENTED · HOST TESTED |
| Power-cut matrices for storage and OTA staging | `tests/storage_cut_test.c`, `tests/ota_cut_test.c` | SIMULATED |

A backup is **not** a recovery path. The manifest says so itself (`restore` field), and
`status` is `complete` only when every byte of the part was read twice and both passes
are byte-identical.

## 2. Two levels of recovery (design only, nothing built)

### Level A — USB/loader (needs a working loader)

1. Identify the unit (handshake) and the NOR (0x12). Unknown or unvalidated parts stop here.
2. Read the part twice and keep both passes (0x13). No write frame is sent by the backup tool.
3. Verify a signed manifest for the package that will be written.
4. Write only the application range; never the loader, the protected header or user data.
5. Read the written range back and compare byte for byte before declaring success.

**Gap today:** the restore side of step 4/5 does not exist as a tool, and the loader does
not verify signatures. Step 5 is the only thing that turns "wrote" into "wrote correctly".

### Level B — independent of this firmware

Needed when the application does not boot, the update loader is damaged, or the write was
interrupted during erase. Candidate paths, none of them validated here:

- USB_KEY / Mask-ROM download mode of the AC791N (vendor path; no tool in this repo).
- An external SPI programmer or a Raspberry Pi RP2040-based reader/writer wired to the NOR.
- Community tooling (`m-bootloader`, M-Flasher, FM-1 Transporter) as **reference only**:
  they are not verified for this firmware, and `m-bootloader` reports a UCUN unit that did
  not boot after its bootloader was installed.

**Status: BLOCKED.** This repository has no code that can recover a unit that does not
boot, and no such procedure has been executed. Do not treat any of the above as available
until it is built and rehearsed on a sacrificial unit.

## 3. Chip policy

- `0x856014` (Puya P25Q80H): the only JEDEC accepted for **writing**.
- `0xB36014` (UCUN UC25HQ80): identified and readable (diagnostics, backup), **refused for
  writing** by `tools/fm1_gate.py` and `web/fm1ota.js`. The community report of a unit that
  did not boot after a third-party bootloader is enough to withhold the write until a lab
  unit proves it.
- Anything else: refused. The firmware falls back to RAM-only storage rather than guessing.

A datasheet is not evidence that the loader can write a part safely; only a unit that was
identified, read, written and verified is.

## 4. Key custody (Ed25519)

Today `tools/fm1_keys.json` holds **no** public key, so every package is refused by both
installers. That is the correct released state, not a bug.

Procedure for the custodian:

0. `python3 tools/fm1_sign.py check` reports the state; `--require-key` makes an empty pin set an
   error (the release gate while no key exists).
1. Generate the release key **offline** (`fm1_sign.py keygen`), never on a networked host. The
   command refuses any destination that resolves inside the checkout, through `..` or a symlink, and
   creates the file `0600` without ever truncating an existing one.
2. Keep the private key on encrypted offline media, two copies in separate places, access
   limited to the release signer. `sign` refuses a key stored inside the repository.
3. Publish only the public key: `fm1_sign.py pin KEY.pub --keyid qg1-release-1`. `make_site.py`
   now ships `tools/fm1_keys.json` to the page as `meta.keys`, so pinning is enough for the
   web installer to start verifying; no code change is needed.
4. Sign each package (`fm1_sign.py sign ... --keyid ...`) and keep the manifest next to the
   package: `make_site.py` publishes it per version as `meta.versions[i].manifest`. `sign` refuses a
   `keyid` whose pinned key is not the one the private key belongs to.
5. Rotation: add the new key id, keep the old one pinned while old packages are still
   distributed, then remove it. Moving an id to a different key needs `pin --replace`. Revocation
   is removing the key id from the map; packages signed with it stop verifying with `key`.
6. Loss of the private key is not recoverable in-place: a new key id must be pinned and a
   new package signed.

Full procedure, roles and the release checklist: [`SIGNING_CUSTODY.md`](SIGNING_CUSTODY.md).
`tests/key_custody_test.py` runs the whole cycle with an ephemeral key.

What the signature does **not** do: it does not make the device verify what it boots. The
stock loader checks no signature, so this is not secure boot and must not be described as
such. The host refuses to hand an unverified package to the loader; a compromised host is
outside that guarantee.

## 5. Lab plan (before any write to a unit)

Preconditions, all of them, on a **sacrificial** unit:

- [ ] SoC and NOR identified; JEDEC read and recorded; geometry corroborated twice.
- [ ] Full backup, two independent passes, byte-identical, manifest `complete`.
- [ ] A second copy of the backup on different media, hash checked by hand.
- [ ] The unit can be put into USB_KEY/Mask-ROM or reached with an external programmer.
- [ ] Stable bench supply, no hub, nothing else on the MIDI ports.
- [ ] A known-good stock package available for the return path.

Order of experiments: identify (×10) → read whole part (×2) → write only the application
range → read back and compare → interrupt at staging → interrupt during erase → interrupt
during program → power cycle → resume from loader → recover from USB_KEY → restore backup →
verify full part. Each step stops the sequence on the first mismatch.

Measurements to record: time per 256-byte read, time per page program, worst-case block
latency with identification running, and whether the audio path glitches during 0x12/0x13.

**Status: BLOCKED.** No hardware is attached to this workspace; nothing above has been run.

## 6. Residual risks (unchanged by this session)

1. No production public key: installs are refused by design until the custodian pins one.
2. `0xB36014` units cannot be written; they can only be read and backed up.
3. No independent recovery path exists in this repository; a unit that does not boot cannot
   be recovered by this software.
4. Restore is not implemented, so a backup cannot yet be put back by our tools.
5. The loader does not verify signatures; host-side verification is the only gate.
6. Nothing about the physical part, its timing, its protection bits or its wear has been
   measured on a real FM-1.
