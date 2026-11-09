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
#                   ellipsised), GREY gray, MONO neutral, the draw cost, the text audit (build/ui_new/text_audit.tsv);
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
# DRUM-X (tests/drumx_test.c): the DRUM engine (src/drumx_voice.c): every lane sounds, stays under full scale and ends,
#                   MORPH A <-> B (also while a hit rings), PITCH, the hat choke, MRPH on the knob; demos in build/drumx_demo/.
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
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
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
# POLY=12 builds every host test with NVOICE=12 (firmware/src/core.h): the 12-voice experiment, measured
# against the shipped 10 with exactly the same presets, goldens and CPU rows. Anything else is refused, so a
# typo cannot quietly produce a build whose voice count nobody wrote down.
case "${POLY:-10}" in
    10) ;;
    12) CC="$CC -DNVOICE=12"; echo "poly: 12 voices (NVOICE=12; the shipped build is 10)" ;;
    *)  echo "poly: POLY=${POLY:?} (the measured profiles are 10 and 12)" >&2; exit 2 ;;
esac
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM and SAMPLE PERC -> DRUM, grid records, DIGITAL kept)" "$OUT/upreset_test"

$CC -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"

$CC -o "$OUT/flash_id_test" tests/flash_id_test.c
run "flash identification: allowlist, two agreeing reads, 1 MiB capacity, protection -> RAM-only" "$OUT/flash_id_test"

$CC -o "$OUT/mc_selftest_test" tests/mc_selftest_test.c
run "second core: the boot self-test refuses a misread shared mailbox" "$OUT/mc_selftest_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
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
    [ -f build/felucca.fwsc ] || { echo "run ./build.sh first (or NO_PACKAGE=1 for the host-only run)"; exit 1; }

    $CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
    run "M-UPGRADE entry (own loader)" "$OUT/ota_test" build/felucca.fwsc

    head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
    python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
    $CC -o "$OUT/ldr_test" tests/ldr_test.c
    run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc
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
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/morph_audible_test" tests/morph_audible_test.c -lm
    run "MORPH moves the samples: every engine with a morph path renders a different waveform when P_MORPH moves under a held note (and without a click)" "$OUT/morph_audible_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/morph_six_test" tests/morph_six_test.c -lm
    run "MORPH under load: six parts, the FX buses, MIDI notes and a preset load -- every transition measured (latency, slew to 90%, click) with the two renders bit-identical before the turn" "$OUT/morph_six_test"
    run "the particle pool bays: EDIT ribbon draws none, HOME and the visualizer keep theirs inside their own rect, no allocation" python3 tests/particle_bounds_test.py
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords, drum hits), gates, chance, swing, projects, user presets, CHANCE page" "$OUT/ratchet_test"
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
    run "UI: DICE ALL on its own, 50 throws in a row in one process (no state left by the test before)" \
        env UI_ONLY=dice_all UI_REPEAT_DICE=50 "$OUT/ui_test"
    FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    mkdir -p build/ui_new/ppm build/ui_slot
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), GREY gray, MONO neutral, alignment by ink (1 px fails), draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    $CC -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test_fm4" tests/ui_test.c -lm
    run "UI built with FELUCCA_FM4=1 (DIGITAL, kept in the tree): its OP pages, EDIT cycle, algorithm charts" "$OUT/ui_test_fm4"
    $CC -O1 -w -DFELUCCA_FM4=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render_fm4" tests/ui_render.c -lm
    mkdir -p build/ui_fm4/ppm build/ui_fm4_slot
    run "UI renders with FELUCCA_FM4=1: DIGITAL's screens (EDIT, OP ENV, the 8 algorithm charts), lint, GREY gray, MONO neutral" \
        "$OUT/ui_render_fm4" build/ui_fm4 build/ui_fm4_slot
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/master_test" tests/master_test.c -lm
    run "FX > MASTER: CLIP, PUNCH, DUCK and DREL each change the rendered mix" "$OUT/master_test"
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
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
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
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/cloud_hot_test" tests/cloud_hot_test.c -lm
    run "CLOUD driven hot (JIANT): the loop never runs away, its line never clamps, no grain across the write head, SIZE 127 from silence" "$OUT/cloud_hot_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_test" tests/drumx_test.c -lm
    mkdir -p build/drumx_demo
    run "DRUM-X: every lane sounds and ends, MORPH, PITCH, NOIS, hat choke, group mutes, demos" "$OUT/drumx_test" build/drumx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
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
for t in eq_dc_test eq_response_test eq_state_test eq_sweep_test six_track_test six_track_live_test; do
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
run "signed packages: manifest, hash, product / version / min_firmware, WebCrypto parity" \
    python3 tests/sign_test.py
