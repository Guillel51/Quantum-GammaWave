# VALIDATION MATRIX — what was actually run (v1.8.7 pass)

Honesty first: this table records the checks executed **in this session**, on this machine, against
this tree. Anything not listed here was not run. Labels are defined in
[`SECURITY_AUDIT.md`](SECURITY_AUDIT.md).

Environment: Linux x86-64, `cc` (gcc), Python 3.12, Node available. **No** JieLi pi32v2 toolchain,
**no** AC79 SDK, **no** `emcc`, **no** physical FM-1, **no** MIDI hardware.

## 1. Checks run

| Check | Command | Label | Result |
|-------|---------|-------|--------|
| Firmware translation unit compiles (`felucca.c` → everything it includes) | `cc -fsyntax-only -Ibuild/gen -Ifirmware/src -Ifirmware/hal -Ifirmware firmware/src/felucca.c` | `[STATIC]` | pass |
| Flash identification policy (11 cases: allowlist, bus, disagreement, capacity, protection) | `cc -o build/host/flash_id_test tests/flash_id_test.c && ./build/host/flash_id_test` | `[HOST]` | pass |
| Second-core boot self-test (deterministic payload, every single-word corruption refused) | `cc -o build/host/mc_selftest_test tests/mc_selftest_test.c && ./build/host/mc_selftest_test` | `[HOST]` | pass |
| OTA core compiles with the new range/flag checks | `cc -Wall -DOWN_PKG=1 -o build/host/ota_test tests/ota_test.c` | `[HOST]` | compiles |
| Update loader compiles with the session deadline | `cc -Wall -o build/host/ldr_test tests/ldr_test.c` | `[HOST]` | compiles |
| Backup CLI: two passes, classification, restore, readback, altered archive refused, model mismatch, private destination | `python3 tests/backup_tool_test.py` | `[HOST]` | pass (24 checks) |
| Installer CLI + the new output-pairing rule | `python3 tests/install_test.py` | `[HOST]` | pass |
| README navigation contract (names, order, double-width WEB SITE, links, palette) | `python3 tests/readme_nav_test.py` | `[STATIC]` | pass |
| Storage / user-preset host tests | `build/host/storage_test`, `build/host/upreset_test` | `[HOST]` | pass |

## 2. Checks that could not run here, and why

| Check | Why not | Status |
|-------|---------|--------|
| Target build (`./build.sh`, `.fwsc`, memory map, `target_budget.py`) | JieLi toolchain and AC79 SDK are not installed; `AC79_SDK` unset | `[TARGET-BUILD]` **not run** |
| `tests/ota_test.c`, `tests/ldr_test.c` **executed** | both need a built `.fwsc`; only compiled here | `[HOST]` **partial** |
| WASM rebuild and `tests/emu_sync.py` | `emcc` not installed | `[WASM]` **not run** |
| Browser checks (portal, Web Studio, installer) | no browser automation in this workspace | `[BROWSER]` **not run** |
| `tests/persistence_test.c`, `tests/backup_test.c`, `tests/fm6bank_test.c` on this host | they fail to compile at **HEAD too** (`csync`/`ssync` inline asm in `fm1_usb.h` has no x86 meaning). Pre-existing, unrelated to this change; the firmware TU check above covers the code they exercise | pre-existing limitation |
| Everything physical | no FM-1 | `[HARDWARE]` `[RECOVERY-HW]` **not verified** |

## 3. Failure-injection coverage added

| Injection | Where | Expected behaviour | Verified |
|-----------|-------|--------------------|----------|
| JEDEC reads disagree / bus floats / bus shorts | `flash_id_test` | no writes; firmware RAM-only | `[HOST]` yes |
| Flash block-protected / busy | `flash_id_test` | no writes | `[HOST]` yes |
| Shared mailbox word corrupted (`0x00200000`), job number changed, job unanswered | `mc_selftest_test` | core held; one core continues | `[HOST]` yes |
| Device content changes between two backup passes | `backup_tool_test` | `inconsistent`, nothing written | `[HOST]` yes |
| Cable pulled mid-read | `backup_tool_test` | `lost`, no partial archive saved | `[HOST]` yes |
| Archive altered after writing (data byte, CRC, file hash, manifest removed) | `backup_tool_test` | refused before the first write | `[HOST]` yes |
| Restore to a device in update mode / another model / missing object | `backup_tool_test` | refused before any write | `[HOST]` yes |
| Package names an offset outside 1 MiB | `ota.c` code path | refused before erase | code + compile only (`[STATIC]`) |
| Host goes quiet during the write | `ldr_core.c` deadline | write abandoned, record kept | `[STATIC]` only |
| Personal archive destination inside the repo | `backup_tool_test` | refused before the device is opened | `[HOST]` yes |

## 4. Not claimed

* That any of this works on an FM-1. It has not been run on one.
* That the update path is safe against a power cut. The record and the resume logic are read and
  tested in software (`tests/ota_cut_test.c` at HEAD), but a real cut has not been made.
* That the package is authentic. There is no signature (`docs/SECURITY_AUDIT.md` A).
* That the Web Emulator contains this change: the WASM was not rebuilt.

## 5. Verdict

**NO-GO for the main unit.** Software-verified only.
**GO for laboratory validation on a secondary unit** once a target build exists and the owner
provides a laboratory FM-1.
