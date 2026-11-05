# SECURITY AUDIT — Quantum GammaWave (QG-1), v1.8.7

Scope: firmware, boot/OTA chain, flash identification, update loader, installers (web and CLI),
backup/restore, the second core, and the repository's own supply chain.

Every finding below was read in the source and, where the change made it possible, exercised by a
host test in this tree. **No physical FM-1 was used.** A host test proves what the code refuses; it
does not prove how a part behaves, and nothing here is a claim of physical validation.

Labels: `[STATIC]` read in the source, `[HOST]` exercised by a host test, `[TARGET-BUILD]` compiled
with JieLi's pi32v2 toolchain, `[HARDWARE]` measured on a device. See
[`VALIDATION_MATRIX.md`](VALIDATION_MATRIX.md) for what was actually run in this session.

---

## 1. Findings and fixes

| # | Risk | Severity | Component | Evidence | Cause | Fix | Test | Result |
|---|------|----------|-----------|----------|-------|-----|------|--------|
| 1 | Writing to an unidentified or write-protected part | **High** | `firmware/src/project.c`, `firmware/hal/fm1_flash_id.h` | `[STATIC]` `[HOST]` | `flash_ok` was `fl_jedec_supported()` over **one** `0x9F` read. A floating bus reads `0x000000`, a glitch reads a neighbour, and a part with `BP0..BP2` set ignores erase/program while every call returns success — the firmware would keep "saving" into nothing. | Identification policy: two independent reads must agree, the ID must be one of the two documented 1 MiB parts, the capacity byte must be 1 MiB, and SR1 must be readable, idle and unprotected. Anything else leaves the firmware RAM-only (`flash_ok = 0`) instead of writing. | `tests/flash_id_test.c` (11 cases) | passed `[HOST]` |
| 2 | A package naming offsets outside the 1 MiB part | **High** | `firmware/src/ota.c` (`ota_ufw`) | `[STATIC]` `[HOST]` | `flash.bin` / `ota.bin` offsets and lengths came from the package header and were used without a range check; `off + len` can wrap 32 bits, and the part ignores the high address bits, so a wrapped range lands low — over the app area. | `ota_in_flash(off, len)` requires `off <= 1 MiB && len <= 1 MiB - off`, `len != 0`, and `flash.bin >= 0x93000` (it must cover the app window). Refused before the first erase. | `tests/ota_test.c` (`[HOST]`, needs a package) / compiled here | compiled `[HOST]`; runtime case requires a `.fwsc` |
| 3 | A stray SysEx frame satisfying a read request | Medium | `firmware/src/ota.c` (`ota_read`) | `[STATIC]` | The reply was matched on command, length and address but not on the flags byte the request was sent with, so a frame with another flag value could be taken as this read's data. | The reply must carry `fl == 0`, the value the firmware always requests. | covered by `tests/ota_test.c` | compiled `[HOST]` |
| 4 | Update loader that never times out | Medium | `firmware/loader/ldr_core.c` | `[STATIC]` | `ota_deadline` was only set for the step-1 session; in the loader it stayed 0, so a host that went quiet without unplugging (suspended laptop, crashed page) left the loader polling for ever — a boot loop the user can only leave by pulling the cable. | The loader sets a 20-minute session deadline (a full write takes ~2 min). On expiry `ota_read` fails, the write is abandoned and the record stays, so the next power-on resumes. | compiled `[HOST]` | compiled `[HOST]` |
| 5 | A half-written loader surviving boot cleanup | Medium | `firmware/src/ota.c` (`ota_boot_cleanup`) | `[STATIC]` | Cleanup sampled 4 bytes at `OTA_AREA` and 8 at `OTA_RES`: a partially erased/written staging area whose first word read `0xFFFFFFFF` looked clean, leaving a `LOADER.BIN` head the SPL could run. | The head (32 B), the first body page (32 B) and the whole record (8 B) are sampled; any non-erased byte clears the area. | `[STATIC]` | reviewed |
| 6 | Installer pairing an input with an unrelated output | **High** | `tools/fm1_install.py` (`pair_output`) | `[HOST]` | When exactly one MIDI output existed, the function returned it whatever its name, so the package could be served to a device that never answered the handshake. | The single-output shortcut now requires the names to match (ignoring Windows' numeric suffix). Otherwise the installer reports "FM-1 not found". | `tests/install_test.py` (`pairing()`) | passed `[HOST]` |
| 7 | Personal music archive written inside the repository | Medium | `tools/fm1_archive.py` | `[HOST]` | A new tool that writes the user's own music to disk had no destination policy; `--out .` would put it in the tree, one `git add -A` away from publication. | Archives default to `~/.local/share/qg1-backups`, mode `0600`, and a destination inside a git work tree is refused **before the device is opened** (`--allow-repo` overrides, deliberately). | `tests/backup_tool_test.py` | passed `[HOST]` |
| 8 | Second core enabled on `alive` alone | **High** | `firmware/hal/fm1_multicore.h` | `[STATIC]` `[HOST]` | `fm1_mc_start()` trusted that core 1 reached its loop. The documented failure of these units is a *wrong 32-bit load under load* (Melodee #17/#21, fm1-x0x #10): a unit that misreads could be handed audio parts on its first block, and the check word only catches a bad job *function/argument* read. | Before the core is enabled, eight fixed jobs run with answers computed on core 0 through the shared mailbox. One wrong word, one changed job number or a late job and the core is held for good; the firmware plays on one core and reports `CORES 1 (BAD READ)`. | `tests/mc_selftest_test.c` | passed `[HOST]` |

## 2. What is *not* fixed, and why

| # | Gap | Severity | State | Why it is still open |
|---|-----|----------|-------|----------------------|
| A | No cryptographic signature on packages | **Critical for production install** | **Chain implemented, no key pinned** | The Ed25519 chain now exists in both installers (`tools/fm1_sign.py`, `verifyManifest` in `web/fm1ota.js`) and `tools/fm1_keys.json` is empty on purpose, so nothing is trusted and every package is refused. What is still missing is a custodian: a key pair generated and held outside this repository (`docs/SIGNING_CUSTODY.md`). A hash alone proves *which bytes*, not *who wrote them*: without a pinned key a mirror that serves a different package with its own hash is indistinguishable. Do not invent keys, do not reuse a test key, do not disable the hash check to make the installer "work". |
| B | No anti-rollback | High | Open | The SPL and bootloader are the vendor's and **must not be modified**. A version floor can only live in the app, which an older package replaces. Documented as a limit, not a bug. |
| C | Single-bank update window | High | Open (by layout) | The app area `[0x4000,0x93000)` is written in place: a power loss mid-write leaves a device that needs the loader again (which the record preserves) or the USB_KEY / Mask-ROM path. There is no A/B slot and no room for one without changing the layout. |
| D | No raw-flash backup or restore over USB | High | Open (by protocol) | The firmware's archive protocol exposes *musical objects* (ids 0..10, 40..71), not flash addresses. A raw dump needs the recovery path in [`RECOVERY_GUIDE.md`](RECOVERY_GUIDE.md), not a new command invented here. The new CLI says so in its own manifest (`no_raw_flash`). |
| E | Secure boot | — | **Not claimed** | The vendor bootloader does not verify a signature over the app. Nothing in this repository may change that, and no document here says "secure boot". |

## 3. Supply chain, as it stands today

* **Integrity:** CRC16 in the UFW header, the JLFS entries and the flash head; `flash.bin`'s CRC is
  checked *before* any erase (loader step 2b), and every app sector is read, written and read back.
* **Authenticity:** none in firmware. The web installer and `tools/fm1_install.py` require a SHA-256
  (nearby `.sha256` / `SHA256SUMS`, or `--sha256`); `web/make_site.py` publishes `sha256` and `size`
  for every package in `firmware/versions.json`, and the page verifies the downloaded bytes.
* **Provenance:** `web/make_site.py` refuses a package without the `FELUCCA-LOADER-1` marker, so a
  package patched from a vendor image cannot be published by accident.
* **Secrets:** none in the tree. No private key, token or password is read, written or logged by any
  tool added in this change; the backup tool writes `0600` files outside the repository.
* **Dependencies:** the browser pages have no third-party runtime dependency; the CLI tools need
  `mido` + `python-rtmidi` and the generators need `Pillow`, `fonttools`, `numpy` (see `AGENTS.md`).

## 4. Residual risk

With 1, 2, 3, 4, 5, 6 and 8 fixed, the remaining brick/soft-brick risk on this layout is **the
single-bank write window (C)** and **the missing signature (A)**. Both are structural. Nothing in
this repository can close them without the vendor bootloader or a production key, so the honest
verdict stays:

**NO-GO for installation on the main unit.** Software-verified only; physical identity, backup,
restore and USB_KEY recovery must be demonstrated on a laboratory unit first
([`INSTALLATION_SAFETY.md`](INSTALLATION_SAFETY.md)).
