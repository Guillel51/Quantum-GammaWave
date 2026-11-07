/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The delay's echo train (src/fx.c fx_dly_rep / fx_dly_gen / fx_dly_frac), the numbers FX > DLY draws.
 * The page must show the repeats, not a level meter, so this checks the numbers themselves against what
 * the bus really returns:
 *   1. silence in, nothing out: with the screen idle every repeat reads 0, so no bar can move.
 *   2. a hit starts the train: repeat 1 appears one delay time after the send and not before.
 *   3. FEEDBK 0: the second repeat is gone (the line holds one pass), so only bar 1 ever lights.
 *   4. FEEDBK 60: repeats 1 .. 4 fall (strictly, by about FEEDBK each), and the train ends.
 *   5. MIX 0: nothing at all (the wet is muted, so there is nothing to show).
 *   6. level is not what the bars measure: halving MIX halves every repeat's peak, and a hit twice as
 *      loud doubles it; the *pattern* of the repeats does not change with the level.
 *   7. PING-PONG (the right echo later) fills the same train, and it too ends.
 *   8. the train stops reading when the line has finished: after the last repeat the peaks decay to 0.
 * Usage: build/host/dly_echo_test */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("dlx: %-88s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t xs = 0x2468aceu;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) >> 31);
}

/* the six peaks and the repeat sounding, as the page reads them */
static int32_t pk[6];
static int32_t gen_now;
static void snap(void)
{
    int32_t k;
    for (k = 0; k < 6; k++)
        pk[k] = fx_dly_rep[k];
    gen_now = fx_dly_gen;
}
static int32_t pkmax[6];
static void snap_max(void)
{
    int32_t k;
    snap();
    for (k = 0; k < 6; k++)
        if (pk[k] > pkmax[k])
            pkmax[k] = pk[k];
}
static void clear_max(void) { memset(pkmax, 0, sizeof pkmax); }

