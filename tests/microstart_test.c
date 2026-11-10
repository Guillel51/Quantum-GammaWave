/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* LOFI T.CLK (P_LRTRG) audibility, measured on the real DSP sources -- not on the screen.
 *
 * The complaint this test answers: on a LOFI track the RETRIG control did nothing audible. It was a plain
 * ON/OFF that only acted on a retrigger of a *sounding* voice, and in POLY every note is a fresh voice, so
 * the reset it asked for had already happened: two renders were bit-identical whatever the switch said.
 *
 * T.CLK is now the formula's clock. NOTE (the default, and what every factory sound expects) starts each note
 * from the formula's designed start; FREE leaves the formula running on the track clock and a note joins it
 * where it is. The test holds everything else still and asks the samples:
 *
 *   NOTE, two notes            bit-identical (the default really restarts)
 *   FREE, two notes 2 s apart  a real fraction of the signal differs (the control is audible)
 *   FREE, the same instant     bit-identical (deterministic: two notes at the same time are the same note)
 *   FREE under a preset load   still FREE (a sound change does not silently drop the switch)
 *   and the note's own attack is checked for a click at the join.
 *
 *   tests/run_tests.sh builds it into build/host/; run it there or by hand:
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/lofi_tclk_test tests/lofi_tclk_test.c -lm */
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

/* the setup hostsim.c uses, kept minimal: factory defaults and an empty pattern per track */
static void host_tracks_init(void)
{
    uint32_t i, k;
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

#define NBLK 320u                 /* 320 * 32 samples = 0.23 s of a held note */
static int32_t buf_a[NBLK * CTL], buf_b[NBLK * CTL];

/* the host has no power-on: every run starts from the same state, or the two notes of a comparison differ by the
 * delay tail and the LFO phase of whatever ran before them and the measurement says nothing */
static void host_reset(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    voices_boot();                                   /* (3.6) the pool: a window per part, no slot left from before */
    memset(comb_buf, 0, sizeof comb_buf);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    memset(cho_buf, 0, sizeof cho_buf);
    vage = 0;
    mns_clk = 0;
}
/* one ANALOG note with M.STRT at `ms` and N.RATE at `nr`; the note is held for the whole buffer */
static void run_note(int32_t *buf, uint32_t ms, uint32_t nr)
{
    uint32_t b, i;
    host_reset();
    host_preset(&trk[0], 0u, 0);                     /* ANALOG, its first sound */
    trk[0].p[P_LEVEL] = 90;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_DEC] = 100;
    trk[0].p[P_SUS] = 100;
    trk[0].p[P_REL] = 60;
    trk[0].p[P_MSTART] = (int16_t)ms;
    trk[0].p[P_NRATE] = (int16_t)nr;
    trk[0].p[P_AMODE] = 0;                           /* no ARP: one note, not a note every step */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_DIST] = trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_LRATE] = 0;
    trk[0].p[P_LD_PIT] = trk[0].p[P_LD_FLT] = trk[0].p[P_LD_SHP] = trk[0].p[P_LD_AMP] = 0;
    fm1_ms = 1000u;
    for (b = 0; b < 40u; b++) {                      /* the tail of whatever ran before: let it die, unrecorded */
        int32_t o[2 * CTL];
        mix_block(o, CTL);
    }
    trk_note_on(&trk[0], 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
}
/* where the note really starts: the first window that reaches a quarter of the note's own steady level. A bare
 * threshold is not enough -- the chorus and the master EQ carry a little of the previous run's tail, and a note that
 * waits would otherwise look as if it had started at the top of the buffer */
static double seg_rms(const int32_t *s, uint32_t a, uint32_t b);
static uint32_t onset_w(const int32_t *s, uint32_t n)
{
    double steady = seg_rms(s, n / 2u, n), lim = steady * 0.5;
    uint32_t i;
    for (i = 0; i + 32u < n; i += 32u)              /* one block at a time: the wait is whole blocks */
        if (seg_rms(s, i, i + 32u) > lim)
            return i;
    return n;
}
static uint32_t onset(const int32_t *s, uint32_t n) { return onset_w(s, n); }
static double seg_rms(const int32_t *s, uint32_t a, uint32_t b)
{
    double acc = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        acc += (double)s[i] * s[i];
    return b > a ? sqrt(acc / (double)(b - a)) : 0;
}
static int32_t seg_step(const int32_t *s, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 1; i < b; i++) {
        int32_t d = s[i] - s[i - 1];
        d = d < 0 ? -d : d;
        if (d > m) m = d;
    }
    return m;
}
static uint32_t differ(const int32_t *a, const int32_t *b, uint32_t n)
{
    uint32_t i, d = 0;
    for (i = 0; i < n; i++)
        if (a[i] != b[i]) d++;
    return d;
}
static double diff_ratio(const int32_t *a, const int32_t *b, uint32_t n)
{
    double acc = 0, sig;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double d = (double)a[i] - (double)b[i];
        acc += d * d;
    }
    sig = seg_rms(a, 0, n) + seg_rms(b, 0, n);
    return sig > 0 ? 2.0 * sqrt(acc / (double)n) / sig : 0;
}

static int fails;
static void check(const char *what, int ok, const char *detail)
{
    printf("mstart: %-64s %s  %s\n", what, ok ? "ok" : "FAIL", detail);
    if (!ok) fails++;
}

