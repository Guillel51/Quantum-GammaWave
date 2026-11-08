# FM-1 viability, installation safety and the installer decision

Date: 2026-11-08. Tree: `f4ccc11` (`main`, clean). Class of everything below: **read from this repository and
from public sources; nothing was measured on an FM-1**, because no unit is attached to this environment.
No write of any kind was sent to a device, and none will be: `AGENTS.md` §3 forbids it permanently.

## Verdict, in one paragraph

**GO CONTROLLED for the software path; GO CONDITIONAL for the first physical write on your unit; NO-GO for any
claim of "unbrickable".** The package, the signature, the pinned key, the write bounds and the runtime admits
are in place and tested (`[HOST]`, `[TARGET]`), and the installer now refuses a unit whose NOR it cannot
identify. What is still missing is not code: it is one reading and one file from *your* unit (its JEDEC +
geometry, and a full 1 MiB dump of that same unit taken before the first write) plus a recovery route you have
physically rehearsed. Those three cannot be produced by me, and no constant in this repository may stand in for
them. The one thing that is **impossible**, not merely pending, is a guarantee of atomicity on this hardware:
the FM-1 is single-bank, so a power cut during the app erase/write leaves the unit unbooted until an external
tool restores it — see §10.

## 1. Repository state, as measured

| Fact | Value |
|---|---|
| `HEAD` | `f4ccc110aaeb10647ced622bcece8ca647a88c52`, tree clean |
| Branches | `freebuff/4549bd389852a9607bba2622` (this work), `freebuff-base` |
| Tags / Releases | **none** (the API returns an empty release list) |
| Open issues | **none**; issues list is empty |
| Pull requests | 72…63 all **closed**, none open (the most recent: *Audit/v1.9 fm1 safety*, *Security/transport require proof*) |
| Workflows | `c-cpp.yml`, `emulator.yml`, `target.yml` — no Pages, no release publisher |
| Host suite | **`ALL HOST TESTS PASSED`**, 5 201 `ok`, exit 0 (`[HOST]`) |
| Package | `build/felucca.fwsc` 611 163 B, `FM-1_982`, sha256 `deb7e343…`, built twice identically at this commit |
| Signature | `verified: FM-1 version 1.9.0, key qg1-release-1` |
| Emulator | `build/emu/felucca.wasm` 1 065 573 B, same commit as the firmware |

**Superseded sizes.** The build was moved to `-Oz` after this report (matrix §16): the package is now
610 806 B, RAM code 19 444 B of 24 576 and the same 1 MiB image/`FM-1_982`, so the byte figures above describe
`f4ccc11`, not this tree. Everything this report decides is about structure — the write window, the gates, the
protocol — and none of it changes with the size of the image. The emulator size is the same because the
WebAssembly is built from the host sources, which `-Oz` does not touch.

There is no open PR, no issue and no release to reconcile: the repository's work arrives as squashed merges to
`main`, which is why the audit trail lives in `docs/` rather than in issue threads.

## 2. What the system actually does (read from the code, not from documents)

The install channel is **device-pull**: the device asks, the host serves. There is no host-initiated write and
no host-initiated read of the flash outside the three query commands below.

| Command | Direction | Meaning | Bound by |
|---|---|---|---|
| `0x11` | device → host | identity, 34 B, model text at `d[6:33]` (`FM-1_982`, `ota-FM-1_982` in the loader) | `web/fm1ota.js` `qryFrame` |
| `0x12` | device → host | **NOR identity + geometry**, 24 B: status, JEDEC, page/sector/part sizes | `firmware/src/ota.c:416-545`, **needs a session token** |
| `0x13` | device → host | **read-only flash read**, 256 B per reply, offset + length + session | `firmware/src/ota.c:416-583` |
| `0x30` | device → host | device requests a range of the package; host answers | `web/fm1ota.js:243` bounds `addr+len` against the image |
| `0xE0000000` / `0xF0000000` | device → host | "check again" / "committed" | `firmware/src/ota.c` `ota_stage` |

The write itself is performed by the **staged loader**, not by the browser:

- `firmware/loader/ldr_core.c:34-41` — `LDR_APP_LO 0x4000`, `LDR_APP_HI 0x93000`, staging window
  `[0xE0000,0xE9000)`, autosave `[0xE5000,0xE8000)` never erased as a record, `LDR_FL_MAX 0x100000`.
- `ldr_core.c:139-173` — the *whole* package is validated (per-sector CRC16) **before anything is erased**;
  sectors that already match are skipped; each 4 KiB sector is erased then programmed 256 B at a time; progress
  is reported per sector.
- `ldr_core.c:117-121` — a host that goes quiet without unplugging hits a deadline; the record survives and the
  next power-on resumes.
- `firmware/src/ota.c:318-330` — if the package carries the **official** loader (recognised by CRC), that loader
  rewrites the whole `flash.bin` *including the head*, so QG compares the package's head `[0,0x4000)` with the
  device's current head byte for byte and **refuses (-5) if one byte differs**. Our own loader has no such
  power: it writes the app area only.

So the boot head is never written by QG's own path, and the one third-party path that would write it is
additionally guarded by an exact-match check.

## 3. Identity: what is known, what is not

- `0x11` proves **communication and a reported identity**. `FM-1_982` is what this build reports; stock units
  report `FM-1_0NN`. Historical notes in this project mention `FM-1_015` and a separate `FM-1_909`; they are
  **different observations, not one device's history**, and neither is your unit's JEDEC.
- `0x12` is the only command that reads the NOR. It returns the JEDEC and the geometry the part reports, and the
  firmware validates the geometry against the identified part before a session is issued (`ota.c:559`).
- **Your unit's JEDEC is unknown here.** No unit is attached. It is *not* assumed from the allowlist: the
  installer asks the device and refuses anything outside `0x856014` (Puya P25Q80H) and `0xB36014` (UCUN
  UC25HQ80) — `web/fm1ota.js:42-62`. A part that answers `0xFFFFFF`, `0`, or an undocumented ID is refused
  before a single frame.
- Capacity being equal is **not** compatibility, and no public source establishes identical erase geometry,
  status-register protection or write timing for the two parts. M-Flasher reports owner backups/restores on the
  UCUN part, which is community evidence of interoperability, not a datasheet comparison.

## 4. Flash map, with the class of each row

QG's own constants and the community map agree on the app and config windows. Confidence is stated per row;
"never written" means no code path in this repository writes it.

| Range | Size | Function | Evidence | Confidence | QG writes it? |
|---|---:|---|---|---|---|
| `[0x00000,0x04000)` | 16 KiB | flash header, JLFS directory, bootloader at `0xA0`, `isd_config`, chip key | `tools/fm1pkg_make.py:7-11,116`, `fm1_restore.py:56`, community wiki | high (structure), **low** for exact bytes of a given unit | **no** — package carries it, loader never writes it; official loader only with exact-head match |
| `[0x04000,0x93000)` | 0x8F000 | app area: `app.bin` at `0x4120`, `cfg_tool.bin` after the slot | `ldr_core.c:34-35`, `fm1pkg_make.py:121-137` | high | **yes**, sector by sector, verified by readback CRC |
| `[0x93000,0xE9000)` | 0x56000 | VM / configuration store; record area inside `[0xE0000,0xE9000)` | `ldr_core.c:36-37`, `fm1pkg_make.py:139`, wiki | high | only the record window, and the autosave `A/B` is protected |
| `[0xE5000,0xE8000)` | 0x3000 | autosave `A/B` + FM6 voice bank — **user data** | `ldr_core.c:38-39` | high | **never** as a record (explicitly excluded) |
| `[0xE9000,0xEA000)` | 4 KiB | Bluetooth pairing (BTIF) | `fm1pkg_make.py:141`, wiki | high | no |
| `[0xEA000,0xFC000)` | 0x12000 | USR resources / patches | `fm1pkg_make.py:142`, wiki | high | no |
| `[0xFC000,0xFF000)` | 0x3000 | **not documented** by the community map | wiki lists no stock region | **low** | no |
| `[0xFF000,0x100000)` | 4 KiB | `key_mac` reservation | `fm1pkg_make.py:122`, wiki | high | no |

The two rows that matter for safety are the first and the last-but-one: the head is never touched by our
loader, and the undocumented `0x3000` window is never touched by anything.

## 5. Backup and recovery, classified as requested

| Capability | Implementation | Class |
|---|---|---|
| Read the whole NOR, twice, compare | `tools/fm1_backup.py`: two full passes, per-pass SHA-256, chip/geometry in the manifest, frame log | **implemented, simulated** (`[HOST]`); needs a session, so it runs against QG firmware or the ROM loader |
| Validate an archive independently | `tools/fm1_restore.py`: 1 MiB, two identical passes, hash, chip, geometry, read-only frame log | implemented, `[HOST]` |
| Compare archive against the unit again | `fm1_restore.py --compare` via `0x13` | implemented, `[HOST]`; `0x13` is unverified on hardware |
| Raw restore of the app area | `fm1_restore.py --write` **refused by design**; `flashrom` path refuses to include the boot header unless `--include-head` is passed explicitly | "technically possible, not demonstrated" |
| Restore including the boot head | only through the chip's ROM UBOOT with a full-dump writer | **depends on an external tool** |
| Recovery of a unit whose boot head was damaged | M-Flasher (full 1 MiB restore, documented executed on an FM-1 by its author), not by QG | **depends on external tool + ROM access** |
| Recovery when the unit enumerates as nothing | FM-1 Transporter (RP2040 on D+/D− sending USB_KEY at power-on) forces mask-ROM UBOOT; the tool then rewrites the **app region only** | **depends on additional hardware**; does not repair `[0,0x4000)` |
| Guaranteed recovery after any power cut | — | **not demonstrable** (§10) |

Two facts about the *first* install matter more than any of the above, and both are read from the code:

1. **The browser installer never takes a backup** (`grep -c backup web/fm1ota.js` → 0). It cannot: the device
   pulls, and `0x13` requires a session that only the update path opens.
2. **A stock unit has no `0x13` at all**, so there is no in-app way to dump your unit before the first QG
   install. The only pre-install snapshot route is external: ROM UBOOT + a full-dump tool (M-Flasher class),
   which is exactly what the community has demonstrated on this device family.

## 6. Cryptographic reality

- **Ed25519**, detached manifest over the package hash and the build record; the browser verifies with WebCrypto
  and the CLI with `cryptography`. One key is pinned: `qg1-release-1` = `70b6036f…07fcd40`
  (`tools/fm1_keys.json`); its private half lives at `~/.fm1-keys/qg1-release.pem` (0600, outside the checkout).
- **Fail-closed**: with no pins every package is refused (`fm1_sign.py` `check` says so, and the custody test
  asserts it); a package signed by a *different* key under the same id is refused (`signature`); an id that is
  not pinned is refused (`key`); rotation and revocation are exercised by `tests/key_custody_test.py` (56 checks).
- **Downgrade**: `min_firmware` is in the signed manifest, and `requireProof` refuses an unknown installed
  version outright ("a downgrade cannot be ruled out"), rather than assuming current.
- **What a signature does not do**: it proves who built these bytes; it does not make them safe, and the stock
  loader does not check signatures at all. Authenticity protects the installer's decision, not the silicon.
- **No anti-rollback in hardware.** There is no documented OTP/fuse version counter on this SoC that a package
  could bump, so a determined user with the vendor tool can flash an older image. That is a property of the
  platform, not a gap in QG.

## 7. The package

`build/felucca.fwsc`: 611 163 B, identity `FM-1_982`, `min_firmware 1.0.0`, version 1.9.0, sha256
`deb7e343…`, signed over the build record of commit `f4ccc11` with `dirty: []`. Inside: a UFW container with
`flash.bin` (type 0, `0x93000` B) and `ota.bin` (type 100, our loader). `APP_SLOT` is `0x8DFBC`; the current
app is well inside it, and `fm1pkg_make.py:129` refuses an app larger than the slot instead of growing the
write range. The head inside `flash.bin` is there because the container format requires it; our loader never
writes it, and the official-loader path refuses unless it equals the unit's head exactly.

## 8. The installer, step by step

`web/fm1ota.js` `requireProof` admits a transfer only with: a product containing `_`, a **known** installed
firmware version, a signed manifest whose version/`min_firmware`/size are well-formed, a package whose own
identity equals the requested product, `manifest.sha256`/`size` equal to the actual bytes, a clean provenance
record bound to those bytes, and a `build_sha256` in the signature that names that same record. The proof is
frozen, single-use, and expires; `admit()` refuses a used or stale one "before any frame".
`admit()` then opens the chip session: `0x12` must answer with a JEDEC inside the allowlist, the geometry must
equal the identified part's, and a session token must come back. Only then does the transfer run. After the
loader commits, the installer waits for the device to return and **requires it to report the QG product**;
anything else raises `mismatch`.

`web/fm1verify.js` (Verify Only) is deliberately weaker and says so: `canInstall` is `false` in every answer,
`gate.chip` is `not-here`, and it lists `flash.jedec` as *unknown* with the reason — the identity query does not
carry it and `0x12` needs a session that only the install path opens. That is a design boundary, not a bug, and
it is exactly why the installer, not the diagnostic page, is where the chip is identified.

## 9. The fourteen requirements, decided

| # | Requirement | State | Evidence (class) | What is left |
|---|---|---|---|---|
| 1 | Reproducible AC791N build | **GO** | two builds, same sha, zero SDK warnings (`[TARGET]`) | CI should pin the same SDK tag |
| 2 | Whole QG package | **GO** | identity, hashes, bounds, slot limit (`[TARGET]` + `[HOST]`) | — |
| 3 | Valid production signature + pinned key | **GO** | `verified`, pinned fingerprint, refusal tests (`[HOST]`) | off-repo key backup is the owner's |
| 4 | Unit identity | **GO CONDITIONAL** | `0x11` proven by tests; your unit not read (`[HOST]` only) | one handshake on your unit |
| 5 | Real JEDEC and geometry | **GO CONDITIONAL** | `0x12` implemented, allowlist + geometry check (`[HOST]`) | one `0x12` on your unit |
| 6 | Flash write compatibility | **GO CONDITIONAL** | loader writes only the app window; per-part geometry enforced (`[HOST]`) | one write, or an accepted external report for your part |
| 7 | Applicable memory map | **GO CONDITIONAL** | table in §4, cross-checked with the community map | confirmation of `[0xFC000,0xFF000)` on your unit |
| 8 | Verified prior backup | **NO-GO** | no pre-install dump exists for your unit; the installer never takes one | external full 1 MiB dump + two-pass hash **before** the first write |
| 9 | Demonstrated recovery | **GO CONDITIONAL** | M-Flasher reports an FM-1 restore; QG's own raw restore is refused by design | you rehearse the ROM-UBOOT route on your unit |
| 10 | Protected write paths | **GO** | `ldr_core.c` bounds, whole-package CRC before erase, head never written, official loader gated by exact head match (`[HOST]`) | — |
| 11 | Stress and stability | **GO** | 5 201 host checks, ASan/UBSan fuzz, golden renders (`[HOST]`) | — |
| 12 | RAM/CPU headroom | **GO** | RAM code 21 768/24 576, calls reach verified, budget test (`[TARGET]`) | — |
| 13 | Installer/emulator/site synchronisation | **GO** | one commit for wasm, firmware and package; `make_site` refuses a mismatched record (`[HOST]`) | — |
| 14 | Post-install revalidation | **GO** | identity required to equal the product after reboot; `mismatch` otherwise (`[HOST]`) | — |

Eight rows move from the old blanket NO-GO. Six of them are genuinely software and are now closed; three stay
conditional on a single reading from your unit, and one (prior backup) is a **NO-GO until a file exists** — the
installer cannot manufacture it, and pretending otherwise would be the exact failure this project forbids.

## 10. Impossible, as opposed to not yet done

1. **Atomic app replacement on a single-bank part.** The FM-1 has no A/B app bank; the loader erases and rewrites
   `[0x4000,0x93000)` in place. A power cut mid-write leaves an app that will not boot. The resume record makes
   the *next* power-on resume the transfer, and the boot head is untouched, so the unit is recoverable **with an
   external tool** — but "never unbooted, under any power cut" is not achievable here. No amount of software in
   this repository changes that.
2. **A guarantee against bricking.** Same reason, plus: a unit whose `[0,0x4000)` is damaged needs ROM UBOOT and
   a full-dump writer; the community tools that document a full restore (M-Flasher) and the ones that only write
   the app region (Unbricker, Transporter) are not the same capability, and only the former reaches the head.
