#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Felucca sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
# Without a package build (CI: generated headers only), NO_PACKAGE=1 runs everything that does not
# need build/felucca.fwsc / build/loader/ota.bin and skips those checks:
#   NO_PACKAGE=1 tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the GM map on DRUM, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), GREY gray, MONO neutral, the draw cost, the text audit (build/ui_new/text_audit.tsv: every
#                   screen, and every value the page / column sweep draws -- the LFO's divisions, DIV's names -- with its
#                   ink box, its ellipsis and its tight cell, in GREY and again under LARGE);
#                   PNGs of GREY MONO GREEN PAPER NIGHT in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another);
#                   DIGITAL's screens (its algorithm charts, OP ENV) with FELUCCA_FM4=1 too (build/ui_fm4: lint, GREY, MONO);
#                   every frame of the rolling digits (lint, GREY, MONO), their filmstrips in build/ui_slot;
#                   the alignment: every text / icon / keycap meant to be centred, or on its neighbours' line, by its
#                   ink against its box (cells, chips, buttons, rows, knobs, the roll's strip, the keycaps' pills), every
#                   screen in FLAT and LINE and every palette, over every value it can show; 1 px off or more fails
#                   (build/ui_new/align.txt). MENU > LARGE: every screen again (FLAT, LINE, every palette) and the page / value
#                   sweep with the tall cards, the same lint and alignment; sheet_LARGE_GREY.png, sheet_LARGE_MONO.png.
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL / SLICER, the SEQ > PATTERNS loader and its REPLACE? dialog, the
#                   one-step undo of both (SAVE held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1,
#                   the DRUM grid (keys, knobs, LEDs, pages, live recording into it, BEAT from PATTERNS).
# DSP: the polyBLEP residual's one-compare fast path (tests/blep_test.c) against the reference formulation
#                   bit for bit: both window edges, the sub-1 Hz incs whose d is 0, the increments past Nyquist
#                   (where the windows overlap) and 10 000 000 random pairs, so an "almost the same" residual
#                   cannot reach the oscillators unseen.
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback, the autosave
#                   (1.2: when it writes, the power-on restore, damaged copies, a write cut short, wear over a session),
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery; the MENU settings over the
#                   editor (MENU_DESC / MENU_SET: every item, clamping, unknown ids, saving, USB SERIAL applied later).
# RATCH (tests/ratchet_test.c): a step's ratchet (x1..x4): its parts in the sequencer (equal, gated, chords and drum
#                   hits whole, one chance roll, swing, no slide or tie out, STOP), FUN8 round trip and older projects x1,
#                   user preset patterns, SEQ > CHANCE KNOB 3 and the roll / grid drawing.
# FASE 2 (tests/phase2_test.c): the engine CHARACTER bank, LFO SHAPE, ENV VEL, their pages and a project round trip.
# PATTERN LEN (tests/pattern_len_test.c): LEN is per track (src/seq.c P_SLEN): a 4-step and a 6-step pattern wrap
#                   on their own step 0 in the same 60 steps, a project keeps 5 / 7 / 12 / 64 per track, and a track
#                   that never set it loads 16.
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (at most +5 %), demos in build/mod_demo/.
# PERFORM (tests/perform_test.c): the FX hold layer (src/perform.c): 1/16 starts, stereo buffer effects, the
#                   too-long REPEAT, the SLICER interplay, silent layer keys, idle bit-identical, cost; build/perform_demo/;
# DELAY (tests/dly_echo_test.c): the echo train the FX > DLY page draws (src/fx.c fx_dly_rep): silence shows
#                   nothing, a hit lights repeat 1 one TIME later, FEEDBK 0 leaves one pass, FEEDBK 60 falls by
#                   about FEEDBK, MIX 0 nothing, the bars follow the peaks linearly (not a level), PING-PONG too,
#                   and the train ends (no bar left lit after it has died).
# REVERB (tests/reverb_test.c): REVERB TYPE (src/fx.c): ROOM bit for bit as before, SPRING's decay against SIZE,
#                   its chirp (group delay rising with frequency), stability at the corners, level, a model change
#                   without a click, its cost against ROOM (+30 % at most); demos in build/fx_demo/.
#                   sine (builds without the CC0 samples); demos in build/slice_demo/.
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents; the LED scan: lit LEDs every
#                   frame, dim ones a short pulse (the second line write) every frame, each only on its own column;
#                   the breath (#119): dark .. ~60 % of lit (DIM LO ~30 %), smooth, no dark run over ~10 ms near its peak;
#                   the power-on sweep (1.1, hal/fm1_led_anim.h): its length, every LED every frame at its level, the
#                   head left to right with its tail, the buttons, the end on the idle glow, the tick as before after.
# CLICK (tests/click_test.c, 1.1): the metronome (src/click.c) and the count-in (src/seq.c) through audio.c: each beat at
#                   the first sample of its block, with a 1/16 track's steps 1 5 9 13 (internal and external clock, DIV 1/8
#                   and SWING too), the accent, the frequencies, length, LEVEL, MASTER, OFF / REC / ON, USB audio bit for bit
#                   without it; COUNT-IN 1 / 2 BARS (step 1 exactly N bars after PLAY, STOP cancels, PLAY again nothing,
#                   never with an external clock), notes in its last eighth onto step 1; build/click_demo/.
# USB audio (tests/uac_test.c): the UAC1 descriptors as a host parses them (with and without CDC; 44100 and
#                   48000 Hz), the ring and packetiser: 44.1 frames per packet, every frame in order (bit for bit),
#                   underrun / overrun, restart; SET_CUR; the 44.1 -> 48 kHz resampler (every frame against a
#                   double-precision one, response, SNR, cost); the stream at 48 kHz (47..49 frames per packet,
#                   fast and slow I2S clocks, switching rates mid-stream).
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json; the MENU settings: tests/editor_test.c -> build/host/menu.json),
#                   the package builder, the updater.
# Browser emulator (web/emu, when emcc is there): the firmware in WebAssembly (build/emu) boots, plays keys and MIDI,
#                   draws, lights its LEDs, keeps a save across instances, plays a song bit for bit as the same file
#                   built with cc (web/emu/native_check.c); the cost of 1 s of a heavy song against real time.
# TONE CACHE (tests/tone_cache_test.c, FASE 5): the delay's TONE and the reverb's FILT keep their coefficients for
#                   the value they were built for (they were rebuilt on every block); a hit is byte-identical to a
#                   first build, a knob turn reaches the DSP, and TONE 0 takes the delay's tone out of the chain.
# BOOT FADE (tests/boot_fade_test.c, FASE 3): the power-on fade as the master gain sees it -- 0 is exact silence
#                   (what the muted output stage plays and what its unmute lands on), the ramp is monotonic and
#                   reaches the same bytes as an unfaded render.
# DIST (tests/dist_models_test.c, FASE 2): the five models of the insert -- SOFT, TAPE, HEARTH, FLUX and the new
#                   FOLD (a triangle wavefolder): each sounds at DRIVE 90, stays inside 16 bits, FOLD's peak matches
#                   SOFT's (loudness-neutral) and carries no DC, the five renders differ, and DRIVE 0 takes TYPE out
#                   of the chain.
# DRUM-X (tests/drumx_test.c): the DRUM engine (src/drumx_voice.c): every lane sounds, stays under full scale and ends,
#                   MORPH A <-> B (also while a hit rings), PITCH, the hat choke, MRPH on the knob; demos in build/drumx_demo/.
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
# ADDTV (tests/additive_test.c, 1.2.6): the additive engine (src/eng_additive.c): the eight partials, the
#                   profiles and the morph between them, TILT, the spectral DECAY, STRETCH, SUB, the band limit,
#                   health and cost.
# WTABLE (tests/wavetable_test.c): the wavetable engine (src/eng_wavetable.c): the eight frames' spectra and the morph
#                   between them (sine, triangle, saw, vocal), HARM's tilt, SUB, UNI's beating, no aliasing at any octave
#                   from C1 to C8, the presets' DC / clipping / release, determinism, host cost.
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# DIGITAL -> FM6 (tests/fm4_test.c, built with FELUCCA_FM4=1): the retired four-operator engine against its conversion
#                   (src/fm4_convert.c): routes and carriers per algorithm, the presets' PTCH, and the sound (pitch,
#                   centroid, RMS envelope) of its presets and algorithms; demos in build/fm4_demo/. tests/digital_test.c
#                   (FELUCCA_FM4=1 too): DIGITAL's operator envelopes. Default builds have no DIGITAL (engine 1 reserved).
#                   tests/fm4_div0_test.c (UBSan, #61): the conversion without a divide that can be 0 gives the values the
#                   guarded form gave (every INDEX x FLT ENV, random sounds); a 0.9 project with DIGITAL ORGAN converts.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c): the 32 algorithms' carriers, the
#                   operator envelopes (stages, rates, the voice ending), bit-stable notes, a click-free retrigger, no DC /
#                   clipping over the factory patches, the macros' directions, PTCH, pack / unpack and the SysEx
#                   layouts, the 6-voice cap, the cost per voice; demos in build/fm6_demo/.
# FM6 BANK (tests/fm6bank_test.c, JIANT 0.6.11 from Felucca 1.4.1): the voice bank (src/fm6_vbank.c) and its editor
#                   commands against simulated NOR: write / read back, its sectors only, a power cut at any byte or erase
#                   (the old bank or the new one), a wrong CRC, SLOT B1..B32, the SLOT values stored before as before.
# ROBUST (tests/robust_test.c): damaged or crafted stored data and editor requests: engine numbers past the last
#                   refused, a retained older
#                   RAM project bounded, user preset patterns inside their fields, malformed requests never stop PLAY.
# Sanitizers (ASan + UBSan, when the compiler has them; SANITIZE=0 skips): the stored-data and protocol tests again
#                   (loader, M-UPGRADE entry, editor, projects, backup, ROBUST) and two short fuzz runs with fixed
#                   seeds: tests/fuzz_ed.c (editor SysEx and raw USB-MIDI packets), tests/fuzz_proj.c (mutated project
#                   stores -> import -> restore -> render). Longer runs: build/host/asan/fuzz_ed 300000 7 (iterations, seed), the same for the others.
#                   UBSan leaves out the DSP's intended wraps (signed overflow, shifts); bounds, object-size and
#                   pointer-overflow stay off as in Felucca (once for the user zones' XIP rebase).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
set -e
# (FASE 5e) THE HOST TESTS COMPILE THE FIRMWARE'S PACKED TABLES. preset_pack.h and lz4_tables.h are written by the
# packing pass inside ./build.sh (they are read out of the target compiler's own object), so a checkout that has
# never been built for the AC791N stopped at "preset_pack.h: No such file or directory" -- the host job in
# c-cpp.yml had no toolchain and no build, so it was red since FASE 1. Build them here when they are missing.
if [ ! -f build/gen/preset_pack.h ] || [ ! -f build/gen/lz4_tables.h ]; then
    echo "tables: build/gen is missing the packed tables (preset_pack.h, lz4_tables.h): building them with ./build.sh"
    mkdir -p build
    ./build.sh > build/tables.log 2>&1 || {
        tail -20 build/tables.log
        echo "::error::tables: ./build.sh failed: the host tests need JIELI_TOOLCHAIN and AC79_SDK to pack the factory tables"
        echo "tables: ./build.sh failed: the host tests need JIELI_TOOLCHAIN and AC79_SDK to pack the factory tables"
        exit 1
    }
