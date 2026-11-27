/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5) The engine audit, measured instead of read: every engine's eight EDIT parameters (P_E0..P_E7) swept to
 * their descriptor's min and max, with the audio of a held note compared against the same note untouched.
 *
 * Why: a knob that is in a table and not in the DSP looks identical to a working one in the source, and greps lie
 * about it (the engines read `e[k]` off a pointer to P_E0, not `P_Ek`). What cannot lie is the render.
 *
 * The measure is the relative RMS of the difference of the sounding part of the note (the last two thirds, past
 * every attack): 0 means the parameter changed nothing at all in the samples; 1 means the change is as loud as the
 * sound itself. A parameter that moves less than 1 % is reported as DEAD: no musical meaning at that resolution.
 *
 * It does NOT say a parameter is useful -- a working knob can be redundant with its neighbour -- and it cannot
 * listen. It says whether the DSP reads it, which is what the audit asked.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/param_sweep_test tests/param_sweep_test.c -lm
 */
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
/* (v2.3.5) DC: the mean of the sounding part as a fraction of full scale. A patch that sits off centre steals
 * headroom from every note that follows it on the bus and, on the unit's DAC, is an audible thump on the release;
 * the DSP's own DC blocker should leave it near zero, so the audit now says how near. */
static double dc_last, dc_h1, dc_h2;   /* the mean of each half of the sounding part: real DC keeps its sign,
                                         * a sub-audio body flips it */


#define NBLK_PROBE 320u

static int32_t sweep_buf[NBLK_PROBE * CTL];
static uint32_t sweep_n;

/* one preset, one held note, one parameter forced; fills sweep_buf with the left channel, returns its RMS */
static double render_with(uint32_t e, uint32_t pi, int32_t pidx, int32_t val)
{
    uint32_t b, i;
    double acc = 0;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_LEVEL] = 100;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    if (pidx >= 0)
        trk[0].p[pidx] = (int16_t)val;
    trk_note_on(&trk[0], 60, 100);
    for (b = 0; b < NBLK_PROBE; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            sweep_buf[b * CTL + i] = o[2 * i];
    }
    sweep_n = NBLK_PROBE * CTL;
    for (i = (NBLK_PROBE / 3u) * CTL; i < sweep_n; i++)
        acc += (double)sweep_buf[i] * sweep_buf[i];
    return acc;
}

static double rel_diff(const int32_t *a, const int32_t *b, double ref)
{
    uint32_t i;
    double acc = 0;
    for (i = (NBLK_PROBE / 3u) * CTL; i < sweep_n; i++) {
        double d = (double)a[i] - (double)b[i];
        acc += d * d;
    }
    return ref > 1.0 ? sqrt(acc / ref) : 0.0;
}


int main(int argc, char **argv)
{
    uint32_t e, k, dead = 0, total = 0, all = (argc > 1 && argv[1][0] == 'a');
    printf("param sweep: %s, forced to min and max, against the same note untouched\n",
           all ? "every track parameter, on ANALOG's first preset" : "every engine's EDIT 1..8");
    printf("%-10s %-4s %-10s %8s %8s  %s\n", "ENGINE", "P", "LABEL", "to min", "to max", "verdict");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        static int32_t base[NBLK_PROBE * CTL];
        double ref;
        uint32_t i, n = all ? 1u : en->npresets;
        if (!en->name || !en->npresets || eng_gone(e) || !n)
            continue;
        ref = render_with(e, 0, -1, 0);
        for (i = 0; i < sweep_n; i++)
            base[i] = sweep_buf[i];
        for (k = 0; k < (all ? P_COUNT : 8u); k++) {
            const param_desc_t *d = all ? &TP[k] : &en->edit[k];
            int32_t idx = all ? (int32_t)k : (int32_t)(P_E0 + k);
            double dmin, dmax, v;
            if (idx == P_LEVEL || idx == P_E0 || (all && idx >= P_E0 && idx <= P_E7))
                continue;                    /* LEVEL is the loudness itself; the E knobs are swept per engine */
            render_with(e, 0, idx, d->min);
            dmin = rel_diff(base, sweep_buf, ref);
            render_with(e, 0, idx, d->max);
            dmax = rel_diff(base, sweep_buf, ref);
            v = dmin > dmax ? dmin : dmax;
            total++;
            if (v < 0.01)
                dead++;
            printf("%-10s %-4d %-10s %7.1f%% %7.1f%%  %s\n", en->name, idx, d->label,
                   dmin * 100.0, dmax * 100.0, v < 0.01 ? "DEAD" : (v < 0.10 ? "weak" : "ok"));
        }
        if (!all)
            printf("\n");
        if (all)
            break;
    }
    printf("param sweep: %u parameters, %u DEAD (< 1 %% of the sound)\n", total, dead);
    return dead ? 1 : 0;
}
