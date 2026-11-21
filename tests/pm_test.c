/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 2) The two phase-modulation paths, measured on the real DSP sources -- not on the screen.
 *
 * ANALOG's WAVE = PM: osc 2 bends osc 1's phase, the index from MORPH (+SHAPE). PHASE's LINE = PM: line 2 bends
 * line 1's phase, the index from DCW (+ENV -> DCW). Both are new values appended to an enum, so no stored sound
 * changes; what has to be proved is that they are not a drawing:
 *
 *   index    the same note at MORPH / DCW 0 and at 127 must differ, and the higher index must be brighter
 *   distinct PM must not render the same samples as SIN (ANALOG) or as MIX / RING (PHASE)
 *   bounded  no sample may leave the bus, and none may be NaN
 *
 * Brightness here is the RMS of the first difference over the RMS of the signal: a cheap, monotone proxy for
 * spectral centroid that needs no FFT and cannot be fooled by level (both terms scale together).
 *
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/pm_test tests/pm_test.c -lm */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
#include "../firmware/src/morph.c"    /* (FASE 6) params.c hands its page the morph's knobs */
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
    const preset_t *p;
    uint32_t i, n = ENGINES[e]->npresets;
    t->eng_req = (uint8_t)e;
    if (!n) {
        t->preset = 0;
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = ENGINES[e]->edit[i].def;
        t->engine = t->eng_req;
        return;
    }
    pi %= n;
    p = &ENGINES[e]->presets[pi];
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
    for (i = 0; i < NELEM(p->x); i++)
        if (p->x[i].id && p->x[i].id - 1u < P_E0)
            t->p[p->x[i].id - 1u] = (int16_t)clamp(p->x[i].v, TP[p->x[i].id - 1u].min, TP[p->x[i].id - 1u].max);
    if (!fm6_bank_read) {                        /* (engines.c includes eng_fm6.c: its statics are in scope) */
        fm6_bank_used = (1u << FM6_ROMBANK_N) - 1u;
        fm6_bank_read = host_rom_read;
    }
    fm6_track_loaded(t);
    t->engine = t->eng_req;
}

#define NBLK 400u            /* 400 * 32 samples ~ 0.29 s: steady state, past the filter's settle */
static int32_t buf[NBLK * CTL];

/* one held note; `set` runs before the note starts (wave, MORPH, LINE, ..) */
static void run_note(uint32_t e, uint32_t pi, const int16_t *set)
{
    uint32_t b, i;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    if (set)
        for (i = 0; i < 8u; i++)
            trk[0].p[P_E0 + i] = set[i];
    trk[0].p[P_LEVEL] = 90;
    trk[0].mo_cur = trk[0].mo_prev = trk[0].p[P_MORPH] << 8;
    trk_note_on(&trk[0], 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
}

static double seg_rms(const int32_t *s, uint32_t a, uint32_t b)
{
    double acc = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        acc += (double)s[i] * s[i];
    return b > a ? sqrt(acc / (double)(b - a)) : 0;
}
/* brightness: RMS of x[n]-x[n-1] over RMS of x (level cancels) */
static double seg_bright(const int32_t *s, uint32_t a, uint32_t b)
{
    double d = 0, x = seg_rms(s, a, b);
    uint32_t i;
    for (i = a + 1; i < b; i++) {
        double v = (double)s[i] - (double)s[i - 1];
        d += v * v;
    }
    d = sqrt(d / (double)(b > a + 1 ? b - a - 1 : 1));
    return x > 0 ? d / x : 0;
}
static int32_t seg_peak(const int32_t *s, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a; i < b; i++) {
        int32_t v = s[i] < 0 ? -s[i] : s[i];
        if (v > m) m = v;
    }
    return m;
}
/* the difference of two renders, over the signal's own rms */
static double rel_diff(const int32_t *a, const int32_t *b, uint32_t from, uint32_t to)
{
    double acc = 0, sig = seg_rms(a, from, to);
    uint32_t i;
    for (i = from; i < to; i++) {
        double d = (double)a[i] - (double)b[i];
        acc += d * d;
    }
    acc = sqrt(acc / (double)(to - from));
    return sig > 0 ? acc / sig : 0;
}

#define FROM (NBLK / 2u * CTL)   /* the second half of the note: the envelopes have settled */
static int32_t quiet[NBLK * CTL], loud[NBLK * CTL], other[NBLK * CTL];
static int fails, cases;

static void copy_out(int32_t *dst) { memcpy(dst, buf, sizeof buf); }

static void report(const char *what, int ok, const char *fmt, double a, double b)
{
    cases++;
    if (!ok)
        fails++;
    printf("pm: %-34s ", what);
    printf(fmt, a, b);
    printf("  %s\n", ok ? "ok" : "FAIL");
}