fi

export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
# POLY=10 builds every host test with NVOICE=10 (firmware/src/core.h): the 10-voice profile, which is still
# built and measured. The shipped value is 12. Anything else is refused, so a typo cannot quietly produce a
# build whose voice count nobody wrote down.
case "${POLY:-12}" in
    12) ;;
    10) CC="$CC -DNVOICE=10"; echo "poly: 10 voices (NVOICE=10; the shipped build is 12)" ;;
    *)  echo "poly: POLY=${POLY:?} (the measured profiles are 10 and 12)" >&2; exit 2 ;;
esac
fail=0
# (2.16) A group that did not run is not a green result. Every "skip" here says so in the summary, and
# STRICT_EMU=1 (CI, where emsdk is installed by the workflow) turns a skipped emulator group into a
# failure: a run that cannot build the WASM must not look like a run that did and passed.
skips=""
n_skip=0
skip() { n_skip=$((n_skip + 1)); skips="$(printf '%s\n  - %s' "$skips" "$1")"; echo "== SKIPPED: $1"; }
# (1.3.7) A check that fails says which one, right there: a suite of hundreds of commands whose only trace
# of a failure is a `fail=1` needs the whole log to find it, and in CI that log needs a credential to read.
# The line is plain text on purpose (no GitHub syntax in a test script): anyone greps for FAIL.
# (FASE 5e) A FAILURE NAMES ITSELF where the CI can see it: the log of a GitHub job is not readable without admin
# rights on the repository, and ":error:" is what the run's annotations carry, so a red job says which test failed.
run() {
    n="$1"; echo "== $n"; shift
    "$@" > /tmp/qg-test.out 2>&1
    r=$?
    cat /tmp/qg-test.out
    [ "$r" = 0 ] && return 0
    fail=1
    echo "FAIL: $n"
    echo "::error::FAIL: $n"                    # the test's name, and the last lines it printed, in the annotations
    tail -8 /tmp/qg-test.out | while IFS= read -r l; do
        [ -n "$l" ] && echo "::error::$l"
    done
}

# A merge that left its markers behind compiles anyway when FELUCCA_VERSION comes in with -D, which is how the
# host tests build; the AC791N build does not define it, so `firmware/src/felucca.c` with a `<<<<<<< HEAD` inside
# its #ifndef failed there and only there, named as a build step and not as a file (v2.7.1 merge, 2026-11-11).
# Checked first, by file and line, so the next one is named here.
markers=$(grep -rn -e '^<<<<<<< ' -e '^>>>>>>> ' --include='*.c' --include='*.h' --include='*.py' --include='*.sh' \
    --include='*.yml' --include='*.json' firmware tests tools web .github 2>/dev/null || true)
if [ -n "$markers" ]; then
    echo "$markers"
    run "no source file carries a merge conflict marker" false
else
    run "no source file carries a merge conflict marker" true
fi

run "release gate: a 3.x tag needs an explicit repository approval" python3 tests/release_gate_test.py
# (4.0.0) the hub card: every published rule, and the sounds line that keeps the listing from stopping
run "the Firmware Hub card: every published rule holds, sounds declared, dropping it fails" python3 tests/fmhub_card_test.py
# The release set is one package described by six files; a release used to be assembled by hand and the sums
# drifted from 1.3.5 to 1.3.8. This is the same check tag-and-publish runs before it creates a tag.
run "release set: package, both SHA256SUMS, the site's versions.json and the installer's meta describe one package" python3 tools/make_release.py --check
# The matrix is the record and the index is generated from it: a row that goes back to PENDING, or a fixed
# row nobody re-read, fails here instead of being discovered by whoever reads the gate next.
run "evidence index: docs/EVIDENCE.md is exactly what docs/VALIDATION_MATRIX.md says" python3 tools/evidence_index.py --check

# A preset carries six more values; a seventh is dropped by the compiler in silence (AURORA DRIFT named an
# aftertouch its matrix never got, 1.5.19 to 1.12.11). The tables are read, so the count is a test.
run "factory presets: every x[] fits its six slots" python3 tests/preset_slots_test.py

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM and SAMPLE PERC -> DRUM, grid records, DIGITAL kept)" "$OUT/upreset_test"

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/flash_id_test" tests/flash_id_test.c
run "flash identification: allowlist, two agreeing reads, 1 MiB capacity, protection -> RAM-only" "$OUT/flash_id_test"

$CC -Ibuild/gen -Ifirmware/src -o "$OUT/mc_selftest_test" tests/mc_selftest_test.c
run "second core: the boot self-test refuses a misread shared mailbox" "$OUT/mc_selftest_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -Ibuild/gen -Ifirmware/src -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -Ibuild/gen -Ifirmware/src -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets, resampler, 48 kHz stream" "$OUT/uac_test_nocdc"
# USB descriptor layouts (#67): CDC UAC LAYOUT CDC-presented 48K; layout 0 and the console left out = 1.0's bytes
# (+ the 48 kHz rate with 48K = 1, the default; 48K = 0: byte for byte)
for v in 1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 \
         1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -o "$OUT/usb_desc_test" \
        tests/usb_desc_test.c
    run "USB descriptors: CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48), layout $t_lay" "$OUT/usb_desc_test"