/* one burst of `burst` blocks, then `blocks` blocks of silence; the peaks are kept at their maximum */
static void burst(int32_t amp, uint32_t burst, uint32_t blocks)
{
    int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i;
    for (b = 0; b < burst + blocks; b++) {
        for (i = 0; i < CTL; i++) {
            d[i] = b < burst ? noise(amp) : 0;
            c[i] = r[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        fx_dly_train(CTL);                             /* (3.0) the train steps once per block: what fx_run_t does */
        snap_max();
    }
}

static void params(int32_t mix, int32_t fb, int32_t ping, int32_t dtime)
{
    host_tracks_init();
    song.g[G_DMIX] = (int16_t)mix;
    song.g[G_DFDBK] = (int16_t)fb;
    song.g[G_DPING] = (int16_t)ping;
    song.g[G_DTIME] = (int16_t)dtime;
    song.g[G_DCOLOR] = 64;
    song.g[G_DPIT] = 0;
    song.g[G_DSPRY] = 0;
    song.g[G_WIDTH] = 0;
    song.g[G_RTYPE] = 0;
    song.g[G_RSIZE] = 0;
    fx_preset_flush();
    memset(pkmax, 0, sizeof pkmax);
}

int main(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t i, b, dl, at[6];
    int32_t k, k1;

    /* ---- 1. silence ---- */
    params(100, 60, 0, 20);
    for (b = 0; b < 40u; b++) {
        for (i = 0; i < CTL; i++) c[i] = d[i] = r[i] = 0;
        fx_buses(c, d, r, w, CTL);
        fx_dly_train(CTL);                             /* (3.0) the train steps once per block: what fx_run_t does */
    }
    snap();
    check("silence: no repeat is measured, so no bar can light", !pk[0] && !pk[1] && !pk[2] && !pk[3] && !pk[4] && !pk[5]);
    check("silence: no repeat is 'sounding now' either", gen_now == 0);

    /* ---- 2. a hit starts the train, one delay time later ---- */
    params(100, 60, 0, 20);
    dl = delay_samples();
    {   /* the hit, then block by block: repeat 1 can only appear after ~dl samples */
        uint32_t first[6];
        for (k = 0; k < 6; k++) first[k] = 0xFFFFFFFFu;
        for (b = 0; b < 2000u; b++) {
            for (i = 0; i < CTL; i++) {
                d[i] = b < 2u ? noise(20000) : 0;
                c[i] = r[i] = 0;
            }
            fx_buses(c, d, r, w, CTL);
            fx_dly_train(CTL);
            snap();
            for (k = 0; k < 6; k++)
                if (pk[k] > 0 && first[k] == 0xFFFFFFFFu)
                    first[k] = b;
        }
        for (k = 0; k < 6; k++) at[k] = first[k];
        printf("dlx: delay %u samples, first block with a peak: %u %u %u %u %u %u (of %u samples a block)\n",
               dl, at[0], at[1], at[2], at[3], at[4], at[5], (unsigned)CTL);
        check("repeat 1 appears one delay time after the hit, not before",
              at[0] != 0xFFFFFFFFu && at[0] * CTL + CTL + 1u >= dl && at[0] * CTL <= dl + 2u * CTL);
        check("the repeats arrive in order", at[0] < at[1] && at[1] < at[2]);
        check("the space between repeat 1 and 2 is one delay time",
              (at[1] - at[0]) * CTL + 2u * CTL >= dl && (at[1] - at[0]) * CTL <= dl + 2u * CTL);
    }

    /* ---- 3. FEEDBK 0: a single pass, so only the first repeat ---- */
    params(100, 0, 0, 20);
    clear_max();
    burst(20000, 2, 700);
    printf("dlx: FEEDBK 0 peaks: %d %d %d %d %d %d\n", pkmax[0], pkmax[1], pkmax[2], pkmax[3], pkmax[4], pkmax[5]);
    check("FEEDBK 0: repeat 1 sounds", pkmax[0] > 200);
    check("FEEDBK 0: no second repeat (the line keeps one pass)", pkmax[1] * 20 < pkmax[0]);

    /* ---- 4. FEEDBK 60: the train falls and ends ---- */
    params(100, 60, 0, 20);
    clear_max();
    burst(20000, 2, 700);
    printf("dlx: FEEDBK 60 peaks: %d %d %d %d %d %d\n", pkmax[0], pkmax[1], pkmax[2], pkmax[3], pkmax[4], pkmax[5]);
    check("FEEDBK 60: four repeats come back", pkmax[0] > 200 && pkmax[1] > 40 && pkmax[2] > 8 && pkmax[3] > 0);
    check("FEEDBK 60: each repeat is softer than the one before",
          pkmax[0] > pkmax[1] && pkmax[1] > pkmax[2] && pkmax[2] > pkmax[3]);
    k1 = pkmax[1] * 100 / (pkmax[0] ? pkmax[0] : 1);
    printf("dlx: repeat 2 is %d %% of repeat 1 (FEEDBK 60)\n", k1);
    check("FEEDBK 60: repeat 2 is about FEEDBK of repeat 1 (40 .. 80 %)", k1 > 40 && k1 < 80);

    /* ---- 5. MIX 0: nothing to show ---- */
    params(0, 60, 0, 20);
    clear_max();
    burst(20000, 2, 400);
    check("MIX 0: the wet is muted, so no repeat is measured", !pkmax[0] && !pkmax[1] && !pkmax[2]);

    /* ---- 6. the bars follow the repeats, not the level ---- */
    {   int32_t a0, b0, ha, hb;
        params(100, 60, 0, 20);
        clear_max();
        burst(20000, 2, 700);
        a0 = pkmax[0];
        b0 = pkmax[1];
        params(50, 60, 0, 20);                       /* half the mix: half the wet */
        clear_max();
        burst(20000, 2, 700);
        ha = pkmax[0];
        hb = pkmax[1];
        printf("dlx: MIX 100 -> %d / %d, MIX 50 -> %d / %d\n", a0, b0, ha, hb);
        check("halving MIX halves the repeats (the bars read the echo, linearly)",
              ha > a0 / 3 && ha < a0 * 2 / 3 && hb > b0 / 3 && hb < b0 * 2 / 3);
        check(".. and the pattern of the repeats is the same at any level",
              (b0 * 100 / (a0 ? a0 : 1)) - (hb * 100 / (ha ? ha : 1)) < 8 &&
              (hb * 100 / (ha ? ha : 1)) - (b0 * 100 / (a0 ? a0 : 1)) < 8);
        params(100, 60, 0, 20);                      /* twice the send: twice the peaks */
        clear_max();
        burst(40000, 2, 700);
        check("a hit twice as loud doubles the peaks", pkmax[0] > a0 * 3 / 2);
    }

    /* ---- 7. PING-PONG: the right echo later, same train ---- */
    params(100, 60, 1, 20);
    clear_max();
    burst(20000, 2, 700);
    printf("dlx: PING-PONG peaks: %d %d %d\n", pkmax[0], pkmax[1], pkmax[2]);
    check("PING-PONG: the train fills and falls too", pkmax[0] > 100 && pkmax[1] > 0 && pkmax[1] <= pkmax[0]);

    /* ---- 8. after the train, the page goes quiet ---- */
    params(100, 60, 0, 20);
    for (b = 0; b < 300u; b++) {                     /* a hit */
        for (i = 0; i < CTL; i++) { d[i] = b < 2u ? noise(20000) : 0; c[i] = r[i] = 0; }
        fx_buses(c, d, r, w, CTL);
        fx_dly_train(CTL);                             /* (3.0) the train steps once per block: what fx_run_t does */
    }
    snap();
    check("while the train rings, the page has something to show", pk[0] > 0 || pk[1] > 0 || pk[2] > 0);
    for (b = 0; b < 6000u; b++) {                    /* ~4.4 s of silence */
        for (i = 0; i < CTL; i++) c[i] = d[i] = r[i] = 0;
        fx_buses(c, d, r, w, CTL);
        fx_dly_train(CTL);                             /* (3.0) the train steps once per block: what fx_run_t does */
    }
    snap();
    printf("dlx: after 4 s of silence: %d %d %d %d %d %d\n", pk[0], pk[1], pk[2], pk[3], pk[4], pk[5]);
    check("the train ends: every bar is back to nothing (no frozen repeat)",
          !pk[0] && !pk[1] && !pk[2] && !pk[3] && !pk[4] && !pk[5]);
    check(".. and 'sounding now' is back to the hit", gen_now == 0);

    printf("dlx: %s\n", bad ? "FAIL" : "all passed");
    return bad ? 1 : 0;
}
