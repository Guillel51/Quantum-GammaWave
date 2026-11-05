/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The four delay states of the master EQ, one by one.
 *
 * master_eq_update() runs on every block (master_begin). When the parameters did not change it
 * must not touch a healthy state, and when a scene or a project loads values that are identical
 * to the current ones it may only clear a state a stable filter cannot reach.
 *
 * (v1.8.8) The guard compared s2_r against BQ_LIM / 2 without shifting by BQ_S, 1024x below the
 * other three states' threshold. An ordinary right-channel state is far above it, so with the EQ
 * active the right delay line was cleared on every block: the left channel kept its EQ and the
 * right lost it. The first test here is that exact case (a loud signal, parameters unchanged).
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* fx.c defines this next to BQ_LIM; keeping the fallback lets this test compile against the
 * pre-v1.8.8 firmware and show the right-channel bug it was written for. */
#ifndef BQ_STATE_HI
#define BQ_STATE_HI (((int64_t)BQ_LIM << BQ_S) / 2)
#endif

static int bad;
static int32_t bl[CTL], br[CTL];

static void check(const char *what, int ok)
{
    printf("eq-s: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void zeros(void)
{
    int i;
    for (i = 0; i < CTL; i++) bl[i] = br[i] = 0;
}

/* the same two int64 pointers, so each of the four states can be addressed by index */
static int64_t *state(int k)
{
    return k == 0 ? &master_eq.s1_l : k == 1 ? &master_eq.s1_r :
           k == 2 ? &master_eq.s2_l : &master_eq.s2_r;
}
static const char *state_name(int k)
{
    return k == 0 ? "s1_l" : k == 1 ? "s1_r" : k == 2 ? "s2_l" : "s2_r";
}

/* a loud tone through the worst shelf; the firmware calls master_eq_update() before every block.
 * 3000 blocks is ~2.2 s: a 20 Hz +24 dB shelf needs that long to reach its level, and the bug
 * only shows once the states are large (the old right-channel threshold was 32768 samples wide). */
static int stereo_matches(int blocks)
{
    int b, i, ok = 1;
    long n = 0;
    for (b = 0; b < blocks; b++) {
        master_eq_update();                          /* unchanged parameters: must not clear anything */
        for (i = 0; i < CTL; i++, n++)
            bl[i] = br[i] = (int32_t)(12000.0 * sin(2.0 * 3.141592653589793 * 20.0 * (double)n / FS));
        qg_biquad_process(&master_eq, bl, br, CTL);
        for (i = 0; i < CTL; i++)
            if (bl[i] != br[i]) ok = 0;
    }
    return ok;
}

int main(void)
{
    int b, i, k, ok;
    printf("eq-s: BQ_STATE_HI = %lld (BQ_LIM %d << BQ_S %d, half)\n",
           (long long)BQ_STATE_HI, (int)BQ_LIM, (int)BQ_S);

    host_tracks_init();
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    check("a low shelf at 20 Hz, +24 dB, is loaded", master_eq.active == 1);

    /* 1. the regression: identical input in both channels, identical output */
    ok = stereo_matches(3000);
    check("left and right stay bit-identical while master_eq_update() runs every block", ok);

    /* 2. an ordinary state (well inside the clamp, above BQ_LIM / 2) is not impossible */
    for (k = 0; k < 4; k++) {
        int only = 1;
        for (b = 0; b < 4; b++) {
            for (i = 0; i < 4; i++) *state(i) = 0;
            *state(k) = (int64_t)BQ_LIM / 2 + 1;     /* the old, unscaled right-channel threshold */
            master_eq_update();
            for (i = 0; i < 4; i++)
                if (*state(i) != ((i == k) ? (int64_t)BQ_LIM / 2 + 1 : 0)) only = 0;
        }
        {
            char what[128];
            snprintf(what, sizeof what, "%s = BQ_LIM/2+1 (a normal state) survives master_eq_update()", state_name(k));
            check(what, only);
        }
    }

    /* 3. each state on its own, far outside the clamp: cleared */
    for (k = 0; k < 4; k++) {
        int cleared = 1;
        for (i = 0; i < 4; i++) *state(i) = 0;
        *state(k) = BQ_STATE_HI + 1;
        master_eq_update();
        for (i = 0; i < 4; i++) if (*state(i) != 0) cleared = 0;
        {
            char what[128];
            snprintf(what, sizeof what, "%s = BQ_STATE_HI+1 (impossible) is cleared", state_name(k));
            check(what, cleared);
        }
    }

    /* 4. the boundary itself is still a possible state (v > HI is strict) */
    for (k = 0; k < 4; k++) {
        int kept = 1;
        for (i = 0; i < 4; i++) *state(i) = 0;
        *state(k) = BQ_STATE_HI;
        master_eq_update();
        for (i = 0; i < 4; i++)
            if (*state(i) != ((i == k) ? BQ_STATE_HI : 0)) kept = 0;
        {
            char what[128];
            snprintf(what, sizeof what, "%s = BQ_STATE_HI (the last valid value) is kept", state_name(k));
            check(what, kept);
        }
    }

    /* 5. a negative state outside the clamp is cleared too (symmetric), one state at a time */
    for (k = 0; k < 4; k++) {
        int cleared = 1;
        for (i = 0; i < 4; i++) *state(i) = 0;
        *state(k) = -BQ_STATE_HI - 1;
        master_eq_update();
        for (i = 0; i < 4; i++) if (*state(i) != 0) cleared = 0;
        {
            char what[128];
            snprintf(what, sizeof what, "%s = -BQ_STATE_HI-1 (impossible) is cleared", state_name(k));
            check(what, cleared);
        }
    }

    /* 6. a poisoned pair that a scene load can leave behind: the guard clears it and the filter
     *    starts from zero, so the next block is silence, not a burst */
    host_tracks_init();
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    master_eq.s1_l = master_eq.s2_l = BQ_STATE_HI + 7;
    master_eq.s1_r = master_eq.s2_r = -BQ_STATE_HI - 7;
    master_eq_update();
    zeros();
    qg_biquad_process(&master_eq, bl, br, CTL);
    ok = 1;
    for (i = 0; i < CTL; i++) if (bl[i] || br[i]) ok = 0;
    check("a poisoned state from a scene load is cleared before it can make sound", ok);

    /* 7. a parameter change still resets the filter (the setters clear it) */
    host_tracks_init();
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    master_eq.s1_l = master_eq.s2_l = 123456;
    master_eq.s1_r = master_eq.s2_r = 654321;
    song.g[G_EQMODE] = 1;                            /* a different type: a new filter, no old tail */
    master_eq_update();
    check("a new EQ type does not inherit the previous filter's delay line",
          master_eq.s1_l == 0 && master_eq.s1_r == 0 && master_eq.s2_l == 0 && master_eq.s2_r == 0);

    /* 8. the same values from a scene load keep a healthy state (no click on every scene recall) */
    host_tracks_init();
    song.g[G_EQMODE] = 3; song.g[G_EQFREQ] = 0; song.g[G_EQGAIN] = 24; song.g[G_EQQ] = 64;
    master_eq_update();
    master_eq.s1_l = 1000; master_eq.s1_r = 1001; master_eq.s2_l = 1002; master_eq.s2_r = 1003;
    master_eq_update();                              /* identical parameters: not a reload */
    check("a scene with the same values keeps the filter's healthy state",
          master_eq.s1_l == 1000 && master_eq.s1_r == 1001 && master_eq.s2_l == 1002 && master_eq.s2_r == 1003);

    printf("eq states: %s (%d failed)\n", bad ? "FAILED" : "passed", bad);
    return bad != 0;
}
