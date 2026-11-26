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

/* one preset, one held note; returns 0 and fills peak/rms/hot */
static void render_note(uint32_t e, uint32_t pi, uint32_t note, int32_t *peak, double *rms, double *hot)
{
    uint32_t b, i, n = 0, hot_n = 0;
    double acc = 0, dacc = 0;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_LEVEL] = 100;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    trk_note_on(&trk[0], note, 100);
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
        dacc += (double)buf[i];
        n++;
        if (a > *peak) *peak = a;
        if (a >= 32000) hot_n++;                          /* inside the soft clipper's last stretch */
    }
    *rms = n ? sqrt(acc / (double)n) : 0;
    *hot = n ? (double)hot_n / (double)n : 0;
    dc_last = n ? dacc / ((double)n * 32768.0) : 0;
    {
        uint32_t i0 = (NBLK / 3u) * CTL, mid = i0 + (NBLK * CTL - i0) / 2u, m;
        double s1 = 0, s2 = 0;
        for (m = i0; m < mid; m++) s1 += (double)buf[m];
        for (m = mid; m < NBLK * CTL; m++) s2 += (double)buf[m];
        dc_h1 = s1 / ((double)(mid - i0) * 32768.0);
        dc_h2 = s2 / ((double)(NBLK * CTL - mid) * 32768.0);
    }
}

static void render_preset(uint32_t e, uint32_t pi, int32_t *peak, double *rms, double *hot)
{
    render_note(e, pi, e == ENGI_DRUM ? 36u : 60u, peak, rms, hot);
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

/* (1.3.4) `--fingerprint`: WHAT EACH SOUND IS, not what its numbers are. One held note per preset through the same
 * render, then ten log-spaced bands (Goertzel) from a 512-sample window in the sounding part and the envelope's
 * shape -- the block it reaches 90 % of its peak and the block it falls back to 50 %. Two presets whose spectra and
 * envelopes agree are the same sound to a player however differently their parameters are written, which is the
 * question a parameter comparison cannot answer (FM6's sounds are all zeros except their patch slot; DRUM's kits
 * are eight instrument numbers). tools/preset_audit.py --sound sorts by this. */
#define FP_W 512u
#define FP_NB 10u
#define FP_HITS 3u
static void jstr(const char *s);   /* below, with the dump */
static void fingerprint(void)
{
    static const double F[FP_NB] = {60, 100, 170, 290, 500, 850, 1450, 2500, 4300, 7400};
    uint32_t e, i, b, k, w0;
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        for (i = 0; i < en->npresets; i++) {
            int32_t peak = 0, lvl[NBLK], peakv = 1;
            double rms, hot, bands[FP_NB], bh[FP_HITS][FP_NB];
            uint32_t atk_b = 0, dec_b = NBLK, h;
            /* a kit is eight lanes: the kick alone says nothing about the snare, so DRUM is measured on three hits */
            uint32_t note[FP_HITS] = {36u, 38u, 42u};   /* C KICK, D SNARE, F# HAT CL (eng_drum.c's map) */
            if (e != ENGI_DRUM) { note[0] = 60u; note[1] = note[2] = 0u; }
            double rms0, hot0;
            int32_t peak0;
            render_note(e, i, note[0], &peak0, &rms0, &hot0);
            peak = peak0, rms = rms0, hot = hot0;   /* the level is the kit's first hit, not the last one measured */
            for (b = 0; b < NBLK; b++) {
                int32_t m = 0, j;
                for (j = 0; j < (int32_t)CTL; j++) {
                    int32_t a = buf[b * CTL + j];
                    a = a < 0 ? -a : a;
                    if (a > m) m = a;
                }
                lvl[b] = m;
                if (m > peakv) peakv = m;
            }
            for (b = 0; b < NBLK; b++)
                if (lvl[b] * 10 >= peakv * 9) { atk_b = b; break; }
            for (b = atk_b; b < NBLK; b++)
                if (lvl[b] * 2 < peakv) { dec_b = b; break; }
            w0 = (NBLK * 2u / 3u) * CTL;
            for (k = 0; k < FP_NB; k++) {
                double coeff = 2.0 * cos(2.0 * M_PI * F[k] / (double)FS), s1 = 0, s2 = 0;
                for (b = 0; b < FP_W; b++) {
                    double s0 = (double)buf[w0 + b] + coeff * s1 - s2;
                    s2 = s1;
                    s1 = s0;
                }
                bh[0][k] = sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2 + 1.0);
            }
            for (h = 1; h < FP_HITS; h++) {
                if (!note[h]) {
                    for (k = 0; k < FP_NB; k++)
                        bh[h][k] = bh[0][k];
                    continue;
                }
                int32_t pk1;
                double rm1, ht1;
                render_note(e, i, note[h], &pk1, &rm1, &ht1);
                w0 = (NBLK * 4u / 5u) * CTL;     /* a hit: its whole life is the sound, so a later window */
                for (k = 0; k < FP_NB; k++) {
                    double coeff = 2.0 * cos(2.0 * M_PI * F[k] / (double)FS), s1 = 0, s2 = 0;
                    for (b = 0; b < FP_W; b++) {
                        double s0 = (double)buf[w0 + b] + coeff * s1 - s2;
                        s2 = s1;
                        s1 = s0;
                    }
                    bh[h][k] = sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2 + 1.0);
                }
            }
            for (h = 0; h < FP_HITS; h++)
                for (k = 0; k < FP_NB; k++)
                    bands[h * FP_NB + k] = bh[h][k];
            printf("{\"engine\":");
            jstr(en->name);
            printf(",\"index\":%u,\"name\":", (unsigned)i);
            jstr(PR_NAME(&en->presets[i]));
            printf(",\"rms\":%.1f,\"peak\":%d,\"atk_ms\":%.1f,\"dec_ms\":%.1f,\"bands\":[",
                   rms, (int)peak, atk_b * (double)CTL * 1000.0 / (double)FS,
                   dec_b * (double)CTL * 1000.0 / (double)FS);
            for (k = 0; k < FP_NB * FP_HITS; k++)
                printf("%s%.1f", k ? "," : "", bands[k]);
            printf("]}\n");
        }
    }
}

