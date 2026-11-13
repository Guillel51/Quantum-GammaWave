/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5) Every EDIT knob of every engine, measured: does turning it change a single sample?
 *
 * The library audit (tests/preset_audit_test.c) proves a preset sounds and is not a copy. This one asks the
 * other question a player asks with their hands: is there a knob that does nothing? For each engine, for a
 * few of its presets, each EDIT index is forced to the descriptor's minimum and then to its maximum, the
 * same note is held and the same block is rendered, and the two buffers are compared sample by sample.
 *
 *   identical   the whole rendered note is bit-identical at both ends of the knob. That is a knob with no
 *               effect *for that preset*: either the engine never reads it, or it only matters with something
 *               else switched on, which the report says (a finding, not automatically a bug).
 *   differ      the knob moves the sound; the report keeps how far (the peak difference), so a knob worth
 *               +/-1 LSB can be told from one that rewrites the timbre.
 *
 * It cannot say whether the movement is beautiful, or whether the range is musical: that needs the unit.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/engine_param_test tests/engine_param_test.c -lm
 *   build/host/engine_param_test
 */
/* (5.0) The factory library, audited through the real load and the real render: not by ear and not by eye.
 *
 * For every preset of every engine this loads the sound the way a user's PRESETS knob does (host_preset: the
 * same fields apply_preset_to writes), holds a note on the engine's own key, and measures the samples the mix
 * produces:
 *
 *   structure   every one of P_E0..P_E7 inside its descriptor's min..max, and the four envelope stages inside
 *               0..127. A preset that ships out of range is a load that silently clamps, or a page that lies
 *               about the value it shows.
 *   silence     the RMS of the sounding part of the note. A factory sound that produces nothing is a broken
 *               preset, however good its numbers look; the floor here is 8 (a 16-bit LSB is 1, so a real pad
 *               is two orders of magnitude above it).
 *   level       the peak and how much of the note sits in the soft clipper's last stretch (|x| >= 32000, the
 *               tanh's own ceiling is 32767). This is REPORTED, not failed: a LOFI square wave is meant to sit
 *               on the rails, and deciding which of those are too hot is a listening decision, not arithmetic.
 *   identity    an empty name, a name repeated inside one engine, and two presets with identical parameters
 *               (a duplicate sounds exactly like its twin: the library is inflated by one).
 *
 * What this does NOT do: it does not listen. It proves the sound exists, stays in range and is not a copy of
 * its neighbour; it cannot say whether a pad is beautiful, or whether a preset that renders fine sounds bad.
 * Any claim about how the library SOUNDS needs the FM-1 and a pair of ears.
 *
 *   tests/run_tests.sh builds and runs it; by hand:
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/preset_audit_test tests/preset_audit_test.c -lm */
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

/* the setup tests/morph_audible_test.c uses, kept minimal: factory defaults and an empty pattern per track */
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
static void host_rom_bank(void) { fm6_bank_used = (1u << FM6_ROMBANK_N) - 1u; fm6_bank_read = host_rom_read; }
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
    if (!fm6_bank_read)
        host_rom_bank();
    fm6_track_loaded(t);
    t->engine = t->eng_req;
}

#define NBLK 320u                                  /* 320 x 32 samples ~ 0.23 s, past every attack */
static int32_t buf[NBLK * CTL];

/* one preset, one held note; returns 0 and fills peak/rms/hot */
static void render_preset(uint32_t e, uint32_t pi, int32_t *peak, double *rms, double *hot)
{
    uint32_t b, i, n = 0, hot_n = 0;
    double acc = 0;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_LEVEL] = 100;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    trk_note_on(&trk[0], e == ENGI_DRUM ? 36u : 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
    *peak = 0;
    for (i = (NBLK / 3u) * CTL; i < NBLK * CTL; i++) {     /* the sounding part: the attack is behind us */
        int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
        acc += (double)buf[i] * buf[i];
        n++;
        if (a > *peak) *peak = a;
        if (a >= 32000) hot_n++;                          /* inside the soft clipper's last stretch */
    }
    *rms = n ? sqrt(acc / (double)n) : 0;
    *hot = n ? (double)hot_n / (double)n : 0;
}


/* one preset, one held note, EDIT index k forced to v; the whole rendered block goes to `out` */
static void render_edit(uint32_t e, uint32_t pi, int32_t k, int32_t v)
{
    uint32_t b, i;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_LEVEL] = 100;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    if (k >= 0)
        trk[0].p[P_E0 + k] = (int16_t)v;
    trk_note_on(&trk[0], e == ENGI_DRUM ? 36u : 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
}

