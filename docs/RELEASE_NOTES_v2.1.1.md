# Quantum GammaWave v2.1.1

The site serves this build: the emulator at `/emulator/` and the package the installer offers are compiled
from the same commit, and the emulator's footer prints its version, commit, build date, WASM SHA-256 and size.
The installer checks the published SHA-256 and size before it writes, and the update loader reads the flash
chip before it erases anything. Every version is built by CI from its tag (`release.yml`,
`docs/RELEASE.es.md`), and this release carries `felucca-2.1.1.fwsc`, the app `.bin` and `SHA256SUMS`.

## What changed

- **HOME's planets turn, and it was checked in the browser.** Two captures of the panel 2.6 s apart: 868 of
  57 600 pixels change. In a 300 ms window 576 pixels change — more than an orbit of 7–13 s per turn can
  move — so what moves is each body's surface, not just its orbit.
  Evidence: `docs/evidence/3.3/` (`home-rotation-1.png`, `home-rotation-2.png`, the cropped pair and the
  difference image).
- **SCL keys keep a 3 px corner**, not the 6 px lozenge of 3.2; `tests/ui_render` re-rendered all 174 screens
  in 17 palettes with 0 lint and 0 alignment findings.
- **LATCH measured on rendered audio**, not on a flag: `tests/perform_test.c::test_latch_audio()` releases the
  key and checks that the filter still filters with LATCH on, that the second touch returns the dry signal bit
  for bit, that LATCH off returns dry, and that REPEAT 1/16 keeps sounding.
- **The dead `/preview/` deployment check is gone.** It waited on a channel that no longer exists, so every
  deploy would have gone red after publishing.

## What this release does not claim

No install has been run on a physical FM-1 and no recovery route has been rehearsed on one. The
control-by-control DSP audit (control → parameter → DSP destination → audible effect) and the low-cost musical
improvements are still open and are listed as such in `docs/VALIDATION_MATRIX.md` §36. Bench numbers are host
numbers.

## The signed channel

`/release/` and the Ed25519 signing scheme were removed in #89. Their last defect is recorded rather than
fixed, because the code it lived in is gone: the signed build record named an emulator of 1 073 325 B and
`3d039f75…`, and the same commit rebuilt with the pinned Emscripten 6.0.11 produced 1 073 325 B and
`c83c8bfb…` — the same size and different bytes — so a page that declared the served emulator *was* that
release refused the very package the record signed. With one channel and hash/size checks, no page claims a
provenance it cannot prove.
