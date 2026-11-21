/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (ETAPA 3) AUDIO STRESS: the firmware at its worst, checked for what a listener would notice.
 *
 * Every other audio test here renders one engine at a time or one golden scene. This one holds the whole box at
 * once and keeps it there for minutes of material: six parts on different engines, sixteen keys down (the voice
 * pool granting every slot it can), every FX bus fed (chorus, delay, reverb, DIST on the parts), the sequencer
 * driving chords, MIDI arriving while it plays (notes, note-offs, program changes, CCs), and the engines being
 * switched under the notes -- the change a user makes while it sounds. It measures the output, not the source:
 *
 *   peak        the loudest sample, both channels (1.0 is full scale; anything past it is clipping, and the
 *               master's leveler is what should stop it)
 *   dc          the mean of each channel over the run: a DC offset eats headroom and thumps on every gate
 *   jump        the largest sample-to-sample step, and how many steps pass 0.5 -- a click is a step no
 *               oscillator or envelope makes, and it is the one artifact a golden hash cannot name
 *   nan/inf     any non-finite sample at all, which is what a broken filter or a bad preset switch produces
 *   cpu         per-block time, p50 / p99 / max, as a ratio of the 725 us the device has (HOST x86-64: it
 *               orders and it shows the tail, it does not predict the FM-1)
 *
 *   tests/audio_stress [seconds]      (default 20 s of audio; the suite runs 8)
 *
 * Fails on: non-finite samples, a peak over the ceiling, a DC offset over the limit, or a step over the click
 * limit. Prints the rest. HOST class: the harness is the emulator's translation unit with the LCD and the flash
 * modelled in memory; nothing here is a reading from a unit.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/audio_stress tests/audio_stress.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../web/emu/felucca_web.c"

#define CEIL_PEAK 1.5           /* the leveler holds ~-0.3 dBFS; 1.5 is already a broken gain stage */
#define CEIL_DC   0.02          /* 0.34 dB of headroom gone, or a gate that thumps */
#define CEIL_JUMP 0.9           /* a full-scale flip in one sample: no voice, FX or glide makes one */
#define BUDGET_S  (725.0 / 1e6) /* 32 samples at 44.1 kHz, AGENTS.md §8 */

static double cpu_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static const uint8_t ENG[6] = {4, 12, 6, 5, 10, 7};   /* ANALOG, FM6, PHASE, VOICE, LOFI, NOISE */
static const uint8_t SCALE[16] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71, 72, 74};

