/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the block-timing arithmetic that FASE 1 made cheaper (firmware/src/fx.c):
 *   build/host/tempo_test        (run_tests.sh builds and runs it)
 *
 * 1. beat_samples(): the value the cache returns is the value the plain division returns, for every tempo the
 *    parameter allows (40..300), with the internal clock and with a measured MIDI tempo, including the zero
 *    MIDI tempo (no external clock has been measured yet). A stale cache would make the sequencer, the SLICER,
 *    AUTO PAN and the delay's WIDE echo run at the previous tempo: this changes the tempo mid-run and reads it
 *    back, the way the transport does.
 * 2. div_samples(): every division id 0..15, against the reference expression it replaced (DIV_DEN table).
 * 3. div_by_n(): exactly C's truncation toward zero, for the block length and for lengths that are not a power
 *    of two, over negative and positive ranges and the values the audio path actually hands it.
 *
 * It asserts values only; it measures no time. Nothing here is a claim about the FM-1. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("tempo: %-70s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the reference: what beat_samples computed before the cache, and what div_samples did before the shifts */
static uint32_t ref_beat(void)
{
    uint32_t bpm = song.g[G_BPM] >= 40 ? (uint32_t)song.g[G_BPM] : 120u;
    return song.g[G_CLOCK] && midi_beat_samples ? midi_beat_samples : (uint32_t)FS * 60u / bpm;
}
static uint32_t ref_div(uint32_t div)
{
    uint32_t quarter = ref_beat();
    return div < 6u ? quarter / DIV_DEN[div] : div < 10u ? quarter << (div - 5u) : quarter / DIV_DEN[div % 6u];
}

int main(void)
{
    uint32_t bpm, clk, mb, div, i;
    int ok = 1, beat_ok = 1, div_ok = 1, byn_ok = 1;
    static const uint32_t MIDI[4] = {0u, 1u, 22050u, 12345u};

    /* 1. the tempo cache: every setting, read twice (the second read is the cached one) */
    for (bpm = 40; bpm <= 300; bpm++) {
        song.g[G_BPM] = (int16_t)bpm;
        for (clk = 0; clk < 2u; clk++) {
            song.g[G_CLOCK] = (int16_t)clk;
            for (i = 0; i < 4u; i++) {
                midi_beat_samples = MIDI[i];
                if (beat_samples() != ref_beat()) beat_ok = 0;
                if (beat_samples() != ref_beat()) beat_ok = 0;      /* cached */
                if (beat_samples() != ref_beat()) beat_ok = 0;
            }
        }
    }
    /* and a change of tempo is seen at once, not after the old value is used again */
    song.g[G_CLOCK] = 0;
    song.g[G_BPM] = 120;
    midi_beat_samples = 0;
    (void)beat_samples();
    song.g[G_BPM] = 90;
    if (beat_samples() != ref_beat() || beat_samples() != (uint32_t)FS * 60u / 90u) beat_ok = 0;
    song.g[G_BPM] = 120;
    (void)beat_samples();
    midi_beat_samples = 9999u;                                  /* an external clock arrives */
    song.g[G_CLOCK] = 1;
    if (beat_samples() != 9999u || beat_samples() != ref_beat()) beat_ok = 0;
    song.g[G_CLOCK] = 0;                                        /* and goes away */
    midi_beat_samples = 0;
    if (beat_samples() != (uint32_t)FS * 60u / 120u || beat_samples() != ref_beat()) beat_ok = 0;
    /* BPM below 40 reads as 120, as it always did */
    song.g[G_BPM] = 0;
    if (beat_samples() != (uint32_t)FS * 60u / 120u) beat_ok = 0;
    check("beat_samples: the cache returns the division, at every tempo and clock", beat_ok);

    /* 2. every division id, across tempos */
    for (bpm = 40; bpm <= 300; bpm += 7u) {
        song.g[G_BPM] = (int16_t)bpm;
        for (div = 0; div < 16u; div++)
            if (div_samples(div) != ref_div(div)) div_ok = 0;
    }
    check("div_samples: 0..15 equal the DIV_DEN divisions at every tempo", div_ok);

    /* 3. div_by_n: exactly C's /, including the block length */
    {
        static const uint32_t N[6] = {CTL, 1u, 3u, 17u, 31u, 64u};
        static const int32_t X[] = {0, 1, -1, 31, 32, -31, -32, 33, -33, 1000, -1000, 32767, -32768,
                                    262143, -262143, 1048576, -1048576};
        uint32_t k, j;
        for (k = 0; k < 6u; k++)
            for (j = 0; j < sizeof X / sizeof X[0]; j++)
                if (div_by_n(X[j], N[k]) != X[j] / (int32_t)N[k]) byn_ok = 0;
        for (i = 0; i < 200000u; i++) {                          /* and a sweep */
            int32_t x = (int32_t)(i * 7919u) - 700000000;
            if (div_by_n(x, CTL) != x / CTL) byn_ok = 0;
            if (div_by_n(x, 17u) != x / 17) byn_ok = 0;
        }
        check("div_by_n: truncation toward zero, block length and others", byn_ok);
    }
    (void)ok;
    printf("%s\n", bad ? "tempo test FAILED" : "tempo test passed");
    return bad ? 1 : 0;
}