done

if [ "${NO_PACKAGE:-0}" = 1 ]; then
    echo "== skip the package / loader tests (NO_PACKAGE=1: no ./build.sh package)"
else
    PKG="$(ls build/felucca*.fwsc 2>/dev/null | head -1)"    # a release build names it felucca-X.Y.fwsc
    [ -n "$PKG" ] && [ -f "$PKG" ] || { echo "run ./build.sh first (or NO_PACKAGE=1 for the host-only run)"; exit 1; }

    $CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
    run "M-UPGRADE entry (own loader)" "$OUT/ota_test" "$PKG"

    head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
    python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
    $CC -o "$OUT/ldr_test" tests/ldr_test.c
    run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" "$PKG"
fi

if [ -f build/gen/felucca_tables.h ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
    run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/speaker_test" tests/speaker_test.c -lm
    run "SPEAKER EQ: FLAT / LOWCUT / BASS+ responses, BASS+ harmonics of the bass, the sub cut, no offset after" "$OUT/speaker_test"
    run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
    run "project formats (FUN1..FUN5 -> FUN6, the grid and song chain; DIGITAL tracks -> FM6, SAMPLE PERC -> DRUM)" "$OUT/project_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/pattern_len_test" tests/pattern_len_test.c -lm
    run "pattern length per track (LEN 4 and 6 wrap on their own step 0; 5 / 7 / 12 / 64 through a project)" "$OUT/pattern_len_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/morph_audible_test" tests/morph_audible_test.c -lm
    run "MORPH moves the samples: every engine with a morph path renders a different waveform when P_MORPH moves under a held note (and without a click)" "$OUT/morph_audible_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/microstart_test" tests/microstart_test.c -lm
    run "M.STRT: a note waits a random 0..20 ms and no two notes wait the same (audio), the wait is silence and does not click, the level holds; N.RATE: the random wander holds 46 ms or a tempo division, down to 1/16" "$OUT/microstart_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/lofi_tclk_test" tests/lofi_tclk_test.c -lm
    run "LOFI T.CLK: NOTE restarts the formula every note, FREE joins it where the track clock is (audio, not a screen), keeps the level, survives a sound change and does not click" "$OUT/lofi_tclk_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/pm_test" tests/pm_test.c -lm
    run "(FASE 2) PM: ANALOG's WAVE = PM and PHASE's LINE = PM bend the phase, move with MORPH / DCW, brighten and are not SIN / MIX / RING" "$OUT/pm_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/phase2_test" tests/phase2_test.c -lm
    run "FASE 2: every engine's CHARACTER bank moves the sound, LFO SLEW / INVERT, ENV VEL, the pages and a project round trip" "$OUT/phase2_test"
    # (FASE 3) PHASE's tenth wave: RECT. Ten renders, one per WAVE value, ten different sounds; RECT in range,
    # DC-blocked and deterministic; the nine waves of before keep their names (and so their stored numbers).
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/phase_wave_test" tests/phase_wave_test.c -lm
    run "FASE 3: PHASE's ten waves are ten different sounds, RECT in range and DC-free, the old nine unchanged" "$OUT/phase_wave_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/phase5_test" tests/phase5_test.c -lm
    run "FASE 5: BAR is the bar's phase as a matrix source (silent when stopped, an arc inside the bar) and ATK DEC REL are the envelope's times as destinations (VEL -> ATK lengthens the loud note, nothing leaks)" "$OUT/phase5_test"

    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/morph_test" tests/morph_test.c -lm
    run "FASE 6: MORPH SCENES: four captured states of one sound, the position between a pair interpolates (0 is A, 127 is B), a jump walks 3 a frame, and a preset load leaves the scenes alone" "$OUT/morph_test"

    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/morph_six_test" tests/morph_six_test.c -lm
    run "MORPH under load: six parts, the FX buses, MIDI notes and a preset load -- every transition measured (latency, slew to 90%, click) with the two renders bit-identical before the turn" "$OUT/morph_six_test"
    run "the particle pool bays: EDIT ribbon draws none, HOME and the visualizer keep theirs inside their own rect, no allocation" python3 tests/particle_bounds_test.py
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords, drum hits), gates, chance, swing, projects, user presets, CHANCE page" "$OUT/ratchet_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/chord_scale_test" tests/chord_scale_test.c -lm
    run "CHORD: the 16 scales x 12 roots x 11 modes x 4 voicings stay in the key; the seven-note walk unchanged" "$OUT/chord_scale_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_control_test" tests/midi_control_test.c -lm
    run "USB/TRS clock, bend, sustain, ownership and panic recovery" "$OUT/midi_control_test"
    $CC -O1 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/digital_test" tests/digital_test.c -lm
    run "DIGITAL (retired, built here with FELUCCA_FM4=1): operator envelopes/levels" "$OUT/digital_test"
    $CC -O2 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/fm4_test" tests/fm4_test.c -lm
    mkdir -p build/fm4_demo
    run "DIGITAL -> FM6: the conversion against DIGITAL (FELUCCA_FM4=1): pitch, centroid, RMS envelope; demos" \
        "$OUT/fm4_test" build/fm4_demo
    $CC -O2 -w -fsanitize=integer-divide-by-zero -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src \
        -o "$OUT/fm4_div0_test" tests/fm4_div0_test.c -lm
    run "DIGITAL -> FM6 without a divide by zero (#61): the same values as before, a 0.9 ORGAN project (UBSan)" \
        "$OUT/fm4_div0_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test" tests/theme_test.c -lm
    run "themes: contrast, text blending and font metrics" "$OUT/theme_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/text_ref_test" tests/text_ref_test.c -lm
    run "text: pens, kerning and pixels equal the reference renderer" "$OUT/text_ref_test"
    if python3 -c "import PIL" 2>/dev/null; then
        run "text spacing: glyph gaps against the font's own (16x supersampled), S M <= 0.25 px, L <= 0.5 px" \
            python3 tests/text_spacing_test.py build/gen/ui_fonts.h
    fi
    $CC -O1 -w -Ibuild/gen -o "$OUT/settings_test" tests/settings_test.c
    run "settings: PER1..PER4 migration, palette ids and preference preservation" "$OUT/settings_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test" tests/ui_test.c -lm
    run "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, drum grid, song chain, GREY gray, MONO neutral" "$OUT/ui_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/planets_test" tests/planets_test.c -lm
    run "HOME worlds: a tap reaches each of the five bodies, and only there" "$OUT/planets_test"
    run "UI: DICE ALL on its own, 50 throws in a row in one process (no state left by the test before)" \
        env UI_ONLY=dice_all UI_REPEAT_DICE=50 "$OUT/ui_test"
    FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    rm -rf build/ui_new/ppm           # (FASE 5) from empty: a frame left by an older run cannot stand in for one the
    mkdir -p build/ui_new/ppm build/ui_slot   # sweep failed to write, so the pixel gate below sees exactly this run's
    # (v1.5.14) the faces lost nine Latin-1 extras and the '~' no string draws: 2 480 B of the app slot. This is
    # what keeps that sound -- gfx.c falls back to '?' for a code a face does not have, which is a wrong pixel no
    # test would see. Static, over the generated faces and every literal in firmware/src.
    run "font charset: S and M cover every character the firmware draws (a glyph trimmed from the faces fails)" \
        python3 tests/font_charset_test.py
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), GREY gray, MONO neutral, alignment by ink (1 px fails), draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    # (v1.5.5) the phase-2 report's three UI defects, on the RECTANGLES they touch: the preset pop-up's close leaves
    # no fragment in the MIXER's dB zone, the tempo's ink shares no pixel with the T3/T4 cushions (six columns and
    # LARGE), and a track change (ALGORITHM's encoder) writes only what changed -- with the frame identical.
    run "UI regions: pop-up close, the tempo against the T3/T4 cushions, a track change's cost" \
        "$OUT/ui_render" --regions
    # (v1.5.6) and what an untouched frame costs: over the 201 scenes of the sweep, the pixels the drawer sends to the
    # panel with nothing changed. It was 1 485 216 (29 280 on every page, the whole 240 x 122 band again) and is 109 056
    # with gfx's identical-band skip; a page that starts re-sending what the panel has fails this step.
    run "UI idle: an untouched frame sends the panel nothing it already has (201 scenes)" \
        "$OUT/ui_render" --idle
    # (v1.5.15) and what ONE STEP of HOME's ambient layer (and the MIXER's meters) costs the panel: gfx.c keeps a
    # hash a row now, so a step writes the rows that changed instead of the whole body (48 480 px). The mode also
    # proves the rows it did not send were the ones the panel already had, pixel for pixel, and that the step still
    # animates something (a fix that freezes the embers would pass the first two and fail this one).
    run "UI bands: a step of HOME's ambience / the MIXER's meters writes only the rows that changed AND leaves the frame a whole redraw leaves" \
        "$OUT/ui_render" --bands
    # (v1.5.20) issue #106's first symptom, measured where it was: the HEADERS of HOME 2..5, drawn twice with one step
    # of the clock between, in FLAT and LINE, with ANIM and with it off. A pixel that changes in the header band with
    # no value changing is the flicker; the mode also moves the tempo on purpose, so a header that simply stopped
    # repainting cannot pass (`--regions` B proves the same ink shares no pixel with T3/T4, `--bands` that the body
    # writes only what changed).
    run "UI headers: HOME 2..5 stand still in FLAT and LINE, with ANIM and without it, and the tempo still lands in the header" \
        "$OUT/ui_render" --headers
    # (v1.5.20, issue #105) what the instrument would SAY, through the real path (ui.c ann_ui -> announce.c): nothing
    # with no reader, the page on HELLO, the row of MENU with its tab, name and value as the selection moves, its
    # value once a turn settles, and WHERE answering with both. The voice is the reader's; this is the bytes.
    run "Screen reader: silence without a reader, the page, the MENU row said and its value announced once it settles" \
        "$OUT/ui_render" --announce
    # (FASE FX 2026-11-27) the pages that must MOVE, measured in pixels: the chorus answering its four knobs, the
    # FX page's meters at 0.9 / 10 / 60 %, the six blocks of FX > BUSES and the ADSR's attack (before and after).
    run "FX animation: chorus, the FX meters, the six BUSES blocks and the ADSR's attack, in pixels" \
        "$OUT/ui_render" --fxanim
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    # (FASE 5) and the pixels themselves against the tracked baseline: the lint above cannot see a glyph table, a
    # palette or a 1 px offset that still parses as aligned. `ui_pixels.py --update` only when it was meant.
    run "UI pixels: every frame the sweep draws is byte-identical to tests/ui_pixels.txt (a moved pixel fails)" \
        python3 tests/ui_pixels.py build/ui_new/ppm
    # (FASE 7) the perf log's host side: the CSV the console prints, read back from a capture made by hand. What it
    # cannot prove is what the unit does -- that is the unit's -- but it pins the shape of the line and the grouping.
    run "perf log: a captured stream is parsed, grouped by screen and by song, and its three pictures are written" \
        python3 tests/perf_log_test.py
    mkdir -p build/capture            # (a clean build/ has no capture dir: the sed below used to fail first)
    sed 's/^int main(/int ui_render_main(/' tests/ui_render.c > build/capture/ui_render_nomain.c
    mkdir -p build/capture/ppm
    $CC -O1 -w -DUI_NO_DRIFT=0 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/capture_pokes" tests/capture_home_pokes.c -lm
    run "HOME's five touch reactions as drawn (UI_NO_DRIFT=0): each body's answer, five distinct ones, a tap off the bay is nothing, a second touch restarts" \
        "$OUT/capture_pokes" build/capture
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/home_roundtrip" tests/home_roundtrip_test.c -lm
    run "HOME round trip (UI_NO_DRIFT=1, fixed clock): 50 entries into and out of each of the five views, twice, entry by entry -- a page that does not come back the same fails" \
        "$OUT/home_roundtrip"
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/scl_test" tests/scl_test.c -lm
    run "SCL as a player's tool (FASE 3): every chord shape recognised, the inversions named from the bass, ambiguity marked, the keybed's picture and its hit test agree, a tapped chord reaches the panel" \
        "$OUT/scl_test"
    # (FASE 5b) ANALOG's SYNC / SUB and ADDITIVE's DRAW, each knob measured: the neutral render, the octave
    # below, the hard sync, the descriptors, and every one of the ten knobs moving the audio by 1 % or more
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/analog_sync_test" tests/analog_sync_test.c -lm
    run "FASE 5b: ANALOG SYNC / SUB and ADDITIVE DRAW, each knob moves the audio (neutral is bit for bit)" \
        "$OUT/analog_sync_test"
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/tune_test" tests/tune_test.c -lm
    run "QUANTUM TUNING (FASE 7): the cents a temperament asks for are the cents the engine plays (measured as a frequency ratio, 99 cents included), the class follows the note through transposition and octaves, pitch bend rides on top, the standard tuning is bit for bit what it was, USER slots keep what they are saved with, a corrupt or older settings record recovers to EQUAL" \
        "$OUT/tune_test"
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_nav" tests/ui_nav_test.c -lm
    run "Navigation across HOME, VISUALIZER and the sections (UI_NO_DRIFT=1): 14 states walked twice, each compared with a full repaint of the same state -- a cached header, card or strip left stale fails" \
        "$OUT/ui_nav"
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_flicker" tests/ui_flicker_test.c -lm
    run "Flicker (v1.7.20): HOME's visualizer, HOME's cards and the preset browser over eight detents each -- an element or band erased for one frame and back, or a row written twice inside a frame, fails" \
        "$OUT/ui_flicker"
    $CC -O1 -w -DGFX_MIX_COUNT=1 -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/blend_budget" tests/blend_budget_test.c -lm
    run "Blend budget (v1.7.21): blends a frame, counted -- the visualizer's veil is a table, not a mix a pixel (19 052 -> 4 396), and no screen may come back to mixing a big area dot by dot" \
        "$OUT/blend_budget"
    $CC -O1 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/viz_worlds" tests/viz_worlds_test.c -lm
    run "VISUALIZER's two deep worlds: the giant reads tracks 1-3 and the rock 4-6 -- a loud half moves its own world only, and both carry a surface" \
        "$OUT/viz_worlds"
    $CC -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test_fm4" tests/ui_test.c -lm
    run "UI built with FELUCCA_FM4=1 (DIGITAL, kept in the tree): its OP pages, EDIT cycle, algorithm charts" "$OUT/ui_test_fm4"
    $CC -O1 -w -DFELUCCA_FM4=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render_fm4" tests/ui_render.c -lm
    mkdir -p build/ui_fm4/ppm build/ui_fm4_slot
    run "UI renders with FELUCCA_FM4=1: DIGITAL's screens (EDIT, OP ENV, the 8 algorithm charts), lint, GREY gray, MONO neutral" \
        "$OUT/ui_render_fm4" build/ui_fm4 build/ui_fm4_slot
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    # (1.4.4) POLY from the output: three keys on one part, three voices, three fundamentals in the spectrum, with
    # and without the load governor set (the owner's "no polyphony" is pinned here as behaviour that must hold)
    $CC -w -O2 -Ibuild/gen -Ifirmware/src -o "$OUT/poly_test" tests/poly_test.c -lm
    run "POLY: three keys give three voices and three notes in the output" "$OUT/poly_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_voices_test" tests/poly_voices_test.c -lm
    run "POLY (voices): eight and twelve notes on one part, six parts sharing the pool, LOAD LITE / ECO, MENU VOICES, the keybed, and the mix itself" "$OUT/poly_voices_test"
    # (FASE 2, issue #107) the report's own path: the engine's preset, the keybed, 1..12 keys one at a time and at
    # once, SUSTAIN, the load budget, DUAL CORE, a long chord, fast on/off, and the voice states the monitor prints
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_report_test" tests/poly_report_test.c -lm
    run "POLY (issue #107): 1..12 keys on ANALOG BRASS / PLUCK and POLY, at once and one by one, SUSTAIN, LOAD TIGHT, DUAL CORE, long chord, fast on/off, release vs sounding and the peak" "$OUT/poly_report_test"
    # (FASE 2) a preset change on a silent track must not reset the instrument-wide performance layer
    run "PRESET CHANGE: perf_reset and fx_preset_flush are gated on the changed track having a voice, so a preset change from GLO MIXER on a silent track cannot cut the rest of the instrument" python3 tests/preset_change_order_test.py
    # (FASE 2) the power-on order of the audio path: held before the engine, opened after the second core
    python3 tests/boot_order_test.py
    run "BOOT ORDER: main.c holds the output before audio_init (the unmute lands on zeros, the fade is not spent behind the mute), opens it after the second core, and the HAL starts the engine before it unmutes" python3 tests/boot_order_test.py
    # (FASE 3) six parts, six engines, effects, chords, fast attacks, preset changes: the allocator's counters
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_stress_test" tests/poly_stress_test.c -lm
    run "POLY STRESS: 24 notes into 12 voices never exceed the pool, no part starved, no note denied, peak == what sounded, six parts + FX inside the block budget, an engine change under a held chord leaves nothing, 20 notes into 12 count their steals" "$OUT/poly_stress_test"
    # (v1.5.13) the budget itself, measured: 8 / 10 / 12 held notes on one part all sound, the setting 10 / 8 is the
    # cap the allocator applies, LOAD ECO's 6 voices (4 a part) are what the monitor now prints, FM6's own 4, four
    # tracks at once, the release queue, and a preset change under a held chord that neither cuts the audio nor leaves
    # a voice behind. The class of every figure is this host's, not the unit's.
    # (v1.5.18) SEQ > TOOLS' OPERATORS, driven directly (firmware/src/seq.c seq_tool_apply): the four the page gained
    # (EUCLID, MIRROR, THIN, CRESC) and the ones it had, each checked on the pattern it produces, and on what it must
    # not touch (a step's timing, its gate, its chance).
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/seq_tools_test" tests/seq_tools_test.c -lm
    run "SEQ TOOLS: DUP HALF ROT REV CLEAR still as they were, EUCLID spreads the same notes over even onsets, MIRROR inverts the line around the tonic inside the scale, THIN takes every n-th onset out, CRESC gives the phrase a direction, and none of them writes past the length or moves a gate or a chance" "$OUT/seq_tools_test"
    # (v1.5.17) THE WHOLE INSTRUMENT: six live tracks and twelve voices together, concentrated, distributed and
    # saturated, with releases, the load governor, the FX, a preset change under the chord, TRACKS 4's sleep, and the
    # cost of waking T5/T6 measured on the same machine in the same run.
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_six_test" tests/poly_six_test.c -lm
    run "POLY SIX: six tracks x 2 = twelve sounding, twelve in one part, 4+4+2+1+1, releases (six gated six ringing), at the cap the released voice goes not a held one, the load governor does not cut a tail with room in the budget, twelve with the FX on, a preset change under the chord, TRACKS 4 sleeps, saturation refuses nothing, and the host cost of 4 vs 6 tracks" "$OUT/poly_six_test"

    # (FASE 2 de esta ronda) WHO LOST THE VOICE: the per-part counters (taken / refused), a part with no window,
    # twelve released at once, a long session with engines moving under it, and the saturation of the counters
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_parts_test" tests/poly_parts_test.c -lm

    # (FASE 4 de esta ronda) the eight voicings, the four of before untouched
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/chord_voicing_test" tests/chord_voicing_test.c -lm
    run "VOICINGS: CLOSE LEAD OPEN +BASS give the notes they always gave, DROP2 and DROP3 move the second and third voice from the top an octave down, QUART stacks fourths, SPREAD lifts every other tone, and all eight keep every shape in range and in order on every key" "$OUT/chord_voicing_test"
    run "POLY PARTS: a full pool names the part that gave the voice, a part with no window is the one blamed, a release is not a steal, a long session with engines moving keeps sum(taken)=voices the pool took and ends empty and whole, and the counters saturate" "$OUT/poly_parts_test"

    # (FASE 3 de esta ronda) a step remembers the length the key was really held for
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/seq_dur_test" tests/seq_dur_test.c -lm
    run "SEQ DUR: playback ends the note where the recorded length says and dur 0 is the GATE, recording measures a quarter of the step as 4/16 and a chord keeps its longest key, a note that crosses steps keeps the tail it really held in the last TIE step (recorded and played back), and a project carries the lengths in the step itself (no byte when there is none)" "$OUT/seq_dur_test"
    # (v1.5.23, FASE 2) the situations the earlier poly tests do not reach: a twelve-note chord through a sound load
    # that changes engine (xp_note held four), twelve keys on a MONO / LEGATO part with PRIO LOW (mono_stack held
    # eight), a 24-note burst, twelve accumulated releases against a new chord, six parts competing, a preset change
    # under the chord with the FX on, and three saturation/recovery cycles.
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_phase2_test" tests/poly_phase2_test.c -lm
    run "POLY FASE 2: a twelve-note chord re-struck whole on the new engine after a load, twelve keys in a LEGATO stack with PRIO LOW still playing the low one, a 24-note burst with every steal counted and no stuck note, twelve accumulated tails paying for a new chord, six parts competing without starving, a preset change under the chord with the FX on, and three saturation/recovery cycles leaving the pool whole" "$OUT/poly_phase2_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/env_test" tests/env_test.c -lm
    run "ENV: OFF / LOOP / ONE differ (ONE frees a held voice, LOOP still cycles), a short note does not jump, two parts and two voices keep their own envelopes, and the steal fade only goes down" "$OUT/env_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/lfo_test" tests/lfo_test.c -lm
    run "LFO CURVE: the bent phase never runs backwards, every wave keeps its range, 0 is the identity, +60 arrives earlier and -60 later, a smooth wave under it has no step of its own" "$OUT/lfo_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/tilt_test" tests/tilt_test.c -lm
    run "TILT: the per-voice tilt moves the top and leaves the body (bright doubles the top, dark takes it away), a peak is bounded by its gain, a lent slot starts with no pole and one voice's cost stays small" "$OUT/tilt_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/poly_budget_test" tests/poly_budget_test.c -lm
    run "POLY BUDGET: 8, 10 and 12 notes held on one part all sound with nothing stolen, the VOICES setting is the cap applied, LOAD ECO caps at 6 (4 a part) and says so, FM6 gives its engine cap, four tracks fill the pool, releases ring as releases, and a preset change under a chord keeps the audio" "$OUT/poly_budget_test"
    # (v1.5.13) the GLO MONITOR rows, built with a bounded appender: the diag page's POLYPHONY row outgrew its 48-byte
    # array while a sequence ran, str_len() walked past it and the next append wrote beyond the array -- the fields
    # reported as overlapping. The builder is driven here with every counter at its worst, under ASan/UBSan.
    $CC -O1 -w -fsanitize=address,undefined -o "$OUT/rowb_test" tests/rowb_test.c -lm
    run "ROWB: every GLO MONITOR row stays inside its buffer with the counters at their worst (ASan/UBSan), a row that does not fit is cut short but stays a string" "$OUT/rowb_test"
    # (FASE 1) the note the key scan keeps while core 1 renders: the pool is never touched mid-job
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/voice_defer_test" tests/voice_defer_test.c -lm
    run "CORE SYNC: a note while the other core renders is postponed (never allocated mid-job), replayed at the block start, note-offs never dropped, a burst over the queue loses only note-ons" "$OUT/voice_defer_test"
    # (FASE 3, issue #105) the screen reader's protocol (announce.c) and the USB-MIDI framing usb.c uses
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/announce_test" tests/announce_test.c
    run "SCREEN READER (issue #105): silent until HELLO, the HELLO answer with the words link, SAY/WHERE/BYE, the 5 s timeout, a line cut at a word, and USB-MIDI CIN 4/5/6/7" "$OUT/announce_test"
    # (FASE 8) the pronunciations: firmware/words.tsv -> the JSON the reader fetches, named after its own content,
    # published with the site, and the exact file announce.c's ANN_WORDS_URL links to (a stale link fails here)
    run "SCREEN READER words: words.tsv, the published JSON and announce.c's ANN_WORDS_URL name the same file, and it fits one HELLO frame" python3 tools/gen_words.py --check
    # (FASE 3) how long a note takes to start: prints, never fails (a comparison tool, not a budget)
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/alloc_bench" tests/alloc_bench.c -lm
    "$OUT/alloc_bench" | sed 's/^/  /'
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/control_map_test" tests/control_map_test.c -lm
    run "RAM / flash map: every object over 1 KiB with its section, and no two rodata objects holding the same bytes (the same flash paid twice)" python3 tools/ram_flash_map.py --dups --check --min 1024
    run "pages: every knob of every page, in every engine and at every state a computed page can be in, edits its own value and no two share one (MASTER > STEREO M/S is the master width, not a track's release)" "$OUT/control_map_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/master_test" tests/master_test.c -lm
    run "FX > MASTER: CLIP, PUNCH, DUCK and DREL each change the rendered mix" "$OUT/master_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_meter_test" tests/fx_meter_test.c
    run "FX meters: attack, release, peak hold (30 frames then a pixel a frame) and clamping" "$OUT/fx_meter_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Ifirmware/hal -o "$OUT/clamp_sym_test" tests/clamp_sym_test.c -lm
    run "DSP: the SVF's one-compare clamp is exactly clamp(v, -150000, 150000) over all of int32" "$OUT/clamp_sym_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/blep_test" tests/blep_test.c -lm
    run "DSP: the polyBLEP fast path is the reference, bit for bit (both window edges, the increments past Nyquist, 10 M pairs)" "$OUT/blep_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/click_test" tests/click_test.c -lm
    run "metronome and count-in: beats sample for sample (internal and external clock), accent, level, MASTER, not in USB, count-in timing, notes onto step 1" "$OUT/click_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/persistence_test" tests/persistence_test.c -lm
    run "persistence: deferred settings, retry and failed-save rollback" "$OUT/persistence_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/startup_test" tests/startup_test.c -lm
    run "startup: saved arrangement survives initialization and idle saving (by @workeffortwaste, #134)" "$OUT/startup_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/lcd_st7789_test" tests/lcd_st7789_test.c
    run "LCD: the ST7789's 240x320 frame memory written whole before DISPON and the backlight (the four black lines)" \
        "$OUT/lcd_st7789_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/backup_test" tests/backup_test.c -lm
    run "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" "$OUT/backup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
    run "editor: real C protocol, malformed transfers, queue recovery, MENU settings (writes build/host/menu.json)" \
        env MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/robust_test" tests/robust_test.c -lm
    run "robustness: engine numbers, retained old projects, preset patterns, malformed requests" \
        "$OUT/robust_test"
    $CC -O2 -w -DMOD_TSRC_COUNT=1 -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
    mkdir -p build/mod_demo
    run "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" "$OUT/mod_test" build/mod_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
    mkdir -p build/slicer_demo
    run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/swing_test" tests/swing_test.c -lm
    run "SWING: track + global at most 100, sequencer and SLICER step lengths, the SWG display" "$OUT/swing_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/perform_test" tests/perform_test.c -lm
    mkdir -p build/perform_demo build/fx_demo
    run "FX layer effects: on the 1/16, stereo, too-long REPEAT, SLICER, silent keys, idle bit-identical, clicks, cost, demos" "$OUT/perform_test" build/perform_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
    run "REVERB TYPE: ROOM bit-identical, SPRING decay / chirp / stability / level, model change, cost, demos" "$OUT/reverb_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/dly_echo_test" tests/dly_echo_test.c -lm
    run "DELAY echo train: the repeats FX > DLY draws, one TIME apart, falling by FEEDBK, none without echoes" "$OUT/dly_echo_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_probe" tests/fx_probe.c -lm
    run "FX measured: the phaser's and flanger's comb depth and sweep, the buses' routing, M-WIDE mono/wide/level, the low cut, the master EQ's shelves and peak" "$OUT/fx_probe"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_stress" tests/audio_stress.c -lm
    run "AUDIO STRESS: six engines, every bus fed, MIDI and preset changes under the notes: no NaN, no clip, no DC, no click" \
        "$OUT/audio_stress" 8
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_bench" tests/fx_bench.c -lm      # (7.0) built, not run: it measures
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/isr_bench" tests/isr_bench.c          # (FASE 1) the TIMER5 tick: built, not run
    $CC -O2 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests \
        -o "$OUT/frame_bench" tests/frame_bench.c -lm                                     # (FASE 2/9) the idle main loop: built, not run
    $CC -O2 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests \
        -o "$OUT/morph_depth" tests/morph_depth.c -lm                                   # (FASE 5) reads the depth off the panel
    # (FASE 5) MORPH's depth stack, proved on the panel it is drawn on: the geometry each pass drew (the state
    # graph_morph keeps and morph_y computes) against the pixels that came out of it, column by column.
    run "MORPH DEPTH: four layers at every value, the back waves 3 px away at the median, and the pixels where the geometry says" \
        "$OUT/morph_depth"
    # (v2.0.3) HOW DEEP THE STACK GOES, measured: the plumbing the firmware's compiler cannot report
    # (the JieLi clang rejects -fstack-usage). Host x86-64 frames, not the unit's -- the number is the
    # shape of the C and a gate against a change that deepens it. Each scene runs in a thread whose
    # stack is an allocated, painted buffer, so the boundary read back is exact.
    $CC -O2 -w -DUI_NO_DRIFT=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests \
        -o "$OUT/stack_watermark" tests/stack_watermark.c -lm -lpthread
    run "stack: the deepest frame chain of the mix, a project capture/pack and the UI stays in budget" \
        "$OUT/stack_watermark"
    $CC -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/eng_bench" tests/eng_bench.c -lm  # (FASE 7) one engine at polyphony: built, not run
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/tempo_test" tests/tempo_test.c -lm
    run "tempo: the cached beat, div_samples and div_by_n equal the divisions they replaced" "$OUT/tempo_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/preset_audit_test" tests/preset_audit_test.c -lm
    run "PRESETS: all of them loaded and rendered (structure, silence, level, duplicates, DC)" "$OUT/preset_audit_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/param_sweep_test" tests/param_sweep_test.c -lm
    run "PARAMS: every engine's eight EDIT knobs move the audio (a knob the DSP never reads fails here)" "$OUT/param_sweep_test"
    run "PARAMS: every track parameter (envelope, filter, LFO, sends, CHARACTER) moves the audio" "$OUT/param_sweep_test" all
    "$OUT/preset_audit_test" --fingerprint > "$OUT/preset_fingerprint.jsonl"
    run "PRESETS: no two of them measure as the same sound (ten bands, level and envelope, per engine)" \
        python3 tools/preset_audit.py --gate-sound 0.05 --fingerprint "$OUT/preset_fingerprint.jsonl"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/engine_param_test" tests/engine_param_test.c -lm
    run "ENGINES: every EDIT knob at both ends of its range moves the rendered note" "$OUT/engine_param_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_exp2_test" tests/fm6_exp2_test.c -lm
    run "FM6: the 1025 exp2 points are stored as a second difference and decode to the generator's integers exactly (2054 B of XIP instead of 4100)" \
        "$OUT/fm6_exp2_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/sine_table_test" tests/sine_table_test.c -lm
    run "SINE: the 1025 points are stored as int8 second differences and decode to the generator's integers exactly (1025 B of XIP instead of 2050)" \
        "$OUT/sine_table_test"
    san7="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -w"
    $CC $san7 -Ibuild/gen -Ifirmware/src -o "$OUT/etapa7_audit_test" tests/etapa7_audit_test.c -lm
    ASAN_OPTIONS=detect_leaks=0 run "ETAPA 7: ARP indices, PHASE waves, the SCL/LFO/MOD tables and their bounds" "$OUT/etapa7_audit_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/etapa5_tools_test" tests/etapa5_tools_test.c -lm
    run "ETAPA 5: ARP PEDAL/ALBERTI, SCL HIJAZ/HUNG MINOR and LFO 1/2D, 1/16D are new and exact" "$OUT/etapa5_tools_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/pd_sync_test" tests/pd_sync_test.c -lm
    run "PHASE: wave 8 SYNC is a new sound (9 distinct waves on one note) and DCW is its ratio" "$OUT/pd_sync_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/engine_options_test" tests/engine_options_test.c -lm
    run "ENGINES: PHASE LINE = FOLD and LOFI WAVE = SYNC sound, differ from every other value of their knob, and no preset uses them" \
        "$OUT/engine_options_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/chorus_rate_test" tests/chorus_rate_test.c -lm
    run "CHORUS RATE: 0.05..6.00 Hz on every path, the DSP's own rate at the top, a corrupt stored byte, the merged pages" "$OUT/chorus_rate_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/cloud_hot_test" tests/cloud_hot_test.c -lm
    run "CLOUD driven hot (JIANT): the loop never runs away, its line never clamps, no grain across the write head, SIZE 127 from silence" "$OUT/cloud_hot_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/tone_cache_test" tests/tone_cache_test.c -lm
    run "TONE cache: a hit is a first build's bytes, a knob turn reaches the DSP, TONE 0 is out of the chain" "$OUT/tone_cache_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/boot_fade_test" tests/boot_fade_test.c -lm
    run "BOOT FADE: gain 0 is exact silence, the ramp never goes back, 32768 is the unfaded render" "$OUT/boot_fade_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/dist_models_test" tests/dist_models_test.c -lm
    run "DIST: the five models (SOFT TAPE HEARTH FLUX FOLD) sound, stay in 16 bits, FOLD loudness-neutral and DC-free, TYPE out at DRIVE 0" "$OUT/dist_models_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/preset_swap" tests/preset_swap_test.c -lm
    run "PRESET SWAP: every preset loads from a dirty state (no inheritance, MOD 5..8 reset), 600 changes with notes held (no NaN, nothing left holding)" "$OUT/preset_swap"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_test" tests/drumx_test.c -lm
    mkdir -p build/drumx_demo
    run "DRUM-X: every lane sounds and ends, MORPH, PITCH, NOIS, hat choke, group mutes, demos" "$OUT/drumx_test" build/drumx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/wavetable_test" tests/wavetable_test.c -lm
    run "WTABLE: the frames and the morph, HARM, SUB, UNI, aliasing at every octave, health, cost" "$OUT/wavetable_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/additive_test" tests/additive_test.c -lm
    run "ADDTV: the profiles and the morph, TILT, spectral DECAY, STRETCH, SUB, band limit, health, cost" "$OUT/additive_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/engine_arena_test" tests/engine_arena_test.c -lm
    run "ARENA: per-track FM6/DRUM and per-slot WHEEL/ADDITIVE/NOISE state, engine switches under held notes, 6 tracks, stealing, unison" "$OUT/engine_arena_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
    mkdir -p build/fm6_demo
    run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, cost, demos" "$OUT/fm6_test" build/fm6_demo
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6bank_test" tests/fm6bank_test.c -lm
    run "FM6 voice bank: write / read, power cuts, CRC, SLOT B1..B32, stored SLOT values as before; GW-1 KEYS built in, the 1.2 presets" "$OUT/fm6bank_test"
