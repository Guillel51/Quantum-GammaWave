# Quantum GammaWave v1.9.0

Firmware for the **M-VAVE FM-1** (JieLi AC791N / WL82), with its browser emulator, Web Studio and installer.

## What this release is

- `felucca-v1.9.0.fwsc` — the whole QG package for the FM-1: identity `FM-1_982`, version 1.9.0,
  minimum firmware 1.0.0.
- `felucca-v1.9.0.manifest.json` — the Ed25519 signature over the package and its build record, made with
  `qg1-release-1` (`70b6036fd33da3297a41da2141c5d135b3efce2d815d55911fc4f9b1d07fcd40`, pinned in
  `tools/fm1_keys.json`).
- `build-manifest.json` — the record of the build the package came from: commit, version, identity, hashes,
  emulator, `dirty: []`.
- `SHA256SUMS` — the hashes of the three files above.

The release workflow re-verifies the signature, the hashes and the identity before publishing, and downloads
the published assets again to check them a second time.

## Install status, stated exactly

**The software path is enabled and enforced; the first physical write is conditional on readings from your
own unit.** The installer identifies the NOR through the device's `0x12` query, refuses any part outside
`0x856014` / `0xB36014`, refuses a geometry that does not match the part it identified, requires the signed
release with the pinned key and a clean provenance record, and requires the unit to report the QG product
after the write.

What it cannot do, and does not pretend to: take a backup of a *stock* unit before the first write (stock
firmware has no read command), and guarantee that a power cut during the app erase leaves the unit booting.
The FM-1 is single-bank; the boot head is never written by QG's loader, and recovery of an interrupted write
needs the chip's ROM UBOOT and an external full-dump tool. Take a 1 MiB dump of that unit first, and rehearse
the recovery route once.

Details, per requirement: [`docs/FORENSIC_FM1_VIABILITY.md`](../docs/FORENSIC_FM1_VIABILITY.md).

## What changed in 1.9.0

- The AC791N RAM code fits its budget again with both fixes in place: the delay's echo train is stepped from
  flash (`fx_vu` once per block) and `perf_begin` left `.hot_text`. RAM code 21 768 B of 24 576, headroom
  2 808 B, `.ram_text` with no calls.
- The target build links against the pinned `AC79NN_SDK_V1.2.1_2023-12-13`: the three files in `SDK_SHA256`
  match, and two builds produce the same bytes.
- Production signing: one key pinned, fail-closed, with rotation and revocation covered by tests; a package
  signed by another key under the same id is refused, and so is an id that is not pinned.
- Installer: `Verify Only` reports what it read and what it could not, `canInstall` stays false there, and the
  page can now export the whole report as `qg1-diagnostic-1` JSON — no keys, no tokens, no secrets.
- Eight named installation states (`DIAGNOSTIC READY` … `INSTALL READY`, `INSTALL BLOCKED`) derived from the
  gate rows, so a lower state can never be shown as a higher one.
- The emulator is built with Emscripten **6.0.11**, the version the workflows pin, from the same commit as
  the firmware.

## Verifying what you downloaded

```bash
sha256sum -c SHA256SUMS
python3 tools/fm1_sign.py verify felucca-v1.9.0.fwsc \
    --manifest felucca-v1.9.0.manifest.json --build-manifest build-manifest.json
# verified: FM-1 version 1.9.0, key qg1-release-1, sha256 …
```

A valid signature proves who built these bytes. It does not prove the flash chip in your unit, and it does
not make a power cut harmless.