static int fails;
static void fail(const char *fmt, ...) { (void)fmt; fails++; }

/* (6.0) `--dump`: the same tables this test measures, printed as data for tools/preset_audit.py. Reading
 * them out of the C source with a regex would be a guess (AW_SAW3, QG_SRC_VEL); this is the firmware's
 * own array. One JSON object per line, nothing else on stdout, so the output can be piped. */
static void jstr(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') printf("\\%c", c);
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

static void dump(void)
{
    uint32_t e, i, k;
    /* (1.3.4) the header: every engine's order, its table size and the EDIT ranges its descriptors declare, so the
     * audit reads the bounds from the firmware instead of a hand-kept table in the tool that goes stale when an
     * engine is added or a range changes (the tool used to carry them and refused to run on ADDTV) */
    printf("{\"engines\":[");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        printf("%s{\"name\":", e ? "," : "");
        jstr(en->name);
        printf(",\"npresets\":%u,\"legacy_n\":%u,\"rng\":[", (unsigned)en->npresets, (unsigned)en->legacy_n);
        for (k = 0; k < 8u; k++)
            printf("%s[%d,%d]", k ? "," : "", en->edit[k].min, en->edit[k].max);
        /* (1.3.4) and the FORMAT of each column: an F_ENUM / F_ONOFF value is a choice, not a number, so two
         * neighbouring values are two different sounds and the audit must not average them into "close" */
        printf("],\"typ\":[");
        for (k = 0; k < 8u; k++)
            printf("%s%u", k ? "," : "", (unsigned)en->edit[k].fmt);
        printf("],\"lbl\":[");
        for (k = 0; k < 8u; k++) {
            printf("%s\"", k ? "," : "");
            for (const char *c = en->edit[k].label; *c; c++)
                printf("%c", *c == '"' ? ' ' : *c);
            printf("\"");
        }
        printf("]}");
    }
    printf("]}\n");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        for (i = 0; i < en->npresets; i++) {
            const preset_t *p = &en->presets[i];
            printf("{\"engine\":");
            jstr(en->name);
            printf(",\"index\":%u,\"name\":", (unsigned)i);
            jstr(PR_NAME(p));
            printf(",\"e\":[");
            for (k = 0; k < 8u; k++) printf("%s%d", k ? "," : "", p->e[k]);
            printf("],\"env\":[%u,%u,%u,%u]", p->env[0], p->env[1], p->env[2], p->env[3]);
            printf(",\"fenv\":%d,\"mono\":%u,\"fx\":[%u,%u,%u,%u]", p->fenv, p->mono,
                   p->fx[0], p->fx[1], p->fx[2], p->fx[3]);
            printf(",\"pat\":%u,\"x\":[", p->pat);
            {
                uint32_t n = 0;
                for (k = 0; k < NELEM(p->x); k++) {
                    if (!p->x[k].id) continue;
                    printf("%s[%u,%d]", n++ ? "," : "", (unsigned)p->x[k].id - 1u, p->x[k].v);
                }
            }
            printf("],\"legacy\":%u}\n", p->legacy);
        }
    }
}