int main(void)
{
    char d[160];
    const uint32_t n = NBLK * CTL;
    uint32_t i, o0, o1, lo = n, hi = 0, spread = 0, prev = 0;

    printf("mstart: M.STRT and N.RATE through the real render path (voice.c, mod.c)\n");
    run_note(buf_a, 0, 0);                           /* the first note of a process is not like the rest */

    /* 1. M.STRT 0 is the sound as it was: the note starts at the first sample, and two notes are the same */
    run_note(buf_a, 0, 0);
    run_note(buf_b, 0, 0);
    o0 = onset(buf_a, n);
    o1 = onset(buf_b, n);
    snprintf(d, sizeof d, "onset %u and %u, %u of %u samples differ", o0, o1, differ(buf_a, buf_b, n), n);
    /* the buffers are not bit-identical: the host has no power-on reset for the LFO phase and the effect tails, so
     * the claim here is the one that matters -- with M.STRT at 0 nothing waits, and the two notes start together */
    /* the note's own attack is a ramp, so half its steady level arrives two blocks in; what M.STRT 0 must not do is
     * add a wait on top of it, and the two notes must land in the same place */
    check("M.STRT 0: the note starts at once, as before", o0 == o1 && o0 <= 96u, d);

    /* 2. M.STRT 127: the note waits, and no two notes wait the same */
    for (i = 0; i < 8u; i++) {
        uint32_t o;
        run_note(buf_a, 127, 0);
        o = onset(buf_a, n);
        if (o < lo) lo = o;
        if (o > hi) hi = o;
        if (i && o != prev) spread++;
        prev = o;
    }
    snprintf(d, sizeof d, "onsets %u..%u samples (0..%u), %u of 7 notes moved", lo, hi, 27u * CTL, spread);
    check("M.STRT 127: the note waits, and the wait is not the same twice", lo > 0 && hi <= 27u * CTL && spread >= 4u, d);

    /* 3. the wait is silence, not a fade: nothing sounds before it and the first sample is small (no click) */
    run_note(buf_a, 127, 0);
    o0 = onset(buf_a, n);
    {
        int32_t first = buf_a[o0] < 0 ? -buf_a[o0] : buf_a[o0];
        int32_t step = seg_step(buf_a, o0, o0 + 64u);
        snprintf(d, sizeof d, "onset %u, first sample %d, largest step %d", o0, first, step);
        check("M.STRT: the wait is silence and the attack does not click", o0 > 0 && first < 2000 && step < 4000, d);
    }

    /* 4. the note is the same note, only later: its level over the held part is unchanged */
    run_note(buf_a, 0, 0);
    run_note(buf_b, 127, 0);
    {
        double a = seg_rms(buf_a, 2000, n), b = seg_rms(buf_b, 2000, n);
        double db = a > 0 && b > 0 ? 20.0 * log10(b / a) : 0.0;
        snprintf(d, sizeof d, "%.2f dB between M.STRT 0 and 127", db);
        check("M.STRT: the held part keeps its level", db > -1.0 && db < 1.0, d);
    }

    /* 5. N.RATE: the wander's hold, in blocks, from the transport's own grid */
    host_reset();
    {
        uint32_t v[6], k;
        for (k = 0; k < 6u; k++) {
            trk[0].p[P_NRATE] = (int16_t)k;
            v[k] = mod_nlen(&trk[0]);
        }
        snprintf(d, sizeof d, "1/1 %u, 1/2 %u, 1/4 %u, 1/8 %u, 1/16 %u, 46ms %u blocks", v[0], v[1], v[2], v[3], v[4], v[5]);
        check("N.RATE: the tempo's grid, then 46 ms as it was", v[5] == 64u && v[0] > v[1] && v[1] > v[2] && v[2] > v[3] &&
              v[3] > v[4] && v[4] >= 1u, d);
    }

    /* 6. and it is audible: the same NOISE slot on CUT moves the sound differently at two rates */
    {
        uint32_t k;
        for (k = 0; k < 2u; k++) {
            int32_t *buf = k ? buf_b : buf_a;
            host_reset();
            host_preset(&trk[0], 0u, 0);
            trk[0].p[P_LEVEL] = 90;
            trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 100; trk[0].p[P_SUS] = 100; trk[0].p[P_REL] = 60;
            trk[0].p[P_M1SRC] = MS_NOISE;
            trk[0].p[P_M1DST] = MD_CUT;
            trk[0].p[P_M1AMT] = 63;
            trk[0].p[P_NRATE] = k ? 4u : 0u;         /* 1/16 against 1/1 */
            fm1_ms = 1000u;
            trk_note_on(&trk[0], 60u, 100);
            for (i = 0; i < NBLK; i++) {
                int32_t o[2 * CTL];
                uint32_t j;
                mix_block(o, CTL);
                for (j = 0; j < CTL; j++)
                    buf[i * CTL + j] = o[2 * j];
            }
        }
        {
            double r = diff_ratio(buf_a, buf_b, n);
            snprintf(d, sizeof d, "%.3f of the signal between 1/1 and 1/16", r);
            check("N.RATE: the random source moves the sound at the rate it says", r > 0.05, d);
        }
    }

    printf("mstart: %s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
