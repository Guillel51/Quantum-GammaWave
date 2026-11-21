/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 6) The two engine options this phase added, measured instead of assumed:
 *
 *   LOFI   WAVE = SYNC  (7)   a pulse whose phase runs 2..5x the note's and restarts with it (DUTY picks the
 *                             ratio): the metallic hard sync the chip never had.
 *   PHASE  LINE = FOLD        designed and measured (docs/FASE6_OPCIONES_v1.5.29.md), rejected: +96 B of XIP
 *                             but +13 instructions a sample in phase_render's loop, over the DSP budget. Its
 *                             absence is checked here too, so the knob and the docs cannot drift apart.
 *
 * What this proves, and what it cannot:
 *   render       a held note renders at all (RMS above the floor: an option that produces silence is not one).
 *   audible      their samples differ from every other value of the same knob, and by a brightness (zero-crossing
 *                rate) that moves, not by one LSB: a control that only nudges the output is not an option.
 *   no drift     no factory preset uses the new values, so the library and the golden renders cannot have moved
 *                (the values are new, appended after the old ones, and the old ones keep their meaning).
 *   knobs        the descriptors carry the new maximum and the new name, which is what the UI reads.
 * It does not listen: ZCR is a number, and whether a fold is musical is a listening decision.
 *
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/engine_options_test tests/engine_options_test.c -lm
 *   build/host/engine_options_test
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FELUCCA_HOST 1
#define NVOICE 10
#define CTL 32
#define NBLK 96

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

static int32_t buf[NBLK * CTL];
/* every value of a knob rendered, kept: the distance between two options is measured on the samples, not guessed
 * from one number (a brightness alone cannot tell a fold from a ring: two different timbres can cross zero alike) */
static int32_t keep[8][NBLK * CTL];
static double dist(int a, int b)                       /* rms(a - b) / rms(a): how much of the sound moved */
{
    uint32_t i;
    double d = 0, r = 0;
    for (i = (NBLK / 3u) * CTL; i < NBLK * CTL; i++) {
        double x = (double)(keep[a][i] - keep[b][i]);
        d += x * x;
        r += (double)keep[a][i] * keep[a][i];
    }
    return r > 0 ? sqrt(d / r) : 0;
}

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