else
    echo "== skip hostsim (run ./build.sh once)"
fi

# (v1.9.0) EQ and DSP, and the host-side security tools. These tests existed but no script ran them, so a
# regression in the master biquad, the six-track load, the chip gates, the raw restore, the signature
# check or the installer preflight could not fail anything. They include hostsim.c, like the rest.
# (FASE 3) filter_types_test: the part filter's NOTCH and SHELF measured through track_filter, with
# LP and HP as the reference and a full-resonance sweep that must stay finite.
for t in eq_dc_test eq_response_test eq_state_test eq_sweep_test filter_types_test six_track_test six_track_live_test; do
    if [ "$t" = filter_types_test ]; then
        $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/$t" "tests/$t.c" -lm
        run "filter types (FASE 3): NOTCH takes the cutoff out, SHELF lifts the bass, both from the SVF's own outputs" \
            "$OUT/$t"
        continue
    fi
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/$t" "tests/$t.c" -lm
    run "EQ / load ($t): DC gain against the coefficients, 11-bit state, stability, response, six tracks, and the six-track live scene" \
        "$OUT/$t"
done
run "transport gate: the image bytes leave only from #serve, behind requireProof(), install/write/resume" \
    python3 tests/transport_gate_test.py
run "chip identification: 0x12/0x13 gates, no write frame without a verified part" python3 tests/chip_test.py
run "flash evidence: a read bundle verifies only against its own bytes and its read-only frame log" \
    python3 tests/flash_evidence_test.py
