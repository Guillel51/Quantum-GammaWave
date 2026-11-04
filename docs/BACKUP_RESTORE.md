# BACKUP, RESTORE AND RECOVERY OF USER DATA

Two different things are called "backup" here, and confusing them is how people lose work:

1. **The musical archive** — the device's own objects (current music, settings, projects, the song
   index, the song sections). This is what `web/fm1backup.js` and the new `tools/fm1_backup.py`
   move. It is complete for *user data*.
2. **A raw flash image** — every byte of the 1 MiB part, including the app, the head and the
   bootloader. **This protocol cannot produce it.** A raw image needs the recovery path in
   [`RECOVERY_GUIDE.md`](RECOVERY_GUIDE.md).

Restoring an archive does **not** restore firmware. Restoring *only some objects* is not a recovery.

---

## 1. `tools/fm1_backup.py`

```
python3 tools/fm1_backup.py info                  # objects the connected FM-1 exposes
python3 tools/fm1_backup.py backup                # two independent reads -> verified archive
python3 tools/fm1_backup.py verify FILE.json      # manifest, file hash, every object hash and CRC
python3 tools/fm1_backup.py restore FILE.json     # full restore, read back and compared
python3 tools/fm1_backup.py restore FILE.json --only 3   # selective: reported as partial
```

Exit codes: 0 ok, 1 cancelled, 2 bad arguments/archive, 3 not found, 4 lost, 5 inconsistent or
unverifiable, 6 device or geometry mismatch.

### What "verified" means here

| Step | Mechanism |
|------|-----------|
| Two independent passes | A full `LIST` + `GET` of every object twice, compared **byte for byte** |
| Content cannot move under the reader | Every object's CRC32 is checked against the device's own manifest, and both passes must agree; a mismatch is refused, nothing is written |
| Per-read digests | `pass_sha256[0]` / `pass_sha256[1]` over id, size, crc and object SHA-256 |
| Per-object digests | `sha256` for each object, next to the device's `crc` |
| File digest | `file_sha256` over the format, device identity and every object's id/size/crc/sha256 |
| Manifest | tool and version, method, device identity/model/version/port, date, capacity (objects, bytes, ids), passes, result |
| Classification | `complete` (every object read, both passes agree) / `partial` (a required object missing) / `inconsistent` (passes disagree) / `unverifiable` (no manifest, or the file hash does not match) |

Archives are written `0600` outside the repository by default
(`~/.local/share/qg1-backups`). A destination inside a git work tree is refused **before the device
is opened**, because a backup is the user's own music.

The file keeps the `felucca-backup` / version 1 layout of the web page (so the Web Studio can open
it) and adds a `qg1` manifest block that the web page ignores.

### What the manifest honestly states

```json
"no_raw_flash": "musical objects only: firmware, bootloader, ROM and the flash head are not readable here",
"hardware_test": "none: no physical FM-1 was used to produce this file"
```

If those two lines are ever untrue for a file, that file is not from this tool.

## 2. Restore

Order of checks, before the first write:

1. The file parses, its format/version are ours, and its `qg1` manifest exists (else `unverifiable`).
2. `file_sha256` matches the archive (else `badpkg`).
3. Every object's base64 length, CRC32 and SHA-256 match (else `badpkg`).
4. The device is not in update mode; its model equals the archive's model (else `mismatch`).
5. Every target object exists in the device's own manifest and fits its geometry.
6. `--force` is required to write an archive classified as anything other than `complete`.

Then, per object: `PUT begin(size, crc)` → chunks → `PUT end`, followed by a `GET` of the whole
object that must equal the bytes just written. An object that does not read back aborts the restore
with a named object. Restoring everything is reported as `complete`; `--only` is reported as
`partial (selective)` with a note that it is not a full recovery — because it is not.

## 3. Limits

* A disconnect mid-restore leaves the objects already committed in place. They were each validated
  before the write and read back after it, so what is there is the archive's content, not garbage —
  but the set is incomplete. Re-run the restore when the cable is back.
* Objects the running firmware does not know (an older firmware, a newer archive) are skipped only
  for ids ≥ 9 in the web tool; the CLI refuses instead, and says which ids.
* Nothing here protects against a device whose flash is failing at the hardware level. The firmware
  refuses to write at all when its part cannot be identified (`docs/SECURITY_AUDIT.md` #1).