# (PR #73) the gate's refusal matrix and the backup audit: both import tools/fm1_gate.py and
# tools/fm1_image.py and assert on the real refusal codes, so a regression in either fails here.
run "install gate: read-only chips, unknown JEDEC, model and identity refusals before any write" \
    python3 tests/negative_write_test.py
run "backup audit: region bounds, bootloader never written, dump and restore evidence agree" \
    python3 tests/backup_audit_test.py
if [ "${NO_PACKAGE:-0}" = 1 ]; then
    echo "== skip installer preflight (NO_PACKAGE=1: needs build/felucca.fwsc)"
else
    run "installer preflight: read-only, refuses before the first write frame" python3 tests/preflight_test.py
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
        run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc
        $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
        run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" build/felucca.fwsc
    fi
    for t in editor_test project_test backup_test robust_test fm6bank_test; do
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
fi

run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt
run "target memory map: 1 MiB flash, RAM / pool / stacks / noinit do not overlap" python3 tests/memory_map_test.py

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

run "preset library: 200 factory sounds, per-engine counts, DRUM's ten indices, names" python3 tests/preset_library_test.py
run "signing chain: Ed25519 manifests and every refusal" python3 tests/sign_test.py
run "key custody: keygen off-repo, pin --replace, check, rotation, revocation" python3 tests/key_custody_test.py
run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py
run "backup tool (fm1_archive.py): two independent passes, manifest, classification, verified restore" \
    python3 tests/backup_tool_test.py
run "README navigation contract (WEB SITE first and double width, names, links, palette)" \
    python3 tests/readme_nav_test.py

if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol, package builder, update protocol" node web/test_web.mjs
    # (v1.9.0) the installer's build provenance: no write without a verified record, and the site
    # builder refuses a package that contradicts its own manifest
    # the provenance test below refuses a site whose emulator and firmware name different commits, and until now a
    # forgotten `sh web/emu/build.sh` surfaced as two red make_site lines. Say it plainly first; the verdict stays
    # with the test, not with this line.
    python3 tools/emu_stamp.py || true
    run "web installer: build provenance, zero writes when it fails" node web/test_provenance.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
    # (1.9) Verify Only is a read: the real Updater against a simulated FM-1, every frame it receives
    # counted -- handshake and one INFO read, never UPGRADE, a PUT or a resume
    run "web verify-only: identity reads only, zero write frames" node web/test_verify_only.mjs
    # (first install) the chip fact a stock unit cannot give over MIDI: the record M-Flasher wrote,
    # its dump hashed in the page, and the donor install path that accepts them -- with the wire
    # assertions that nothing is sent without them.
    run "web chip record: M-Flasher record + dump, and what it does not authorise" node web/test_record.mjs
    # (first install) a unit sitting in its update mode answers the handshake and not 0x12, so the
    # chip gate on the loader link comes from the same attempt or from the record -- and a unit's own
    # update mode is never fed a Felucca image.
    run "web update mode: loader links, foreign loaders, and the resume path" node web/test_update_mode.mjs
    # (PR #73) the production pipeline against the real release package the build record names:
    # requireChip, requireProof and Updater.install refuse every partial authorisation, on the wire.
    run "web installer pipeline: the real package, and a refusal for every partial authorisation" \
        node tests/installer_pipeline_production_test.mjs
    PKG=""
    if [ -f build/felucca.fwsc ]; then PKG=build/felucca.fwsc; fi
    run "web package parser: malformed .fwsc refused, never loops, the built one parses" \
        node web/test_pkg_bounds.mjs $PKG
else
    echo "== skip web tests (no node)"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "== skip the browser emulator (no emcc: Emscripten builds web/emu)"
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the browser emulator (needs node and build/gen)"
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
    # the real package, its real record and a signed release manifest through the transport gate: this
    # is the only test that refuses to invent its artefacts, so it needs the build above
    if command -v node >/dev/null 2>&1; then
        run "release e2e: the built package, its record and a signed manifest agree end to end" \
            node web/test_release_e2e.mjs
    fi
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }
