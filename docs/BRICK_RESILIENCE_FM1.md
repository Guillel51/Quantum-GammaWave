# FM-1 brick-resilience and worst-case audit (no hardware)

Date: 2026-11-04. Branch `audit/fm1-no-hardware`, HEAD `fe61aa2`.
No FM-1 was connected, flashed, reset or written to. No bootloader, SPL, ROM, partition
table or flash map was modified. Nothing was pushed, merged, released or deployed.
`main` was not touched.

This report separates **implemented**, **host tested**, **simulated**, **emulated**,
**target compiled** and **hardware not verified**. A green host test is not a physical
validation, and an emulated reboot is not a power cut.

The EQ biquad, the PUNCH-IN FX screen and their tests were **in progress work from the
previous task and were preserved untouched**; they are not part of this audit's changes.

## 1. What was investigated

Primary sources read (not summarised from memory):

- Boot chain, flash map, update protocol, recovery, installers, open questions:
  <https://sbegueria.github.io/fm1-dev-wiki/Boot-Chain-and-Bootloaders.html>,
  <https://sbegueria.github.io/fm1-dev-wiki/Update-Protocol-and-Installers.html>,
  <https://sbegueria.github.io/fm1-dev-wiki/Recovery.html>,
  <https://sbegueria.github.io/fm1-dev-wiki/Flash-Storage.html>,
  <https://sbegueria.github.io/fm1-dev-wiki/Open-Questions.html>
- `m-bootloader` (boot menu, checksum before start, download keys, wipe user data):
  <https://github.com/jvitkauskas/m-bootloader>
- `fm1-emu` (flags and env vars actually present in 0.4.0):
  <https://github.com/joakimeriksson/fm1-emu>
- NIST SP 800-193 (protect / detect / recover) and SP 800-218 (SSDF):
  <https://csrc.nist.gov/pubs/sp/800/193/final>,
  <https://www.securebydesignhandbook.com/docs/standards/us/nist-sp-800-218-ssdf-overview>
- SUIT architecture and manifest model: <https://www.rfc-editor.org/rfc/rfc9019.txt>,
  <https://www.rfc-editor.org/rfc/rfc9124.txt>
- TUF (rollback, freeze, key compromise):
  <https://theupdateframework.github.io/specification/latest/>,
  <https://theupdateframework.io/docs/security/>
- MCUboot test/confirm/revert and ESP-IDF OTA rollback and anti-rollback:
  <https://docs.mcuboot.com/design.html>,
  <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html>
- EMB3D threat enumeration: <https://emb3d.mitre.org/threats/>
- OWASP FSTM and ISTG firmware-update test cases:
  <https://github.com/scriptingxss/owasp-fstm>,
  <https://github.com/OWASP/owasp-istg/blob/main/src/03_test_cases/firmware/firmware_update_mechanism.md>
- W3C Web MIDI, "Malicious Firmware Updates": <https://www.w3.org/TR/webmidi/>
- Community reverse engineering of the same SoC (checksum-only update, no signature):
  <https://github.com/aroum/fm1-custom-fw/blob/main/REVERSE_ENGINEERING.md>,
  <https://github.com/AL-255/FM-1-RE>

Three claims the mission started from could **not** be sourced and are treated as
unverified, not as facts:

- **No public AC791N/pi32v2 errata sheet was found** for dual-core cache coherence, a
  32×32→64 multiply erratum, or watchdog/flash-controller anomalies. The vendor SDK wiki
  documents a watchdog armed first with reset-cause readback, an SFC clock that breaks
  flash reads when too fast, shared CRC16/FFT hardware needing mutexes, and SDRAM
  power-down bus errors. Everything else is unverified.
- `FM1_FLASH_CUT` is **documented** by the emulator project but is **absent from the
  0.4.0 binary available here** (not in `--help`, not in `strings`). See §5.
- The FM-1 is a **single-slot** device: 1 MiB NOR, boot area `[0, 0x4000)`, app
  `[0x4000, 0x93000)`. There is no second image slot and no ROM-verified signature.

## 2. Threat model (EMB3D IDs, this codebase)

| ID | Scenario | Cause | Component | Consequence | Protection today | Test needed | Viable mitigation | Residual |
|---|---|---|---|---|---|---|---|---|
| TID-211/217 | Any web page with SysEx can start an update | update trigger is a SysEx frame, no device-side confirmation | `ota.c` `usb.ota_req`, `felucca.c` parser | malicious or accidental install | the running firmware must answer and stage; the host must still confirm `success`; OCT−+OCT+ path is local | emulator/WebMIDI with a hostile page (blocked: no browser here) | require a physical confirmation before `ota_session()` | the host still decides; user must not run unknown pages |
| TID-213 | Faulty update-integrity check | CRC-16 only, no signature | `.fwsc` parser, `ota.c` | a crafted package installs | UFW/flash CRCs, loader CRCs, package loader identity (`FELUCCA-LOADER-1` / official CRCs), app-area-only writes | `test_pkg_faults.mjs`, `test_pkg_bounds.mjs`, `ota_test`, `ota_cut_test` | publish a signed digest (Ed25519) and verify it in the installer; device-side verification needs the bootloader | no cryptographic authenticity on the device |
| TID-216 | Rollback to an older firmware | no monotonic counter enforced at boot | bootloader (not modifiable here) | known-vulnerable image reinstalled | the installer checks the model and the manifest's `min_firmware`, and has no force flag any more (`--force` was removed); rollback at boot is still unprotected | not testable without a signed manifest | manifest sequence number checked by the installer/device app | rollback at boot is **unprotected** |
| TID-201/210 | Interrupted or bad firmware image | in-place write of the only copy | app area `[0x4000, 0x93000)` | brick until ROM download mode | the updater stages the loader first and commits only after `success`; the loader is the only writer of the app area | host staging cuts (this audit); **no emulator support for a cut inside the loader's write** | write the first sector last (ChoralRoot boot stub pattern), or a second slot | **unprotected** on a unit without a stub |
| TID-327/326 | Malformed package or SysEx | length/offset fields | `ota.c` `ota_ufw`, `ota_read`, `fm1pkg.js` | OOB read/write | bounded reads (`ota_read` checks `addr+len` against the package in the host; the device checks message lengths), JFrog-style header CRCs | `test_pkg_bounds/faults`, `fuzz_ed`, `ota_test` | keep the bounds checks and fuzz them in CI | parser memory safety only as good as the tests |
| TID-303 | Compromised installer/host tool | the host is trusted | `tools/fm1_install.py`, `web/fm1pkg.js` | wrong image written | SHA-256 sidecar check (`--sha256`, `SHA256SUMS`), model check, app-area-only guards | `install_test.py`; no signature check | sign the digest; keep the key offline | a malicious installer can still lie |
| TID-111/115 | Untrusted firmware file | user-supplied package | installer | wrong model / corrupt image | product marker, chip-key allowlist, CRC, size checks | `test_pkg_bounds`, `install_test` | — | user must verify the source |
| TID-208/306 | Emulator/WASM escape | wasm runtime bug | `web/emu` | host compromise | browser sandbox only | not in scope here | — | out of scope |
| TID-105 | Hardware fault injection (glitch) | no secure boot | SoC | signature bypass (if one is ever added) | none | requires hardware | — | unverifiable here |

Not applicable to this device: network update servers, TLS, OTA partitions, eFuse
anti-rollback, secure element.

## 3. Existing protections found in the code (audited, not assumed)

- **Persistent data is A/B with a commit record written last** (`storage.c`): 32-byte
  header (magic, type, slot, sequence, length, payload CRC-32, header CRC-32) at offset 0,
  payload from offset 256, read-back verification after the write. Loading takes the valid
  copy with the newest sequence. This is the Felucca pattern the mission asked about; it is
  applied to settings, the four project slots, user presets, the FM6 patch/voice banks,
  songs 2..8, the song index, the TAKES reel and the autosave.
- **OTA staging is commit-last** (`ota.c`): the running firmware pulls the package, checks
  the UFW header, decides between the official loader (known CRCs, verified to match the
  device head) and Felucca's own loader (marker + size), erases only
  `0xE0000..0xE4FFF`, programs the body, reads it back, writes the `LOADER.BIN` head last,
  and commits the `UPDATA_PARM` record only after the host answers `success`. Any failure
  unstages.
- **Boot loop guard** (`main.c`): the watchdog is armed as the first statement of
  `fm1_cstart` (`fm1_wdt_arm(0x0D)`); `bootguard` (magic, failed, pending in `.noinit`)
  counts boots that die within 30 s and enters UBOOT after two consecutive failures.
- **Crash record and screen**: `fm1_crash_t` keeps vector, PC, EPC, debug and return
  address; `fm1_fault` disables the second core, stops audio, shows them for 4 s and
  reboots. A crash newer than the last boot suppresses the LED sweep and the autosave
  restore.
- **Storage boundaries**: the flash driver accepts writes only inside the staging area
  (`ota_erase`/`ota_prog` reject anything else; the host test counts violations), and
  `ota_test` asserts nothing is written outside `0xE0000..0xE4FFF`.
- **CI fail-closed**: `.github/workflows/emulator.yml` requires `build/felucca.fwsc` and
  `build/felucca.dis` in an `if: always()` step before the site can be published, and the
  deploy job only runs on `main` (or two named branches), never on a pull request.
  `.github/workflows/c-cpp.yml` runs the host suite with `NO_PACKAGE=1` on PRs.

## 4. Worst-case matrix and results

Legend: **PASS** = executed here with the command shown; **BLOCKED** = the tool or the
environment cannot execute it; **UNPROTECTED** = no mechanism exists in this codebase.

| # | Scenario | Where | Result | Evidence |
|---|---|---|---|---|
| 1 | Package truncated, empty, oversized, bad header/entry/flash CRC, flash outside the file | host | **PASS** | `node web/test_pkg_bounds.mjs build/felucca.fwsc` → `package bounds: all checks passed` |
| 2 | Fault injection into a valid package (identity markers, payload bytes) | host | **PASS** | `node web/test_pkg_faults.mjs build/felucca.fwsc` → `package fault injection: all checks passed` |
| 3 | Installer: disconnects, lost/delayed ACKs, wrong model, `--verify-only`, bad packages, read past the package | host (simulated MIDI) | **PASS** | `python3 tests/install_test.py` → `install tests passed` (official V15 restore skipped: file absent) |
| 4 | Host disappears mid-update; host never confirms `success` | host | **PASS** | `tests/ota_test.c`: no commit, staging erased |
| 5 | **Power cut at every erase and every page program of the OTA staging**, half way through the operation, no cleanup code, then reboot | host | **PASS** | new `tests/ota_cut_test.c`: 23 cuts, `no commit, nothing launchable, next update works` |
| 6 | Power cut **while the boot cleanup is erasing** (a cut during recovery) | host | **PASS** | same test: 3 cleanup cuts, `nothing launchable, updates still work` |
| 7 | **Power cut at every erase/page program of every persistent object's save** | host | **PASS** | new `tests/storage_cut_test.c`: 40 objects, 160 cuts, old copy byte-identical, next save works, nothing outside the pair touched |
| 8 | Corrupted commit record / payload (bit rot) in the newest copy | host | **PASS** | `tests/storage_test.c` (older copy loads), `tests/fm6bank_test.c` (bank cuts) |
| 9 | Second core misreads RAM, with and without `--cpu1-survives` | emulated | **PASS (emulated)** | §5 table: 14 runs, all exit 0, 25–180 LCD frames, no crash screen |
| 10 | Emulated device boots a flash image with corrupt persisted data | emulated | **BLOCKED** | `--raw-flash` is read-only here (0 bytes changed in 2.5 s / 180 frames); `--flash` rejects every image produced from the package |
| 11 | Cut inside the **loader's** write of the app area | emulated/hardware | **BLOCKED** | no `FM1_FLASH_CUT` in emulator 0.4.0; the loader runs outside this firmware |
| 12 | Brownout, erase/program timing, stuck bits, worn sectors | hardware | **BLOCKED** | physical |
| 13 | Bootloader, SPL, ROM, recovery dongle, power-cut brick | hardware | **BLOCKED** | prohibited to modify; no unit |
| 14 | Malicious web page triggering an update over WebMIDI | browser | **BLOCKED** | no browser automation in this VM (the editor renderer also exits before capture) |
| 15 | Rollback to an older valid firmware | device | **UNPROTECTED** | no monotonic counter enforced before boot; `--force` was removed, so the host can only refuse what the manifest lets it check |
| 16 | Signature / authenticity of the package | device | **UNPROTECTED** | CRC-16 only; the chip-key scramble is obfuscation, not authentication |
| 17 | Bad image that does not boot (single slot) | device | **UNPROTECTED** | no A/B, no golden image; only ROM download mode / USB key dongle |

