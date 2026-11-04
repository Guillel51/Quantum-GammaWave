/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Six-track acceptance load, on the host: six tracks sounding at once with the
 * ten voices Quantum allows, per-track effects and the master EQ, for a minute
 * of continuous blocks. Every block is measured (the percentiles
 * cover the whole set, not a sample) and the run fails on clipping, silence while
 * notes are held, a lost or stolen voice, or a tail over the audio deadline. The
 * wall clock is the host's: it proves the scene and the absence of glitches, not
 * the AC791N's timing (target_budget.py covers the static budget; target timing
 * is NOT tested). */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *w, int ok)
{
    printf("six: %-64s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

int main(void)
{
    static int32_t out[2 * CTL];
    static uint64_t t[100000];
    /* ten notes over six tracks: 3 + 2 + 2 + 1 + 1 + 1 = the ten voices */
    static const uint8_t NOTE[6][3] = {{48, 55, 64}, {41, 53, 60}, {45, 57, 69}, {50, 0, 0}, {43, 0, 0}, {36, 0, 0}};
    static const uint8_t CNT[6] = {3, 2, 2, 1, 1, 1};
    uint64_t worst = 0, total = 0, dt, deadline = (uint64_t)CTL * 1000000ull / FS * 1000ull;
    uint32_t b, i, j, blocks = (uint32_t)(60ull * FS / CTL), busy_peak = 0, clips = 0, quiet = 0, late = 0, lost = 0;
    uint32_t noted = 0, kills0;
    int32_t peak = 0;
    double mean, pct_quiet;
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (i = 0; i < NPART; i++) host_preset(&trk[i], 0, 0);
    /* All six tracks sustain: a drum voice is one-shot and would drop out of the
     * ten by design, so the DRUM render path is covered by target_budget.py's
     * "DRUM path" line instead of being confused with a lost voice here. */
    for (i = 0; i < 6u; i++) {                          /* every track audible, with its own FX */
        trk[i].p[P_LEVEL] = 90;
        trk[i].p[P_DIST] = 20;
        trk[i].p[P_CHOR] = 30;
        trk[i].p[P_DLY] = 25;
        trk[i].p[P_REV] = 20;
        trk[i].p[P_VOICE] = V_POLY;
    }
    song.g[G_EQMODE] = 1; song.g[G_EQFREQ] = 64; song.g[G_EQGAIN] = 12; song.g[G_EQQ] = 64;
    song.master_q12 = 4096;
    for (i = 0; i < 6u; i++)
        for (j = 0; j < CNT[i]; j++, noted++)
            trk_note_on(&trk[i], NOTE[i][j], 100);
    check("the scene asks for exactly ten notes over six tracks (3+2+2+1+1+1)", noted == 10u);
    check("all ten voices are sounding before the first block", voices_busy() == 10u);
    kills0 = voice_kills;
    for (b = 0; b < blocks; b++) {
        uint32_t busy;
        dt = now_ns();
        mix_block(out, CTL);
        dt = now_ns() - dt;
        total += dt;
        t[b] = dt;
        if (dt > worst) worst = dt;
        if (dt > deadline) late++;
        busy = voices_busy();
        if (busy > busy_peak) busy_peak = busy;
        if (busy != 10u) lost++;                        /* a dropped or silenced voice */
        for (i = 0; i < 2u * CTL; i++) {
            int32_t v = out[i] < 0 ? -out[i] : out[i];
            if (v > peak) peak = v;
            if (v >= 32700) clips++;
            if (v == 0) quiet++;
        }
    }
    mean = (double)total / (double)blocks;
    pct_quiet = 100.0 * (double)quiet / ((double)blocks * 2.0 * CTL);
    qsort(t, blocks, sizeof t[0], cmp_u64);             /* the whole set: no sampling */
    printf("six: 6 tracks, %u notes, %u voices, %u blocks of %u frames (%.0f s)\n",
           noted, busy_peak, blocks, CTL, (double)blocks * CTL / FS);
    printf("six: p50 %.1f us, p99 %.1f us, p99.9 %.1f us, worst %.1f us, mean %.1f us, deadline %.1f us\n",
           (double)t[blocks / 2] / 1000.0, (double)t[(uint32_t)((double)blocks * 0.99)] / 1000.0,
           (double)t[(uint32_t)((double)blocks * 0.999)] / 1000.0, (double)worst / 1000.0,
           mean / 1000.0, (double)deadline / 1000.0);
    printf("six: blocks over the deadline %u of %u (%.3f %%), peak %d, samples at the rail %u, exact zeros %u (%.3f %%)\n",
           late, blocks, 100.0 * (double)late / (double)blocks, peak, clips, quiet, pct_quiet);
    check("ten voices stay sounding in every one of the blocks", lost == 0u);
    check("no voice was stolen during the run", voice_kills == kills0);
    check("audio is present and never hits the rail", peak > 1000 && clips == 0u);
    check("no silent buffer while the ten voices are held", pct_quiet < 50.0);
    check("the p99.9 of all blocks fits the audio deadline (host clock)",
          t[(uint32_t)((double)blocks * 0.999)] < deadline);
    check("no systematic overrun: fewer than 0.01 % of blocks late", (double)late / (double)blocks < 1e-4);
    printf(bad ? "six: %d FAILED\n" : "six: all passed\n", bad);
    return bad != 0;
}