int main(void)
{
    int16_t s0[8], s1[8], s2[8];
    double br0, br1, d;

    printf("pm: phase modulation through the real render path (voice.c track_render -> the engines)\n");

    /* --- ANALOG WAVE = PM: MORPH is the index (SHAPE adds to it) */
    for (uint32_t i = 0; i < 8u; i++) s0[i] = s1[i] = s2[i] = 0;
    s0[0] = (int16_t)AW_PM; s0[1] = 0; s0[2] = 0; s0[3] = 0; s0[4] = 110; s0[5] = 0; s0[6] = 0; s0[7] = 0;
    s1[0] = (int16_t)AW_PM; s1[1] = 0; s1[2] = 0; s1[3] = 0; s1[4] = 110; s1[5] = 0; s1[6] = 0; s1[7] = 0;
    s2[0] = 3;              s2[1] = 0; s2[2] = 0; s2[3] = 0; s2[4] = 110; s2[5] = 0; s2[6] = 0; s2[7] = 0;  /* SIN */

    run_note(ENGI_ANALOG, 0u, s0);            /* MORPH 0 */
    copy_out(quiet);
    br0 = seg_bright(quiet, FROM, NBLK * CTL);

    host_tracks_init();
    host_preset(&trk[0], ENGI_ANALOG, 0u);
    for (uint32_t i = 0; i < 8u; i++) trk[0].p[P_E0 + i] = s1[i];
    trk[0].p[P_LEVEL] = 90;
    trk[0].p[P_MORPH] = 127;                  /* the index at its top */
    trk[0].mo_cur = trk[0].mo_prev = 127 << 8;
    trk_note_on(&trk[0], 60u, 100);
    for (uint32_t b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (uint32_t i = 0; i < CTL; i++) buf[b * CTL + i] = o[2 * i];
    }
    copy_out(loud);
    br1 = seg_bright(loud, FROM, NBLK * CTL);

    d = rel_diff(loud, quiet, FROM, NBLK * CTL);
    report("ANALOG PM: MORPH moves it", d > 0.02, "difference %.2f of the signal%.0s", d, 0);
    report("ANALOG PM: the index brightens", br1 > br0 * 1.20, "brightness %.3f -> %.3f", br0, br1);
    {
        int32_t p0 = seg_peak(loud, FROM, NBLK * CTL);
        report("ANALOG PM: bounded", p0 > 0 && p0 < (1 << 27), "peak %.0f under the bus%.0s", (double)p0, 0);
    }

    run_note(ENGI_ANALOG, 0u, s2);
    copy_out(other);
    d = rel_diff(loud, other, FROM, NBLK * CTL);
    report("ANALOG PM is not SIN", d > 0.02, "difference %.2f of the signal%.0s", d, 0);

    /* --- PHASE LINE = PM: line 2 bends line 1's phase. DCW bends the wave too, so PM is compared with MIX and
     * RING at the SAME DCW: the only difference left is the modulation, not the bend */
    int16_t q0[8], q1[8], q2[8];
    for (uint32_t i = 0; i < 8u; i++) q0[i] = q1[i] = q2[i] = 0;
    q0[0] = 3; q0[1] = 2; q0[2] = 127; q0[3] = 0; q0[4] = 50; q0[5] = 2; q0[6] = 0; q0[7] = 0; /* DSIN + SQR, LINE PM */
    q1[0] = 3; q1[1] = 2; q1[2] = 127; q1[3] = 0; q1[4] = 50; q1[5] = 0; q1[6] = 0; q1[7] = 0; /* same, LINE MIX */
    q2[0] = 3; q2[1] = 2; q2[2] = 127; q2[3] = 0; q2[4] = 50; q2[5] = 1; q2[6] = 0; q2[7] = 0; /* same, LINE RING */

    run_note(2u, 0u, q0);
    copy_out(loud);
    br1 = seg_bright(loud, FROM, NBLK * CTL);
    run_note(2u, 0u, q1);
    copy_out(quiet);
    br0 = seg_bright(quiet, FROM, NBLK * CTL);
    d = rel_diff(loud, quiet, FROM, NBLK * CTL);
    report("PHASE PM: line 2 bends line 1", d > 0.02, "difference %.2f of the signal%.0s", d, 0);
    report("PHASE PM: brighter than MIX", br1 > br0 * 1.10, "brightness %.3f -> %.3f", br0, br1);
    {
        int32_t p0 = seg_peak(loud, FROM, NBLK * CTL);
        report("PHASE PM: bounded", p0 > 0 && p0 < (1 << 27), "peak %.0f under the bus%.0s", (double)p0, 0);
    }
    run_note(2u, 0u, q2);
    copy_out(other);
    d = rel_diff(loud, other, FROM, NBLK * CTL);
    report("PHASE PM is not RING", d > 0.02, "difference %.2f of the signal%.0s", d, 0);

    printf("\npm: %u cases, %d failed\n", cases, fails);
    return fails ? 1 : 0;
}
