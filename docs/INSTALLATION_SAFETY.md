# INSTALLATION SAFETY — Quantum GammaWave (QG-1)

The installer's job is to refuse. It should only write when every condition below is true, and it
must say *which* condition failed. This page states the conditions, the state machine that reports
them, and exactly what is missing before the main unit may be touched.

**Current verdict: NO-GO for the main unit.** Software checks pass in this tree; physical identity,
backup, restore and recovery have not been demonstrated on any FM-1 here.

---

## 1. Pre-flight conditions (all mandatory)

| # | Condition | Checked by | Today |
|---|-----------|-----------|-------|
| 1 | The device answers the handshake and reports a `FM-1_*` identity | `handshake()` / `parseIdentity()` | yes |
| 2 | The package's model matches the device's model | `install()` model check | yes |
| 3 | The package's SHA-256 equals the published one | CLI `--sha256` / `.sha256` sidecar; web page `crypto.subtle` | yes |
| 4 | The package carries Felucca's own loader marker (or is the unmodified official V15, pinned by hash) | `LOADER_MARK`, `STOCK_V15_SHA256` | yes |
| 5 | The identity the device reports **after** the reboot equals the package's identity | `verify()` | yes |
| 6 | The device's flash is identified: documented part, two agreeing reads, 1 MiB, unprotected | firmware `flash_identify()` (this change) | firmware yes; **installer cannot see it** — open |
| 7 | A valid, verified complete backup exists before writing | installer backup step | web installer yes; CLI no (by design: it does not own your archive) — open |
| 8 | The package is signed by the project's production key | — | **missing** (no key material in this tree) |

Conditions 6–8 are why production stays blocked. 1–5 and 7 (web) are implemented.

## 2. Reported states

The installer distinguishes, and never paints a security state it did not reach:

```
searching → detected → identified → (backup) → package → hash → model → ready
        → installing → verifying → installed | error | cancelled
```

* **detected / identified** — a port answered; the identity is the device's own bytes.
* **hash / model / package** — the bytes are the published ones *and* the package is for this model.
* **ready** — every check above that this build implements passed. If a check is unavailable, say
  so; never show green.
* **verifying** — only after the device reports the new identity. A reboot that did not happen is
  `noreturn`, not success.
* **error** — carries the reason: `lost`, `stopped`, `noloader`, `noreturn`, `mismatch`, `badreq`.

## 3. What the write path does

1. The running firmware pulls the package in `0x30` read requests, verifies the UFW header and the
   flash.bin CRC, stages the loader at `0xE0000`, reads it back and waits for the exact `success`
   ACK. Nothing is committed before that ACK; any failure erases the staging area.
2. The loader reads the package **twice over**: the whole `flash.bin` CRC before the first erase,
   and every 4 KiB app sector again before writing it. A sector that differs on the second read is
   re-read, never written.
3. It writes only `[0x4000, 0x93000)`. The head (SPL, `isd_config`), the VM below `0xE0000`, the
   autosave, the FM6 voice bank and `USR` are excluded by a static range check that also fails the
   build if the window is wrong (`_Static_assert` in `firmware/loader/loader.c`).
4. Each sector is read back and compared; a sector that fails verification twice aborts the update
   with the record intact, so the next power-on resumes.

## 4. Laboratory procedure (secondary unit only)

Never on the main unit. Record every step; keep the log.

1. Confirm the unit and the port (`python3 tools/fm1_install.py --info`).
2. Take a verified archive: `python3 tools/fm1_archive.py backup` (see
   [`BACKUP_RESTORE.md`](BACKUP_RESTORE.md)). Keep the file off this repository.
3. `--verify-only` with the package; it must print the identity and write nothing.
4. Install with the same hash the release publishes.
5. Confirm the post-reboot identity, then a power cycle, then audio, controls, presets, memory and
   (`MENU > ABOUT`) whether the second core is up or was held.
6. Restore the archive and verify it reads back.
7. Only then discuss the main unit. The owner decides; no test here substitutes for that.

## 5. Cancelling

Cancelling before the loader is confirmed writes nothing. Cancelling after the loader appears leaves
the device in update mode; re-running the install (or `--verify-only` first) finishes it. The
documented recovery ladder is in [`RECOVERY_GUIDE.md`](RECOVERY_GUIDE.md).
