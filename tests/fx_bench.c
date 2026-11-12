/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (7.0) A bench for the whole audio block, so a change to the DSP path can be compared with a number instead of a
 * feeling. It renders the same material every run: one ANALOG chord on a part, a second part on FM6, the drum kit on
 * a third, every FX bus fed (reverb, delay, chorus, distortion), and the sequencer playing -- i.e. mix_block doing
 * what it does on the device, ~34 500 blocks per rendered second.
 *
 *   tests/fx_bench [repeats]
 *
 * Prints, per repeat, the wall time of NBLOCKS blocks as ns per sample and as a ratio against the reported 725 us
 * budget for 32 samples (the budget AGENTS.md gives; the ratio is a HOST figure and says nothing about the FM-1).
 * The best (minimum) repeat is printed last: it is the least noisy estimate a shared machine can give.
 *
 * tests/fx_bench.c is not part of the suite's pass/fail gate: it does not assert, it measures. tests/run_tests.sh
 * builds it so a later run can reproduce the same numbers on the same machine. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../firmware/src/core.h"
#include "../firmware/src/engines.c"
#include "../firmware/src/params.c"
#include "../firmware/src/mod.c"
#include "../firmware/src/voice.c"
#include "../firmware/src/slicer.c"
#include "../firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../firmware/src/usb.c"
#include "../firmware/src/midi_uart.c"
#include "../firmware/src/song_chain.c"
#include "../firmware/src/seq.c"

#ifndef HOSTSIM_FLASH_ID
#define HOSTSIM_FLASH_ID 1
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static uint32_t fl_status_ram(void) { return 0u; }
#endif

static void host_tracks_init(void)
{
    uint32_t i, k;
    voices_boot();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
    song.master_q12 = 4096;
    trk_live = NTRK;
}
static int host_rom_read(uint32_t k, uint8_t *pk)
{
    if (k >= FM6_ROMBANK_N) return 1;
    memcpy(pk, FM6_ROMBANK[k], FM6_PACKED);
    return 0;
}
static void host_preset(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const engine_t *en = ENGINES[e];
    const preset_t *p;
    uint32_t i, n = en->npresets;
    t->eng_req = (uint8_t)e;
    if (!n) {
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = en->edit[i].def;
        t->engine = t->eng_req;
        return;
    }
    pi %= n;
    p = &en->presets[pi];
    t->preset = (uint8_t)pi;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = p->e[i];
    t->p[P_ATK] = p->env[0];
    t->p[P_DEC] = p->env[1];
    t->p[P_SUS] = p->env[2];
    t->p[P_REL] = p->env[3];
    t->p[P_ED_FLT] = p->fenv;
    t->p[P_VOICE] = p->mono ? V_LEGATO : V_POLY;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = (int16_t)(p->fx[i] ? p->fx[i] - 1 : FX_DEF[i]);
    if (!fm6_bank_read) {
        fm6_bank_used = (1u << FM6_ROMBANK_N) - 1u;
        fm6_bank_read = host_rom_read;
    }
    fm6_track_loaded(t);
    t->engine = t->eng_req;
}

#define NBLOCKS 4000u
static int32_t out[2 * CTL];

/* sc = 0: every part feeds all three buses (the stress case). sc = 1: the same six parts with the sends
 * at 0 -- a dry patch, the case where the send path must cost nothing. sc = 2: (4.0) the same stress case
 * with the chorus bus in FLANGER, so the new mode's cost is measured against the ensemble's on the same run. */
static void load_scene(uint32_t sc)
{
    uint32_t i;
    host_tracks_init();
    host_preset(&trk[0], 0, 0);                       /* ANALOG */
    host_preset(&trk[1], FELUCCA_FM4 ? ENGI_FM6 : ENGI_FM6, 0);
    host_preset(&trk[2], ENGI_DRUM, 0);
    for (i = 0; i < 3u; i++) {
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_SUS] = 120;
        trk[i].p[P_DIST] = 40;                        /* every bus fed */
        trk[i].p[P_CHOR] = sc == 1u ? 0 : 60;
        trk[i].p[P_DLY] = sc == 1u ? 0 : 70;
        trk[i].p[P_REV] = sc == 1u ? 0 : 80;
    }
    song.g[G_DMIX] = 80;                              /* the delay's wet, on the bus */
    song.g[G_CTYPE] = sc == 2u ? 2 : 0;               /* (4.0) 0 the ensemble, 2 the FLANGER */
    song.g[G_CDEPTH] = 100;
    for (i = 0; i < 4u; i++) {                        /* a sixteen-step pattern on each part */
        trk[i].step[i * 2u].time = ST_NOTE;
        trk[i].step[i * 2u].n = i < 2u ? 3u : 1u;
        trk[i].step[i * 2u].note[0] = (uint8_t)(48u + i * 5u);
        trk[i].step[i * 2u].note[1] = (uint8_t)(55u + i * 5u);
        trk[i].step[i * 2u].note[2] = (uint8_t)(60u + i * 5u);
        trk[i].step[i * 2u].vel = 100;
        if (i == 2u) trk[i].step[i * 2u].hit = 1u;    /* the kit's kick */
    }
}
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
/* (FASE 1) process CPU time as well as the wall clock: on a shared VM the wall clock moves with whatever else is
 * running, while CPU time only counts this process, so the same change can be resolved below the scheduler's own
 * noise. Both are printed; neither says anything about the FM-1. */
static double now_cpu_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    uint32_t r, reps = argc > 1 ? (uint32_t)atoi(argv[1]) : 5u, b, i, sc;
    static const char *const SCENE[3] = {"every bus fed", "dry parts (sends 0)", "every bus fed, CHORUS in FLANGER"};
    double best[3] = {1e9, 1e9, 1e9}, best_cpu[3] = {1e9, 1e9, 1e9};
    double budget = 725e-6 / 32.0;                    /* ns of budget per sample (AGENTS.md, 32-sample block) */
    printf("fx_bench: host x86, %u Hz, %u blocks of %u samples per repeat\n", (unsigned)FS, NBLOCKS, (unsigned)CTL);
    for (sc = 0; sc < 3u; sc++) {
        printf(" scene %u: %s\n", sc, SCENE[sc]);
        for (r = 0; r < reps; r++) {
            double t0, t1, ns, c0, c1, nsc;
            load_scene(sc);
            seq_start();
            for (b = 0; b < 64u; b++) mix_block(out, CTL);         /* warm: the loads, the buses, the lines */
            t0 = now_s();
            c0 = now_cpu_s();
            for (b = 0; b < NBLOCKS; b++) mix_block(out, CTL);
            t1 = now_s();
            c1 = now_cpu_s();
            ns = (t1 - t0) * 1e9 / (double)(NBLOCKS * CTL);
            nsc = (c1 - c0) * 1e9 / (double)(NBLOCKS * CTL);
            if (ns < best[sc]) best[sc] = ns;
            if (nsc < best_cpu[sc]) best_cpu[sc] = nsc;
            printf("  repeat %u: %.1f ns per sample wall, %.1f ns CPU (%.2f x the 725 us budget, wall)\n",
                   r + 1u, ns, nsc, (ns * 1e-9) / budget);
            for (i = 0; i < 2u * CTL; i++) out[i] = 0;
        }
        printf(" best of %u: %.1f ns per sample wall, %.1f ns CPU\n", reps, best[sc], best_cpu[sc]);
    }
    return 0;
}