run "external read: the first backup of a stock unit, two reads, and the restore hand-off" \
    python3 tests/flash_read_test.py
run "flash image: boot/app tables, CRC16s and the identity decoded from the stock layout" \
    python3 tests/mflasher_test.py
run "raw restore: geometry, identifiers, sector alignment, readback, refusal before any erase" \
    python3 tests/restore_test.py
# (PR #73) the gate's refusal matrix and the backup audit: both import tools/fm1_gate.py and
# tools/fm1_image.py and assert on the real refusal codes, so a regression in either fails here.
run "install gate: read-only chips, unknown JEDEC, model and identity refusals before any write" \
    python3 tests/negative_write_test.py
run "backup audit: region bounds, bootloader never written, dump and restore evidence agree" \
    python3 tests/backup_audit_test.py
if [ "${NO_PACKAGE:-0}" = 1 ]; then
    echo "== skip installer preflight (NO_PACKAGE=1: needs build/felucca.fwsc)"
else
    run "installer preflight: read-only, refuses before the first write frame" python3 tests/preflight_test.py "$PKG"
fi

# ASan + UBSan: the stored-data and protocol paths, and short fuzz runs (fixed seeds: the same inputs every run)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow"
SAN="$SAN -fno-sanitize-recover=undefined -w"
san_ok() {
    printf 'int main(void){return 0;}\n' > "$OUT/san_probe.c" &&
        ${CC%% *} $SAN -o "$OUT/san_probe" "$OUT/san_probe.c" 2>/dev/null && "$OUT/san_probe"
}
if [ "${SANITIZE:-1}" = 0 ]; then
    echo "== skip the sanitizer runs (SANITIZE=0)"