static int32_t buf2[NBLK * CTL];

/* (FASE 5) `--levels`: the same render over the whole factory bank, as RMS in dBFS, so a sound that sits
 * 10 dB below its neighbours is a number instead of an impression. Reported, not failed: a pad and a kick are
 * not supposed to peak alike, but a lead 8 dB under the rest of the leads is a preset that needs a level. */
static void levels(void)
{
    uint32_t oi, i;
    printf("preset levels: every factory preset through the real render, RMS of its sounding part (host x86)\n");
    for (oi = 0; oi < NENG_SHOWN; oi++) {
        uint32_t e = ENGINE_ORDER[oi];
        const engine_t *en = ENGINES[e];
        double db[128], med, lo, hi;
        int32_t n = 0, k;
        if (en->npresets == 0 || en == &ENG_GONE || en->npresets > 128u)
            continue;
        for (i = 0; i < en->npresets; i++) {
            int32_t peak;
            double rms, hot;
            render_preset(e, i, &peak, &rms, &hot);
            db[n++] = rms > 0 ? 20.0 * log10(rms / 32768.0) : -120.0;
        }
        for (k = 1; k < n; k++) {                        /* the engine's median, insertion sort: n <= 128 */
            double v = db[k];
            int32_t j = k - 1;
            while (j >= 0 && db[j] > v) { db[j + 1] = db[j]; j--; }
            db[j + 1] = v;
        }
        med = db[n / 2];
        lo = db[0];
        hi = db[n - 1];
        printf("  %-7s %2d presets: median %5.1f dBFS, from %5.1f to %5.1f dBFS\n", en->name, n, med, lo, hi);
        for (i = 0; i < en->npresets; i++) {             /* the ones that sit far from their own family */
            int32_t peak;
            double rms, hot;
            render_preset(e, i, &peak, &rms, &hot);
            if (rms <= 0)
                continue;
            {
                double d = 20.0 * log10(rms / 32768.0);
                if (d < med - 6.0 || d > med + 6.0)
                    printf("    %-7s #%-2u %-12s %5.1f dBFS (%+.1f vs the engine)\n", en->name, (unsigned)i,
                           en->presets[i].name, d, d - med);
            }
        }
    }
}

int main(int argc, char **argv)
{
    uint32_t oi, k, p, pi, worst_engine = 0;
    int32_t noeffect = 0, checked = 0;
    if (argc > 1 && !strcmp(argv[1], "--levels")) {
        levels();
        return 0;
    }
    printf("engine param audit: every EDIT knob at both ends of its range, through the real render (host x86)\n");
    for (oi = 0; oi < NENG_SHOWN; oi++) {
        uint32_t e = ENGINE_ORDER[oi];
        const engine_t *en = ENGINES[e];
        if (en->npresets == 0 || en == &ENG_GONE)
            continue;
        for (k = 0; k < 8u; k++) {
            const param_desc_t *d = &en->edit[k];
            int32_t bad = 1, span = 0;
            if (d->max <= d->min)
                continue;
            for (p = 0; p < 3u; p++) {                     /* three presets of this engine: first, middle, last */
                pi = p == 0 ? 0u : p == 1 ? en->npresets / 2u : en->npresets - 1u;
                render_edit(e, pi, (int32_t)k, d->min);
                memcpy(buf2, buf, sizeof buf);
                render_edit(e, pi, (int32_t)k, d->max);
                checked++;
                if (memcmp(buf2, buf, sizeof buf) != 0) {
                    int32_t i, df = 0;
                    for (i = 0; i < (int32_t)(NBLK * CTL); i++) {
                        int32_t a = buf[i] - buf2[i];
                        a = a < 0 ? -a : a;
                        if (a > df) df = a;
                    }
                    if (df > span) span = df;
                    bad = 0;
                }
            }
            if (bad) {
                noeffect++;
                printf("  NO EFFECT  %-7s %-11s (%s) %d..%d: the note is bit-identical at both ends\n",
                       en->name, d->label, en->page_title[0], d->min, d->max);
                worst_engine++;
            } else if (span <= 2) {
                printf("  FAINT      %-7s %-11s (%s) %d..%d: moves the sound by %d LSB\n",
                       en->name, d->label, en->page_title[0], d->min, d->max, span);
            }
        }
    }
    printf("engine param audit: %d knobs x 3 presets checked, %d with no effect at all%s\n",
           (int)checked, (int)noeffect, noeffect ? "" : " (none)");
    return noeffect ? 1 : 0;
}
