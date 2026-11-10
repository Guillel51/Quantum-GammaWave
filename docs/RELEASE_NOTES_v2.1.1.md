# Quantum GammaWave v2.1.1

**This release is not the installable channel.** It carries the WebAssembly emulator built from this
tag's commit and the build record that describes it. The package a CI run builds carries no Ed25519
signature — the private key lives off the repository — so the installer publishes it and refuses to write
it. That is fail-closed by design.

**Installing a package means the signed release, `v1.9.0`**, at
<https://guillel51.github.io/Quantum-GammaWave/release/>: that channel serves the signed package with its
manifest, its record and the anchored key `qg1-release-1`. A signed `v2.1.1` needs the custodian's key.

## What changed in 2.1.1

`/release/` was refusing its own package. Its installer was told the emulator served beside it *was* the
release; `fm1provenance.js` then compared that emulator's WASM hash with the one the signed build record
names and concluded "the site mixes two builds". The record names 1 073 325 B of `3d039f75…`; the same
commit rebuilt with the pinned Emscripten 6.0.11 and the same flags gives 1 073 325 B of `c83c8bfb…` — the
same size and a different hash, because that build is not reproducible byte for byte.

The channel's installer is now written without `--emu`: it holds the package to its signed record and to
the anchored key, and claims nothing about a rebuilt emulator. The emulator still lives there, built from
the same commit, with its own hash in the footer.

Also in this release:

- `tools/site_check.py --no-emulator`, so the release channel's gate checks its commit, its package bytes
  and its pages instead of a pairing that channel does not make.
- The dead `/preview/` step in `emulator.yml` is gone; it would have failed every deploy after publishing.
- A per-version GitHub Release job: tag, wasm, manifest and `SHA256SUMS`, published once per version.

Carried from 2.0: the SCL keys' 3 px corner, HOME's planets spinning on their own axis with a note
lighting them, the Web Studio's view selector, and the two-channel site (this tree at `/`, the signed
release at `/release/`).

## Assets

| File | What it is |
|---|---|
| `felucca.wasm` | The firmware compiled by Emscripten 6.0.11 at this tag's commit, unmodified. |
| `manifest.json` | The emulator's own record: commit, version, Emscripten version, size, SHA-256. |
| `SHA256SUMS` | The two files above. |

Read the record first; it is the only thing here that says what the wasm is.

## What this release does not claim

No install has been run on a physical FM-1. No recovery route has been rehearsed on one. The DSP audit
(every control measured at the output) and the music-side improvements are still open and listed as such in
`docs/VALIDATION_MATRIX.md` §36. Bench numbers are host numbers.