elif [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the sanitizer runs (run ./build.sh once)"
elif ! san_ok; then
    echo "== skip the sanitizer runs (the compiler has no ASan / UBSan)"
else
    mkdir -p "$OUT/asan"
    A="$OUT/asan"
    SCC="${CC%% *} $SAN -Ibuild/gen -Ifirmware/src"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    if [ "${NO_PACKAGE:-0}" != 1 ]; then
        $SCC -o "$A/ldr_test" tests/ldr_test.c
        run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" "$PKG"
        $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
        run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" "$PKG"
    fi
    for t in editor_test project_test pattern_len_test backup_test robust_test fm6bank_test; do
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
    # (FASE 6) the seed that found it: a project whose track window is 0 reached a % 0 in voice_alloc.
    run "ASan/UBSan fuzz: projects, the seed that found the vn=0 divide (20000, seed 7)" "$A/fuzz_proj" 20000 7
    # (v1.5.16) THE ARITHMETIC ITSELF, STRICT. The SAN above excludes signed-integer-overflow and shift on purpose:
    # this code is full of shifts on Q15 values and the suite would drown. ui_test drives every engine, the FX, the
    # sequencer and every screen, so the ones that really are undefined -- a left shift of a negative, an overflow a
    # product hits at full scale -- are named here instead of shipping. v1.5.16 fixed 14 of them, all silent on the
    # host and all reachable from the unit's own controls.
    UBSCC="${CC%% *} -O1 -g -fsanitize=undefined,bounds -fno-sanitize-recover=all -w -Ibuild/gen -Ifirmware/src"
    $UBSCC -o "$A/ub_ui" tests/ui_test.c -lm
    UBSAN_OPTIONS=halt_on_error=1 run "UBSan strict: ui_test, every engine and screen (no signed overflow, no shift of a negative, no OOB)" "$A/ub_ui"
fi

# (v1.5.12) the RAM windows, when there is a target build to read: the map is what the linker wrote, and a
# feature that eats the headroom has to say so in tests/mem_budget.json instead of failing on the unit
if [ -f build/felucca.map ]; then
    run "memory: every RAM window keeps its floor (AC791N link map)" python3 tools/mem_budget.py
fi
run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt
run "the ST7789's init table: no gamma, no brightness, no VCOM (the 1.4 black-screen class, kept out)" \
    python3 tests/resources_test.py
    python3 tests/panel_table_test.py firmware/src/lcd.c
run "target memory map: 1 MiB flash, RAM / pool / stacks / noinit do not overlap" python3 tests/memory_map_test.py
run "target memory map: the ELF's RAM windows report their used and free bytes (tools/mem_map.py)" python3 tools/mem_map.py --top 3
run "engine size: which symbol belongs to which engine, XIP vs RAM, the map cross-check" python3 tests/engine_size_test.py
run "engine size of this build: XIP and .data+.bss per engine (skips without build/felucca.syms)" \
    python3 tools/engine_size.py --strict --map --no-image

# (JIANT 0.6.83) the audio path's CPU where proc_pid_rusage is missing (Linux): callgrind's exact counts, section by
# section (each engine's heaviest preset, every REVERB TYPE, a full mix, the sequencer at its heaviest), against
# tests/cpu_baseline_vg.txt (x86-64 gcc; another compiler: BUDGET_UPDATE=1 once, then it watches from there)
if command -v valgrind >/dev/null 2>&1 && [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/cpu_vg" tests/cpu_vg.c -lm
    run "CPU under valgrind: engines, reverbs, full mix, the sequencer's heaviest (exact counts, +8 %)" \
        python3 tests/cpu_vg.py "$OUT/cpu_vg" tests/cpu_baseline_vg.txt
else
    echo "== skip CPU under valgrind (no valgrind, or not Linux x86-64)"
fi

run "preset library: factory sounds, per-engine counts, DRUM's indices, names" python3 tests/preset_library_test.py
run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py
# the two official M-VAVE releases (V15 V16) as both installers pin them: one digest, one layout,
# one loader window, so a page and a terminal never disagree about which file is official
run "stock profile: V15/V16 pins agree in both installers" python3 tests/stock_profile_test.py
run "backup tool (fm1_archive.py): two independent passes, manifest, classification, verified restore" \
    python3 tests/backup_tool_test.py
run "README navigation contract (WEB SITE first and double width, names, links, palette)" \
    python3 tests/readme_nav_test.py

if command -v node >/dev/null 2>&1; then
    # (v1.5.21) AND THE WEB STUDIO'S DEVICE IS THE FIRMWARE'S. v1.5.19 added three presets and left web/editor.html's
    # mock lists behind: the simulator answered with 22 WTABLE sounds where the device has 23, and only the full web
    # test (the mirror of build/host/desc.json) noticed, after the release. This step fails the build instead.
    run "Web Studio: the mock device's preset lists and PATTERNS are the firmware's (no drift)" \
        python3 tools/sync_editor_mock.py --check
    run "web pages: editor protocol, package builder, update protocol" node web/test_web.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
    # (1.9) Verify Only is a read: the real Updater against a simulated FM-1, every frame it receives
    # counted -- handshake and one INFO read, never UPGRADE, a PUT or a resume
    run "web verify-only: identity reads only, zero write frames" node web/test_verify_only.mjs
    # V15 and V16 are the stock releases this project has read: both are origins for a first
    # install, and neither may answer as the installed identity afterwards.
    run "web donor: V15/V16 are origins, an unread stock number is not" node web/test_donor.mjs
    # the pinned official files: digests, the measured V16 layout, and the loader-window rule that
    # makes a vendor V16 file a restore target rather than something a QG package is patched from
    run "web stock releases: V15/V16 pins, the V16 layout and the loader window" node web/test_stock_releases.mjs
    run "web V16 path: the unit's own version, the echo/partial chip diagnosis, and the copy a V16 needs" node web/test_v16_path.mjs
    # (first install) the chip fact a stock unit cannot give over MIDI: the record M-Flasher wrote,
    # its dump hashed in the page, and the donor install path that accepts them -- with the wire
    # assertions that nothing is sent without them.
    # (first install) a unit sitting in its update mode answers the handshake and not 0x12, so the
    # chip gate on the loader link comes from the same attempt or from the record -- and a unit's own
    # update mode is never fed a Felucca image.
    run "web update mode: loader links, foreign loaders, and the resume path" node web/test_update_mode.mjs
    # (state machine) the seventeen named states: each one tied to an observation this page can make,
    # or declared unreachable with its reason -- WebMIDI cannot see the ROM mode, and the loader has no
    # separate erase or verify phase.
    run "web state: the named states, the four blind spots, and who may unplug" node web/test_state.mjs
    # authenticity: the Ed25519 manifest the page checks before it enables INSTALL, against the key
    # pinned in tools/fm1_keys.json -- and every way a tampered one must be refused
    run "web signing: Ed25519 against the pinned key, and what it refuses" node web/test_sign.mjs
    # what M-Flasher itself will check before its first erase, on the package this repository releases
    if [ -f release/felucca-2.0.0.fwsc ]; then run "web acceptance: M-Flasher's own checks and the sector plan" node web/test_accept.mjs; fi
    # (PR #73) the production pipeline against the real release package the build record names:
    # requireChip, requireProof and Updater.install refuse every partial authorisation, on the wire.
    run "web installer pipeline: the real package, and a refusal for every partial authorisation" \
        node tests/installer_pipeline_production_test.mjs
    [ -n "${PKG:-}" ] || PKG="$(ls build/felucca*.fwsc 2>/dev/null | head -1)"
    run "web package parser: malformed .fwsc refused, never loops, the built one parses" \
        node web/test_pkg_bounds.mjs $PKG
else
    echo "== skip web tests (no node)"
fi

# (v2.0.2) THE EMULATOR GROUP IS NO LONGER SILENTLY SKIPPED HERE. emcc is not on PATH in this project's own
# setup -- it lives in $HOME/emsdk and is reached by sourcing emsdk_env.sh -- so the suite used to print
# "skip the browser emulator" on a machine that has Emscripten installed, and the parity check ("= native",
# 88 200 frames bit for bit) never ran as part of the suite. A skipped group is a gap in the evidence, not a
# green result: if emcc is not on PATH but an SDK is where this repository puts it, source it and run the group.
# shellcheck source=../tools/emu_env.sh
. tools/emu_env.sh
emu_find_emcc || true

if ! command -v emcc >/dev/null 2>&1; then
    skip "browser emulator (no emcc: Emscripten builds web/emu; looked on PATH, \$EMSDK, ~/emsdk, /opt/emsdk)"
    [ "${STRICT_EMU:-0}" = 1 ] && { echo "FAIL: STRICT_EMU=1 and the emulator group did not run"; fail=1; }
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/felucca_tables.h ]; then
    skip "browser emulator (needs node and build/gen)"
    [ "${STRICT_EMU:-0}" = 1 ] && { echo "FAIL: STRICT_EMU=1 and the emulator group did not run"; fail=1; }
else
    run "browser emulator: the firmware to WebAssembly (web/emu/build.sh -> build/emu)" sh web/emu/build.sh
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/emu_native" web/emu/native_check.c -lm
    run "browser emulator: the same song from the native build" "$OUT/emu_native" "$OUT/emu_native.f32"
    run "browser emulator: boot, keys, MIDI, screen, LEDs, PLAY, a save kept across instances, = native, cost" \
        node web/emu/emu_test.mjs build/emu/felucca.wasm "$OUT/emu_native.f32"
fi

# (v1.9.0) last, so the emulator's manifest has just been refreshed: the artefacts, the version, the
# commit, the configuration and the WASM have to describe one another, and the verifier is tested too.
if [ "${NO_PACKAGE:-0}" = 1 ]; then
    echo "== skip the build manifest (NO_PACKAGE=1: no artefacts)"
else
    run "build manifest: artefacts, version, commit, config and the emulator agree (and a wrong one fails)" \
        python3 tests/manifest_test.py
fi

if [ "$n_skip" -gt 0 ]; then
    echo ""
    echo "$n_skip group(s) NOT RUN (a skip is not a pass):$skips"
    echo ""
fi
[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