int main(int argc, char **argv)
{
    uint32_t seconds = argc > 1 ? (uint32_t)atoi(argv[1]) : 20u, p, k, b, nblk, seed = 0x1234u;
    double t_cpu[60000], peak_l = 0, peak_r = 0, dc_l = 0, dc_r = 0, jump = 0, sum_l = 0, sum_r = 0;
    uint32_t jumps_hot = 0, clipped = 0, nonfinite = 0, frames = 0, nclk = 0, over = 0;
    float last_l = 0.0f, last_r = 0.0f, prev_l = 0.0f, prev_r = 0.0f;
    int fails = 0;

    if (seconds < 1u) seconds = 1u;
    if (seconds > 600u) seconds = 600u;
    nblk = seconds * 44100u / 32u;
    if (nblk > 60000u) nblk = 60000u;                    /* the timing array's size; the run is capped */

    web_seed(7);
    web_boot();
    web_keys(0);
    web_render(4096);                                    /* the box idle: every voice free */
    web_test_heavy();                                    /* the sequencer's material on three parts + drums */
    for (p = 0; p < 6u; p++) {                           /* six engines, six parts */
        web_test_engine(ENG[p]);
        web_midi(0x90u | (p + 1u), (uint32_t)SCALE[p * 2u % 16u], 100u);
        for (k = 0; k < 4u; k++)                         /* and four more each: the pool at its limit */
            web_midi(0x90u | (p + 1u), (uint32_t)SCALE[(k * 3u + p) % 16u] + 12u, 96u);
        web_midi(0xB0u | (p + 1u), 91u, 120u);           /* every part's reverb send up */
        web_midi(0xB0u | (p + 1u), 93u, 100u);           /* and its chorus */
    }
    for (b = 0; b < nblk; b++) {
        double t0 = cpu_s(), t1;
        uint32_t i;
        web_render(32);
        t1 = cpu_s();
        if (nclk < 60000u)
            t_cpu[nclk++] = t1 - t0;
        for (i = 0; i < 32u; i++) {
            float l = web_out_l()[i], r = web_out_r()[i];
            double al = fabs((double)l), ar = fabs((double)r), dl, dr;
            if (!isfinite(l) || !isfinite(r)) {
                nonfinite++;
                continue;
            }
            if (al > peak_l) peak_l = al;
            if (ar > peak_r) peak_r = ar;
            if (al > 1.0 || ar > 1.0) clipped++;
            sum_l += l;
            sum_r += r;
            frames++;
            if (frames > 1u) {
                dl = fabs((double)l - (double)prev_l);
                dr = fabs((double)r - (double)prev_r);
                if (dl > jump) jump = dl;
                if (dr > jump) jump = dr;
                if (dl > 0.5 || dr > 0.5) jumps_hot++;
            }
            prev_l = l;
            prev_r = r;
        }
        last_l = web_out_l()[31];
        last_r = web_out_r()[31];
        /* the user's own moves, on the grid: a preset under the notes, MIDI while it plays */
        if ((b % 64u) == 63u) {                          /* ~46 ms: an engine switch under held notes */
            web_test_engine(ENG[(b / 64u) % 6u]);
            web_midi(0xC0u, ENG[(b / 64u) % 6u], 0u);
        }
        if ((b % 16u) == 15u) {                          /* a stream of MIDI: notes and their offs */
            seed = seed * 1664525u + 1013904223u;
            web_midi(0x90u | (1u + (seed >> 8) % 6u), 36u + (seed >> 16) % 60u, 80u + (seed >> 24) % 47u);
        }
        if ((b % 16u) == 7u) {
            seed = seed * 1103515245u + 12345u;
            web_midi(0x80u | (1u + (seed >> 8) % 6u), 36u + (seed >> 16) % 60u, 0u);
            web_midi(0xB0u | 1u, 74u, (seed >> 20) & 127u);   /* CUTOFF: modulation intensity under load */
            web_midi(0xB0u | 2u, 71u, (seed >> 12) & 127u);
        }
        if ((b % 512u) == 511u) {                        /* keep it loud: retrigger the held chord */
            for (p = 0; p < 6u; p++)
                web_midi(0x90u | (p + 1u), (uint32_t)SCALE[(b / 512u) % 16u] + 12u, 100u);
        }
    }
    dc_l = frames ? sum_l / (double)frames : 0.0;
    dc_r = frames ? sum_r / (double)frames : 0.0;
    qsort(t_cpu, nclk, sizeof(double), cmp_d);
    for (b = 0; b < nclk; b++)
        if (t_cpu[b] > BUDGET_S)
            over++;
    printf("audio_stress: host x86-64, %u blocks (%.1f s of audio), 6 engines, 6 parts, every bus fed\n",
           nblk, (double)frames / 44100.0);
    printf("  peak    L %.3f  R %.3f   (%.0f frames past full scale)\n", peak_l, peak_r, (double)clipped);
    printf("  dc      L %+.4f  R %+.4f\n", dc_l, dc_r);
    printf("  jump    %.3f in one sample, %u steps over 0.5\n", jump, jumps_hot);
    printf("  nonfinite %u samples\n", nonfinite);
    printf("  cpu     p50 %.1f us  p99 %.1f us  max %.1f us of the %.1f us budget (%.0f %% over, host)\n",
           t_cpu[nclk / 2] * 1e6, t_cpu[(nclk * 99u) / 100u] * 1e6, t_cpu[nclk - 1] * 1e6, BUDGET_S * 1e6,
           100.0 * (double)over / (double)(nclk ? nclk : 1u));
    (void)last_l;
    (void)last_r;
    if (nonfinite) {
        printf("  FAIL  %u non-finite samples\n", nonfinite);
        fails++;
    }
    if (peak_l > CEIL_PEAK || peak_r > CEIL_PEAK) {
        printf("  FAIL  peak %.3f over the ceiling %.1f\n", peak_l > peak_r ? peak_l : peak_r, CEIL_PEAK);
        fails++;
    }
    if (fabs(dc_l) > CEIL_DC || fabs(dc_r) > CEIL_DC) {
        printf("  FAIL  dc offset past %.2f\n", CEIL_DC);
        fails++;
    }
    if (jump > CEIL_JUMP) {
        printf("  FAIL  a %.3f step in one sample (click ceiling %.1f)\n", jump, CEIL_JUMP);
        fails++;
    }
    printf(fails ? "audio_stress: FAILED (%d)\n" : "audio_stress: all checks passed\n", fails);
    return fails ? 1 : 0;
}