3. **Chip identity without reading it.** A JEDEC cannot be inferred from the model string, the capacity, or the
   package. It has to come from `0x12` on that unit.
4. **Universal compatibility across FM-1 revisions.** The revision space is not enumerated anywhere; QG can only
   allowlist what it can identify and refuse the rest.
5. **Hardware anti-rollback.** No secure version counter is documented; `min_firmware` is enforced by the
   installer, not by the silicon.
6. **Physical proof from host tests or the emulator.** An emulated reboot is not a power cut, and a wasm build is
   not a chip. Any statement to the contrary would be fabrication.

## 11. Residual risks, stated so they can be accepted knowingly

| Risk | Probability | Impact | Mitigation in place |
|---|---|---|---|
| Power cut during the app write | low | unit does not boot until recovered | resume record, head untouched, external ROM-UBOOT restore |
| First install from stock goes through the stock update service | — | QG's own bounds do not apply to that service | package carries our loader; head-match guard applies when the official loader is present |
| Your part is an undocumented revision | low | geometry mismatch | `0x12` geometry equality check before any frame |
| Undocumented `0x3000` window at `0xFC000` holds unit data | unknown | data loss if written | QG never writes it |
| Release key lost or leaked | low | cannot sign, or can sign wrongly | off-repo 0600 key, rotation/revocation tested, fail-closed default |
| Recovery route unavailable when needed | medium | no way back | external full-dump tooling documented and rehearsed *before* the first write |

## 12. The decision

```
Software path ............................ GO CONTROLLED
First physical write on the owner's unit .. GO CONDITIONAL
"Unbrickable" guarantee .................. NO-GO (impossible on this hardware)
```

**GO CONTROLLED** means the installer is enabled and enforces, by itself: allowlisted live JEDEC + geometry,
signed release with a pinned key, provenance bound to the exact bytes, single-use expiring proof, a loader that
writes only the app window, and a post-install identity check. Nothing about that depends on trusting a document.

**GO CONDITIONAL** means three things must exist before you press Install, and all three are yours because they
are readings from your unit, not software:

1. the installer's own `0x12` reading on your unit returns `0x856014` or `0xB36014` **and** a geometry that
   matches the part — if it returns anything else, the installer refuses and the answer is NO-GO for that unit;
2. a full 1 MiB dump of **that** unit, taken through ROM UBOOT with an external tool, two identical reads, chip
   CRC agreeing, SHA-256 recorded — the installer has no way to take it before the first write;
3. the recovery route rehearsed once: the unit enumerates as `WL82 UBOOT1.00`, the dump is recognised, and you
   know which tool would write it back. Without step 3 the dump is a souvenir.

I cannot execute any of the three, and I will not simulate them. What I can and did do is make sure that when
they exist, nothing else is missing.

## Sources, so every external claim can be checked

- Community flash map, package format, update protocol, hardware overview:
  <https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Flash-Storage.md>,
  <https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Firmware-Package-Format.md>,
  <https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Update-Protocol-and-Installers.md>,
  <https://github.com/sbegueria/fm1-dev-wiki/blob/master/docs/Hardware-Overview.md>
- Full-flash read/restore, ROM UBOOT access, documented FM-1 restore:
  <https://github.com/jvitkauskas/m-flasher> (`docs/usage.md`, `docs/development.md`, `docs/safety.md`),
  <https://github.com/jvitkauskas/fm1-backup>
- App-region-only recovery, and the hardware USB_KEY route:
  <https://github.com/Quixotic7/MvaveFM1Unbricker> (and `TRANSPORTER-GUIDE.md`),
  <https://github.com/kurogedelic/FM-1-transporter>
- Vendor download mode and update modes (dev-kit procedure, not an FM-1 procedure):
  <https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/getting_started/preparation/update.html>,
  <https://doc.zh-jieli.com/AC79/zh-cn/release_v1.2.0/module_example/system/update.html>
- Stock firmware workbench and its statement that the standard update protocol stays available:
  <https://github.com/m-vave/fm1_open>

Claims from these sources are labelled as community or vendor as they appear above; none of them is a
measurement of the owner's unit, and none is used here to move a requirement that needs one.
