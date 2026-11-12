/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
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

/* the same parameters twice in one engine: a preset that is a copy of another */
static int same_as(const preset_t *a, const preset_t *b)
{
    uint32_t i;
    if (memcmp(a->e, b->e, sizeof a->e) || memcmp(a->env, b->env, sizeof a->env) || a->fenv != b->fenv ||
        a->mono != b->mono || memcmp(a->fx, b->fx, sizeof a->fx))
        return 0;
    for (i = 0; i < NELEM(a->x); i++)
        if (a->x[i].id != b->x[i].id || a->x[i].v != b->x[i].v)
            return 0;
    return 1;
}

static int fails;
static void fail(const char *fmt, ...) { (void)fmt; fails++; }

int main(void)
{
    uint32_t e, i, k;
    uint32_t total = 0, silent = 0, hot_presets = 0, dup_names = 0, dup_params = 0, out_of_range = 0;
    printf("preset audit: every factory preset through the real load and render (host x86, %u Hz)\n", (unsigned)FS);
    printf("%-8s %6s %7s %6s %8s %8s %6s\n", "engine", "presets", "silent", "hot", "dupn", "dupp", "range");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        uint32_t n = en->npresets, e_silent = 0, e_hot = 0, e_dupn = 0, e_dupp = 0, e_range = 0;
        if (!n)
            continue;
        for (i = 0; i < n; i++) {
            const preset_t *p = &en->presets[i];
            int32_t peak;
            double rms, hot;
            total++;
            /* structure: the eight EDIT values and the four envelope stages inside their own ranges */
            for (k = 0; k < 8u; k++)
                if (p->e[k] < en->edit[k].min || p->e[k] > en->edit[k].max) {
                    e_range++;
                    printf("  range: %-8s %-16s E%u = %d outside %d..%d\n", en->name, p->name, (unsigned)k,
                           (int)p->e[k], (int)en->edit[k].min, (int)en->edit[k].max);
                }
            if (!p->name[0] || p->env[0] > 127 || p->env[1] > 127 || p->env[2] > 127 || p->env[3] > 127) {
                e_range++;
                printf("  range: %-8s %-16s name empty or an envelope stage past 127\n", en->name, p->name);
            }
            for (k = 0; k < i; k++)
                if (strcmp(p->name, en->presets[k].name) == 0) e_dupn++;
            for (k = 0; k < i; k++)
                if (same_as(p, &en->presets[k])) {
                    e_dupp++;
                    printf("  duplicate: %-8s %-16s has the parameters of %s\n", en->name, p->name, en->presets[k].name);
                }
            render_preset(e, i, &peak, &rms, &hot);
            if (rms < 8.0) {
                e_silent++;
                printf("  silent: %-8s %-16s rms %.1f, peak %d\n", en->name, p->name, rms, (int)peak);
            }
            if (hot > 0.02) {
                e_hot++;
                printf("  hot:    %-8s %-16s %.0f%% of the note in the clipper's last stretch (peak %d)\n",
                       en->name, p->name, hot * 100.0, (int)peak);
            }
        }
        printf("%-8s %6u %7u %6u %8u %8u %6u\n", en->name, n, e_silent, e_hot, e_dupn, e_dupp, e_range);
        silent += e_silent; hot_presets += e_hot; dup_names += e_dupn; dup_params += e_dupp; out_of_range += e_range;
    }
    printf("\ntotal %u presets: %u silent, %u hot (reported, not failed), %u repeated names, %u exact duplicates, "
           "%u out of range\n", total, silent, hot_presets, dup_names, dup_params, out_of_range);
    if (silent || dup_names || out_of_range)
        fails += (int)(silent + dup_names + out_of_range);
    printf("preset audit: %s\n", fails ? "FAILED" : "passed (structure and signal; nobody listened)");
    return fails ? 1 : 0;
}
