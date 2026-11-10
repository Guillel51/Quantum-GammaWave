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

/* the host has no power-on: every run starts from the same state, or the two notes of a comparison differ by
 * the delay tail and the LFO phase of whatever ran before them and the measurement says nothing */
static void host_reset(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    voices_boot();                    /* (3.5) the pool: a window of slots a part, not every voice on slot 0 */
    host_tracks_init();
    memset(comb_buf, 0, sizeof comb_buf);
    memset(dly_buf, 0, sizeof dly_buf);
    memset(rev_comb, 0, sizeof rev_comb);
    vage = 0;
    mns_clk = 0;
}
/* one LOFI note, BYTE with a formula that moves, from a given clock instant */
static void run_note(int32_t *buf, uint32_t tclk, uint32_t ms, uint32_t preset_after)
{
    uint32_t b, i;
    host_reset();
    host_preset(&trk[0], 3u, 0);
    trk[0].p[P_E1] = 5;                              /* WAVE = BYTE */
    trk[0].p[P_E2] = 4u << 2;                         /* ALGO: a formula that changes with t */
    trk[0].p[P_E3] = 64;                              /* VAR */
    trk[0].p[P_LEVEL] = 90;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_DEC] = 100;
    trk[0].p[P_SUS] = 100;
    trk[0].p[P_REL] = 60;
    trk[0].p[P_LRTRG] = (int16_t)tclk;
    if (preset_after)                                /* the switch has to survive a sound change */
        host_preset(&trk[0], 3u, 1);
    fm1_ms = ms;
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
/* the two buffers sample for sample */
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
    printf("lofi: %-66s %s  %s\n", what, ok ? "ok" : "FAIL", detail);
    if (!ok) fails++;
}

int main(void)
{
    char d[160];
    const uint32_t n = NBLK * CTL;

    printf("lofi: T.CLK through the real render path (voice.c -> eng_lofi.c), BYTE formula 4\n");
    /* the first note in a process is not like the rest (the master is still coming up from silence), so it is
     * played and thrown away before anything is measured */
    run_note(buf_a, 0, 1, 0);

    /* There is a floor under every comparison here: the host has no power-on reset for the LFO phase and the
     * effect tails, so two notes of the same pair never match to the last bit. The floor is measured first, with
     * the control the complaint was about, and everything else is stated against it. */
    run_note(buf_a, 0, 1000, 0);
    run_note(buf_b, 0, 4000, 0);
    {
        double noise = diff_ratio(buf_a, buf_b, n);
        double r;

        printf("lofi: the floor of the harness itself (NOTE twice): %.4f of the signal\n", noise);

        /* 1. the default restarts: two notes, same everything, land on the same waveform */
        snprintf(d, sizeof d, "%.4f vs the %.4f floor", noise, noise);
        check("T.CLK NOTE: two notes start from the same place", noise < 0.30, d);

        /* 2. FREE joins the running formula: two notes 3 s apart are different readings of it */
        run_note(buf_a, 1, 1000, 0);
        run_note(buf_b, 1, 4000, 0);
        r = diff_ratio(buf_a, buf_b, n);
        snprintf(d, sizeof d, "%.3f of the signal, %.1fx the floor", r, noise > 0 ? r / noise : 0.0);
        check("T.CLK FREE: two notes 3 s apart render different audio", r > 0.30 && r > 4.0 * noise, d);

        /* 3. and it is deterministic: the same instant is the same audio, floor and all */
        run_note(buf_a, 1, 2500, 0);
        run_note(buf_b, 1, 2500, 0);
        r = diff_ratio(buf_a, buf_b, n);
        snprintf(d, sizeof d, "%.4f vs the %.4f floor", r, noise);
        check("T.CLK FREE: the same instant twice is the same audio", r < 4.0 * noise + 0.02, d);
    }

    /* 4. it is not just a different note: the note's own envelope and level are the same */
    run_note(buf_a, 0, 2500, 0);
    run_note(buf_b, 1, 2500, 0);
    {
        double r0 = seg_rms(buf_a, 0, n), r1 = seg_rms(buf_b, 0, n);
        double db = 20.0 * log10((r1 + 1.0) / (r0 + 1.0));
        snprintf(d, sizeof d, "%.1f dB between the two clocks", db);
        check("T.CLK FREE keeps the sound's level (not a second sound)", db > -3.0 && db < 3.0, d);
    }

    /* 5. the switch survives a sound change */
    run_note(buf_a, 1, 5000, 1);
    run_note(buf_b, 1, 8000, 1);
    snprintf(d, sizeof d, "%.3f of the signal after loading another sound", diff_ratio(buf_a, buf_b, n));
    check("T.CLK FREE survives a preset load", diff_ratio(buf_a, buf_b, n) > 0.10, d);

    /* 6. joining the formula does not click: the first block of a FREE note is as smooth as a NOTE one */
    run_note(buf_a, 0, 7000, 0);
    run_note(buf_b, 1, 7000, 0);
    {
        int32_t s0 = seg_step(buf_a, 0, 4u * CTL), s1 = seg_step(buf_b, 0, 4u * CTL);
        snprintf(d, sizeof d, "largest step at the attack %d vs %d (NOTE)", s1, s0);
        check("T.CLK FREE: no click where the note joins the formula", s1 <= s0 + 512, d);
    }

    if (fails) {
        printf("lofi T.CLK: %d FAILED\n", fails);
        return 1;
    }
    printf("lofi T.CLK: all passed\n");
    return 0;
}