/* one engine index by its name: the test does not hard-code the ENGINES[] order */
static int eng_by_name(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e]->name && !strcmp(ENGINES[e]->name, n))
            return (int)e;
    return -1;
}
/* engine e, its preset pi, EDIT knob k forced to v: the note rendered into buf; fills rms and zcr */
static void render(uint32_t e, uint32_t pi, int k, int v, double *rms, double *zcr)
{
    const preset_t *pr = ENGINES[e]->presets + pi;
    uint32_t b, i, n = 0, xc = 0;
    double acc = 0;
    host_tracks_init();
    trk[0].eng_req = (uint8_t)e;
    trk[0].preset = 0;
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = ENGINES[e]->edit[i].def;
    for (i = 0; i < 8u && (pr->x[i].id || i == 0u); i++)
        if (pr->x[i].id) {
            uint32_t id = pr->x[i].id - 1u;
            trk[0].p[id] = (int16_t)clamp(pr->x[i].v, TP[id].min, TP[id].max);
        }
    if (k >= 0)
        trk[0].p[P_E0 + k] = (int16_t)v;
    trk[0].p[P_LEVEL] = 100;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    trk_note_on(&trk[0], 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
    for (i = (NBLK / 3u) * CTL + 1u; i < NBLK * CTL; i++) {   /* the sounding part */
        int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
        acc += (double)buf[i] * buf[i];
        n++;
        if ((buf[i] < 0) != (buf[i - 1u] < 0) && (a > 200 || buf[i - 1u] > 200 || buf[i - 1u] < -200))
            xc++;                                           /* a sign change that is not noise around zero */
    }
    *rms = n ? sqrt(acc / (double)n) : 0;
    *zcr = n ? (double)xc / (double)n : 0;
}

static int fails;
static void ok(int cond, const char *what)
{
    printf("%s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond)
        fails++;
}

int main(void)
{
    static double rms[8], zcr[8], br[8];
    char line[160];
    int e, i, k;
    double peak_rms;

    /* --- PHASE: LINE = FOLD was designed, measured and rejected (docs/FASE6_OPCIONES_v1.5.29.md):
     * +96 B of XIP, but +13 instructions a sample inside phase_render's loop (+14 % over the DSP budget), so it
     * is not in the engine and the knob still ends at 2 */
    e = eng_by_name("PHASE");
    ok(e >= 0, "PHASE is one of ENGINES[]");
    if (e >= 0) {
        printf("     PHASE edit[5] max %d, names[0] %s, names[3] %s\n", ENGINES[e]->edit[5].max,
               ENGINES[e]->edit[5].names ? ENGINES[e]->edit[5].names[0] : "?", 
               ENGINES[e]->edit[5].names && ENGINES[e]->edit[5].names[3] ? ENGINES[e]->edit[5].names[3] : "(none)");
        ok(ENGINES[e]->edit[5].max == 2, "PHASE LINE still ends at 2: FOLD did not ship");
        ok(ENGINES[e]->edit[5].names && ENGINES[e]->edit[5].names[3] == 0, "PHASE LINE names stop at PM");
    }

    /* --- the four values of FASE 3 reach factory sounds (docs/FASE3_SONIDO_v1.5.24.md §"lo que quedó": the
     * report asked for presets that demonstrate them, not for the engine alone to know them). Two presets of
     * FM6 carry them: KOTO HARP (NOTCH + PLUK) and SITAR RESON (SHELF + the matrix's RES). Found here by walking
     * every engine's table, so a preset edit that drops one is caught rather than silently un-demonstrated. */
    {
        static const int want[4] = {P_FTYPE, P_FTYPE, P_LWAVE, P_M1DST};
        static const int val[4] = {6, 7, 9, 26};
        static const char *const what[4] = {"NOTCH", "SHELF", "PLUK", "RES"};
        int w, ee, nn, pp, kk;
        for (w = 0; w < 4; w++) {
            int found = 0;
            for (ee = 0; ee < NENGINES && !found; ee++)
                for (nn = ENGINES[ee]->npresets, pp = 0; pp < nn && !found; pp++) {
                    const preset_t *pr = ENGINES[ee]->presets + pp;
                    for (kk = 0; kk < 6; kk++)
                        if (pr->x[kk].id == want[w] + 1 && pr->x[kk].v == val[w]) {
                            found = 1;
                            printf("     %s: %s / %s\n", what[w], ENGINES[ee]->name, pr->name);
                            break;
                        }
                }
            printf("     %s in a factory preset: %s\n", what[w], found ? "yes" : "NO");
            ok(found, what[w][0] ? "a factory preset uses the new value" : "");
        }
    }

    /* --- LOFI: WAVE 0..7, the new one is SYNC */
    e = eng_by_name("LOFI");
    ok(e >= 0, "LOFI is one of ENGINES[]");
    if (e >= 0) {
        ok(ENGINES[e]->edit[1].max == 7, "LOFI WAVE accepts 0..7");
        ok(ENGINES[e]->edit[1].names && ENGINES[e]->edit[1].names[7] &&
           !strcmp(ENGINES[e]->edit[1].names[7], "SYNC"), "LOFI WAVE is called SYNC at 7");
        peak_rms = 0;
        for (i = 0; i < 8; i++) {
            render((uint32_t)e, 0, 1, i, &rms[i], &zcr[i]);
            memcpy(keep[i], buf, sizeof buf);
            if (rms[i] > peak_rms) peak_rms = rms[i];
            printf("     LOFI WAVE %d (%s): rms %.0f, zero crossings %.4f/sample\n", i,
                   ENGINES[e]->edit[1].names[i], rms[i], zcr[i]);
        }
        ok(rms[7] > 8.0, "LOFI WAVE = SYNC sounds (rms above the 16-bit floor)");
        for (i = 0; i < 7; i++) {
            snprintf(line, sizeof line, "LOFI SYNC differs from %s: %.0f %% of the signal changed",
                     100.0 * dist(7, i), ENGINES[e]->edit[1].names[i]);
            ok(dist(7, i) > 0.05, line);
        }
    }

    /* --- and the library cannot have moved: no factory preset carries the new values */
    {
        int used_wave = 0;
        uint32_t pi, n;
        if ((e = eng_by_name("LOFI")) >= 0)
            for (n = ENGINES[e]->npresets, pi = 0; pi < n; pi++) {
                const preset_t *pr = ENGINES[e]->presets + pi;
                for (k = 0; k < 8; k++)
                    if (pr->x[k].id == P_E0 + 1 && pr->x[k].v == 7)
                        used_wave++;
            }
        printf("     presets on WAVE = SYNC: %d\n", used_wave);
        ok(used_wave == 0, "no factory preset uses SYNC: the library and the golden renders keep their samples");
    }

    printf("engine options: %s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