int main(int argc, char **argv)
{
    uint32_t e, i, k;
    int levels = argc > 1 && !strcmp(argv[1], "--levels");   /* (FASE 5) every preset's measured level */
    if (argc > 1 && !strcmp(argv[1], "--dump")) {
        dump();
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "--fingerprint")) {
        fingerprint();
        return 0;
    }
    uint32_t total = 0, silent = 0, hot_presets = 0, dup_names = 0, dup_params = 0, out_of_range = 0, dc_bad = 0;
    printf("preset audit: every factory preset through the real load and render (host x86, %u Hz)\n", (unsigned)FS);
    printf("%-8s %6s %7s %6s %8s %8s %6s %7s\n", "engine", "presets", "silent", "hot", "dupn", "dupp", "range", "dc max");
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        uint32_t n = en->npresets, e_silent = 0, e_hot = 0, e_dupn = 0, e_dupp = 0, e_range = 0;
        double e_dc = 0;
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
                    printf("  range: %-8s %-16s E%u = %d outside %d..%d\n", en->name, PR_NAME(p), (unsigned)k,
                           (int)p->e[k], (int)en->edit[k].min, (int)en->edit[k].max);
                }
            if (!PR_NAME(p)[0] || p->env[0] > 127 || p->env[1] > 127 || p->env[2] > 127 || p->env[3] > 127) {
                e_range++;
                printf("  range: %-8s %-16s name empty or an envelope stage past 127\n", en->name, PR_NAME(p));
            }
            for (k = 0; k < i; k++)
                if (strcmp(PR_NAME(p), PR_NAME(&en->presets[k])) == 0) e_dupn++;
            for (k = 0; k < i; k++)
                if (same_as(p, &en->presets[k])) {
                    e_dupp++;
                    printf("  duplicate: %-8s %-16s has the parameters of %s\n", en->name, PR_NAME(p), PR_NAME(&en->presets[k]));
                }
            render_preset(e, i, &peak, &rms, &hot);
            if (fabs(dc_last) > e_dc) e_dc = fabs(dc_last);
            if (fabs(dc_last) > 0.02) {
                dc_bad++;
                printf("  dc:     %-8s %-16s sits %.1f %% off centre (halves %.1f / %.1f %%, peak %d) %s\n",
                       en->name, PR_NAME(p), fabs(dc_last) * 100.0, dc_h1 * 100.0, dc_h2 * 100.0, (int)peak,
                       dc_h1 * dc_h2 > 0 ? "DC" : "sub-audio");
            }
            if (rms < 8.0) {
                e_silent++;
                printf("  silent: %-8s %-16s rms %.1f, peak %d\n", en->name, PR_NAME(p), rms, (int)peak);
            }
            if (levels)   /* (FASE 5) the number the remaster decides on: what each sound actually puts out */
                printf("%-8s %3u %-18s rms %6.1f peak %5d clip %5.2f%%\n",
                       en->name, (unsigned)i, PR_NAME(p), rms, (int)peak, hot * 100.0);
            if (hot > 0.02) {
                e_hot++;
                printf("  hot:    %-8s %-16s %.0f%% of the note in the clipper's last stretch (peak %d)\n",
                       en->name, PR_NAME(p), hot * 100.0, (int)peak);
            }
        }
        printf("%-8s %6u %7u %6u %8u %8u %6u %6.2f %%\n", en->name, n, e_silent, e_hot, e_dupn, e_dupp, e_range,
               e_dc * 100.0);
        silent += e_silent; hot_presets += e_hot; dup_names += e_dupn; dup_params += e_dupp; out_of_range += e_range;
    }
    printf("\ntotal %u presets: %u silent, %u hot (reported, not failed), %u repeated names, %u exact duplicates, "
           "%u out of range, %u with DC above 2 %%\n", total, silent, hot_presets, dup_names, dup_params, out_of_range,
           dc_bad);
    if (silent || dup_names || out_of_range)
        fails += (int)(silent + dup_names + out_of_range);
    /* (v2.3.5) DC is reported, like HOT: a bytebeat is asymmetric on purpose, so which of these sit too far off
     * centre is a listening and a per-engine decision, not arithmetic. What WOULD be a bug -- an offset that is
     * really a sub-audio body, not DC -- is told apart by the halves: real DC keeps its sign in both of them. */
    printf("preset audit: %s\n", fails ? "FAILED" : "passed (structure and signal; nobody listened)");
    return fails ? 1 : 0;
}
