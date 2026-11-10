/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MORPH (P_MORPH) audibility, measured on the real DSP sources -- not on the screen.
 *
 * The claim under test is the one a visual check cannot settle: moving MORPH while a note
 * plays must change the samples the mix produces. For every engine that has a MORPH path
 * this test renders the same held note twice from the same state:
 *
 *   control  MORPH stays where the test put it (0)
 *   morph    MORPH is set to 127 at the same block of the same note
 *
 * and compares the two renders sample by sample after the switch. If the engine applies
 * MORPH, the waveforms diverge; if it does not, the two renders are bit-identical and the
 * knob is a drawing. The switch is also measured for a click: the largest sample step
 * across it against the largest one in the 20 blocks before it (the slew in voice.c
 * track_render exists for exactly this).
 *
 *   tests/run_tests.sh builds it into build/host/; run it there or by hand:
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/morph_audible_test tests/morph_audible_test.c -lm */
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
    voices_boot();                    /* (3.5) the pool's windows and a clean pool, as felucca_init */
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

#define NBLK 640u            /* 640 * 32 samples = ~0.46 s, well past the 127-step slew */
#define SWITCH 160u          /* the block MORPH is moved at (~0.12 s into the note) */
static int32_t buf_ctrl[NBLK * CTL], buf_morph[NBLK * CTL];

/* render one held note; at block SWITCH set P_MORPH to `to` when `move` is set */
static void run_render(uint32_t e, uint32_t pi, uint32_t wave, uint32_t move, int32_t to)
{
    int32_t *buf = move ? buf_morph : buf_ctrl;
    uint32_t b, i;
    host_tracks_init();
    host_preset(&trk[0], e, pi);
    trk[0].p[P_MORPH] = 0;
    if (e == 0u)
        trk[0].p[P_E0] = (int16_t)wave;             /* ANALOG: the WAVE MORPH moves along from */
    else if (e == 3u)
        trk[0].p[P_E1] = (int16_t)wave;             /* LOFI: the wave it morphs towards */
    else if (e == 2u)
        trk[0].p[P_E0] = (int16_t)wave;             /* PHASE: the pd wave */
    trk[0].p[P_LEVEL] = 90;
    trk[0].mo_cur = trk[0].mo_prev = 0;
    trk_note_on(&trk[0], e == ENGI_DRUM ? 36u : 60u, 100);
    for (b = 0; b < NBLK; b++) {
        int32_t o[2 * CTL];
        if (move && b == SWITCH)
            trk[0].p[P_MORPH] = (int16_t)to;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            buf[b * CTL + i] = o[2 * i];
    }
}

/* rms of samples [a, b) */
static double seg_rms(const int32_t *s, uint32_t a, uint32_t b)
{
    double acc = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        acc += (double)s[i] * s[i];
    return b > a ? sqrt(acc / (double)(b - a)) : 0;
}
/* the largest |x[n] - x[n-1]| in [a, b) */
static int32_t seg_step(const int32_t *s, uint32_t a, uint32_t b)
{
    int32_t m = 0;
    uint32_t i;
    for (i = a + 1; i < b && i; i++) {
        int32_t d = s[i] - s[i - 1];
        d = d < 0 ? -d : d;
        if (d > m) m = d;
    }
    return m;
}

static int fails, cases;
/* one engine/wave: is MORPH audible, and does moving it click? */
static void probe(const char *name, uint32_t e, uint32_t pi, uint32_t wave)
{
    uint32_t from = SWITCH * CTL, to = NBLK * CTL;
    double sig, diff = 0, ratio;
    uint32_t i;
    int32_t before, across;
    run_render(e, pi, wave, 0, 0);
    run_render(e, pi, wave, 1, 127);
    sig = seg_rms(buf_ctrl, from, to);
    for (i = from; i < to; i++) {
        double d = (double)buf_morph[i] - (double)buf_ctrl[i];
        diff += d * d;
    }
    diff = sqrt(diff / (double)(to - from));
    ratio = sig > 0 ? diff / sig : 0;
    before = seg_step(buf_ctrl, from - 20u * CTL, from);
    across = seg_step(buf_morph, from, from + 4u * CTL);
    cases++;
    /* audible: the two renders differ by a real fraction of the signal, not by 1 LSB */
    if (ratio < 0.02) {
        fails++;
        printf("morph: %-18s %-5s wave %2u: silent (difference %.4f of the signal)  FAIL\n", name, "", wave, ratio);
    } else {
        printf("morph: %-18s wave %2u: audible (difference %.2f of the signal), step at the turn %d vs %d before %s\n",
               name, wave, ratio, across, before, across <= before * 2 + 64 ? "ok" : "CLICK?");
        if (across > before * 2 + 64)
            fails++;
    }
}

int main(void)
{
    uint32_t w;
    printf("morph: P_MORPH through the real render path (voice.c track_render -> the engines)\n");
    for (w = 0; w < 4u; w++)                        /* ANALOG's plain waves 0..3 morph */
        probe("ANALOG SAW..", 0u, 0u, w);
    probe("ANALOG PWM", 0u, 3u, 4u);                /* 4..9 move another axis (width/ratio/spread) */
    probe("ANALOG SYNC", 0u, 12u, 5u);
    probe("ANALOG RING", 0u, 13u, 6u);
    probe("ANALOG SAW3", 0u, 28u, 7u);
    for (w = 0; w < 5u; w++)                        /* LOFI: PLS TRI SAW NOIS WRAM */
        probe("LOFI", 3u, 1u, w);
    for (w = 0; w < 8u; w++)                        /* PHASE: its eight waves */
        probe("PHASE", 2u, 0u, w);
    printf("\nmorph: %u cases, %d failed\n", cases, fails);
    return fails ? 1 : 0;
}