Cut counts executed in this audit: **160 persistent-object cuts + 23 staging cuts + 3
recovery cuts = 186**, plus the byte-level and bank cut loops already in
`tests/storage_test.c` and `tests/fm6bank_test.c`.

## 5. Emulator reality check (0.4.0, build 9de1ef053)

| Feature from the mission | Present? | What happened |
|---|---|---|
| `FM1_FLASH_CUT=N` | **No** | not in `--help`, not in `strings`; the variable is ignored |
| `--flaky-core1 RATE --seed N` | Yes | 12 runs, seeds 1/7/13, rates 0.001/0.005/0.01/0.05, plus 2 default runs |
| `--cpu1-survives` | Yes | accepted; every run exited 0 |
| `--flash FILE` | Yes, but rejects the images we can build | `error: ... does not contain build/felucca.fwsc at 0x4120` for a 1 MiB 0xFF image, the scrambled `pkg.flash`, the decoded `pkg.dec`, a `0x93000` image and the `.fwsc` itself |
| `--raw-flash CHIP.bin` | Yes (boots) | boots and runs 2500 ms / 180 LCD frames, but the file is **not written back**: `changed bytes: 0`, every storage sector still `0xFF` |
| Watchdog / reset modelling | documented by the project | not exercised here; no register-level test was written that could prove it |

`--flaky-core1` results (all exit 0, all with the LCD on and no crash screen):

| rate | seed 1 | seed 7 | seed 13 |
|---|---|---|---|
| 0.001 | 1 misread / 100 loads | 1 / 765 | 1 / 1344 |
| 0.005 | 1 / 100 | 1 / 742 | 1 / 323 |
| 0.01 | 1 / 100 | 1 / 174 | 1 / 323 |
| 0.05 | 1 / 27 | 1 / 3 | 1 / 19 |

The emulator reports one misread and stops injecting, so this table proves "the firmware
survives one bad read at these seeds", not "the fallback core path is proven".

## 6. What is implemented, detected and unprotected

- **Mitigated (in this firmware):** torn saves of every persistent object (A/B + commit
  record + read-back); a power cut at any point of the OTA staging or of its boot cleanup
  (nothing is committed and nothing launchable survives); malformed packages are refused;
  a boot loop of two failed starts enters UBOOT; a crash shows its registers and reboots;
  a crash still lets core 0 run (core 1 is barred for the session).
- **Detected only:** package corruption (CRC), wrong model (product marker), storage
  corruption (per-copy CRC → fall back to the other copy). Detection is not authenticity.
- **Unprotected:** a bad or interrupted **firmware image** (single slot, no A/B, no golden
  image); rollback at boot; authenticity of the package (no signature); recovery that does
  not depend on the same firmware (only ROM download mode / USB key dongle).
- **Not provable here:** physical flash behaviour, erase/program failures, brownout,
  bootloader and SPL behaviour, the loader's own write of the app area.

### 6.1 The order of the write, read in the code today

`firmware/loader/ldr_core.c` (`ldr_session()`) does, in this order:

1. **read the whole `flash.bin` before erasing anything** (`ldr_check`), keeping a CRC16 per app
   sector: a damaged or truncated package is refused with the flash untouched;
2. **refuse a package that does not decrypt with this unit's key** (`ldr_chip_key` + the app head's
   CRC), so a package for another chip key never erases;
3. for each sector of `[0x4000, 0x93000)` **from low to high**: read it, compare with the checked
   copy, skip it when equal, otherwise erase, program in 256-byte pieces, read back, and retry once;
   a sector that never verifies aborts the session;
4. only an exact `success` on `0xF0000000` clears the update record; anything else returns an error
   and leaves the record, so the next power-on runs the loader again and the host resumes.

The first sector written is therefore the app entry, and the last is the top of the app area.

**First-sector-last was analysed and not adopted.** Writing the entry sector last (the pattern a boot
stub uses) reads the entry last but leaves the *old* entry in place for the whole transfer: a cut then
leaves an old head over a half-new image, which is still unbootable and, worse, could be booted. The
variant that would actually help is *erase the entry sector first, write it last*, so that a cut leaves
a blank head the SPL refuses. That depends on how this SPL treats an app head whose CRC and marker are
blank — `0xFFFFFFFF` may be read as a valid empty entry — and that behaviour is not documented for the
AC791N and cannot be measured here. Changing the write order on an assumption would trade a known
failure mode (record-gated resume, which is tested in `tests/ldr_test.c`) for an unknown one, so the
order stays as it is until a laboratory unit can show what the SPL does with a blank head.

