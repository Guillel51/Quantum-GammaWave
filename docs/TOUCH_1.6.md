# Velocity and aftertouch — QG-1 v1.6

## What the hardware and MIDI actually offer

| Source | Availability | Path |
|---|---|---|
| Key velocity | 27 micro-switches, on/off with a scan time; no analogue velocity | note-on velocity as the firmware has always produced it |
| Channel aftertouch | MIDI `0xD0` from an external controller | track `at` → matrix source **AT** (`MS_AT`) |
| Polyphonic aftertouch | MIDI `0xA0` from an external controller | the matching held voice's `pat` → matrix source **PAT** (`MS_PAT`, new) |
| Per-key pressure from the FM-1's own keys | **not available** (no sensor) | never simulated |

`AT` is per track (the whole channel's pressure). `PAT` is per voice: it only reaches the keys the
controller names, and a new note starts at 0. Both are unipolar 0..127.

## Assigning them

The modulation matrix (EDIT > MOD, four slots) is the routing surface:

- Source: `AT` for channel pressure, `PAT` for per-key pressure, `VEL` for velocity.
- Destination: `CUT` (filter), `SHP` (timbre/FM brightness), `AMP` (volume), `PITCH`, `VIB` (vibrato),
  `E1..E8` (the engine's EDIT parameters: a formant's vowel, FM6's algorithm/macros, ANALOG's wave…),
  `CHO` / `DLY` / `REV` (FX sends), `RATE` (LFO).
- AMT is the sensitivity: 0 leaves the destination alone, ±63 is the full range (bipolar where the
  destination is).

There is deliberately **no new stored curve/sensitivity parameter**: `P_COUNT` is frozen by the FUN1–FUNC
project layout (`project.c`'s `PROJ_PFX_OFF` assertion), and growing it would break every stored project
and user preset. The curve is the engine's own velocity response plus the matrix amount; the presets below
show the practical range.

## Demonstration sounds

| Sound | Engine | Routes |
|---|---|---|
| PRISM KEYS | ANALOG | VEL→CUT +46, AT→CUT +30 |
| GLASS BELL | ANALOG | VEL→CUT +50, PAT→VIB +34 |
| AURORA PLUCK | ANALOG | VEL→CUT +52, AT→REV +26 |
| BREATH CHOIR | FORMANT | AT→E1 (vowel) +22 |
| VELVET EP | FM6 | VEL→CUT +40, AT→VIB +24 |
| EMBER LEAD | ANALOG | VEL→CUT +34, AT→VIB +18 |

## Testing

- `tests/regress.c` renders every sound and checks clipping, DC, silence and voices-free.
- MIDI parsing is exercised by the editor/fuzz tests; `0xA0` is a three-byte message like `0x90`, and the
  new branch is beside the existing `0xD0` one (`midi_control.c`).
- **NO VERIFICADO on hardware:** no FM-1 was available. An external controller sending `0xA0` is the only
  way to exercise `PAT`; the device's keys cannot.