What would give independent recovery, in order of cost: (a) a stub in the first sector that survives
a failed app write and re-enters the loader (needs the SPL's load rules); (b) a golden image in the
staging window and a flag the SPL acts on (needs the same); (c) the vendor's ROM download mode, which
exists but is not this project's code and is not demonstrated here.

## 7. Budgets

The JieLi toolchain and the AC79 SDK are not installed in this VM, so the target package
was **not rebuilt** and the memory/DSP budgets were **not re-measured**. The package used
here is `580d4275243491c8bbb2003c457da1d49422d98927b8c03118e46fed3689b936` (610,897 B),
which predates the EQ/UI source changes; the previously recorded numbers
(`.data+.bss 95732 B`, RAM spare 2572 B, pool spare 9248 B, RAMTEXT spare 2112 B, NOINIT
spare 124 B) therefore describe an older tree and are **stale**, not evidence for this
one. No new RAM or flash was consumed by this audit: the only additions are two host
tests and a shell line.

## 8. Changes made

| File | Change |
|---|---|
| `tests/storage_cut_test.c` | **new**: power-cut matrix over every persistent object's A/B save |
| `tests/ota_cut_test.c` | **new**: power-cut matrix over the OTA staging and its boot cleanup |
| `tests/groups.sh` | `security` now builds and runs both cut tests (the OTA one only when `build/felucca.fwsc` exists) |
| `docs/BRICK_RESILIENCE_FM1.md` | this report |

No firmware, bootloader, flash map, package format or user-data format was changed.

## 9. Reproduction

```sh
cc -O1 -w -o build/uifix/storage_cut_test tests/storage_cut_test.c && ./build/uifix/storage_cut_test
cc -O1 -w -o build/uifix/ota_cut_test tests/ota_cut_test.c && ./build/uifix/ota_cut_test build/felucca.fwsc
python3 tests/install_test.py
node web/test_pkg_bounds.mjs build/felucca.fwsc
node web/test_pkg_faults.mjs build/felucca.fwsc
bash tests/groups.sh security          # ASan/UBSan, fuzz, EQ and the two cut matrices

# emulator (single core-1 injection)
/tmp/fm1emu/fm1-emu-0.4.0-linux-x86_64/fm1-emu --fwsc build/felucca.fwsc --ms 300 \
  --flaky-core1 0.01 --seed 7 --cpu1-survives
```

Hashes of this audit's artifacts:

```
6a55979aad2c7255f92746cd0b21ef09142522b226baa7ddc0a4e59e9c40598f  tests/storage_cut_test.c
68a3a6a76877a108f956c01f5b8699bcb7df343443ce5e3df3442f83b13144e2  tests/ota_cut_test.c
32341b3b149e8a060c630be04f05cc9b2c0a33b13ee8d69ec386eca757394a27  build/uifix/storage_cut_test
a4dadad60849f19d43a8d0357411c3c29310c6055d67eceb2c50047c82fd51e7  build/uifix/ota_cut_test
2a2734592cd6c50afa0a48c4de4be89450eae7774dd683e67fb6da5d208f1342  build/storage_cut.log
83193ccf4678e9fe0e9a091e68125d56be29768b4ea005242830ddd097c06143  build/ota_cut.log
334fc77a4fc9133b69a266adc653fe9750e9d2d2030d3b8cb1d6cb03cdef52f2  build/core1_matrix.log
```

## 10. Verdict

- **Mitigated and proven on the host:** persistent-data torn writes (all 40 objects) and
  every erase/page program of the OTA staging and of its recovery cleanup.
- **Detected only:** package corruption/metadata errors, storage corruption, wrong model.
- **Unprotected:** an interrupted or bad **firmware image** (single slot, no A/B, no
  recovery independent of the same firmware), rollback at boot, package authenticity
  (CRC-16 only, no signature), and any attacker who can write flash directly.
- **Not verifiable without hardware:** physical power cuts, flash erase/program failures,
  brownout, bootloader/SPL behaviour, the loader's write of the app area, the USB-key
  recovery path.
- **Not claimed:** this firmware is **not** "anti-brick", has **no secure boot** and **no
  independent physical recovery**. What it has is commit-last staging, A/B user data,
  read-back verification, a two-strike boot guard and a crash record.
