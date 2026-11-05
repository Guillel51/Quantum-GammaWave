/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Six-track live scene, on the host: the acceptance load with a player on top of
 * it. Six tracks hold ten voices (3+2+2+1+1+1) with DIST / CHORUS / DELAY /
 * REVERB per track and the master EQ, and while they sound the test does what a
 * hand does to the instrument: a cutoff sweep, pan and level moves, a send and a
 * drive change, a MIDI note off/on, a preset switch on a track, and the punch-in
 * lane (REPEAT 1/16 and LPF) pressed and released -- several of them on the same
 * block, because that is when a block is slow.
 *
 * Every block is measured (the percentiles cover the whole set) and the audio is
 * hashed once per second so a change in the render is caught, not just a change
 * in the timing. The wall clock is the host's: it proves the scene and the absence
 * of glitches, not the AC791N's timing (tests/target_budget.py covers the static
 * budget; target timing is NOT tested here).
 *
 * The expected hashes live in tests/six_live_golden.txt. SIX_LIVE_ACCEPT=1
 * rewrites that file: it is for a deliberate change in the sound, and the diff is
 * the evidence that the change was deliberate.
 *
 * build: cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests tests/six_track_live_test.c -lm
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *w, int ok)
{
    printf("live: %-66s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
static uint64_t fnv(uint64_t h, int32_t v)                  /* FNV-1a over the sample's four bytes */
{
    uint32_t u = (uint32_t)v;
    int k;
    for (k = 0; k < 4; k++, u >>= 8) h = (h ^ (u & 0xFFu)) * 1099511628211ull;
    return h;
}

int main(void)
{
    static int32_t out[2 * CTL];
    static uint64_t t[100000], h[600];
    /* ten notes over six tracks: 3 + 2 + 2 + 1 + 1 + 1 = the ten voices */
    static const uint8_t NOTE[6][3] = {{48, 55, 64}, {41, 53, 60}, {45, 57, 69}, {50, 0, 0}, {43, 0, 0}, {36, 0, 0}};
    static const uint8_t CNT[6] = {3, 2, 2, 1, 1, 1};
    const uint32_t blocks = (uint32_t)(60ull * FS / CTL);     /* one minute of blocks */
    const uint32_t bsec = FS / CTL;                           /* blocks in a second */
    const uint32_t b2s = 2u * bsec;                           /* the punch-in period: two seconds */
    const uint64_t deadline = (uint64_t)CTL * 1000000ull / FS * 1000ull;
    uint64_t worst = 0, total = 0, dt, prev = 0, hrun = 1469598103934665603ull;
    uint32_t b, i, j, k = 0, nsec = 0, busy_peak = 0, clips = 0, quiet = 0, late = 0, lost = 0;
    uint32_t zero_blocks = 0, nz_min = 0xFFFFFFFFu, noted = 0, kills0, steps = 0, beyond = 0;
    uint32_t punch_blocks = 0, preset_changes = 0, notes_moved = 0, moves = 0, mpitch = 50u;
    int32_t peak = 0, step_max = 0;
    double mean, pct_quiet;
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    memset(&chain, 0, sizeof chain);
    host_tracks_init();
    for (i = 0; i < NPART; i++) host_preset(&trk[i], 0, 0);
    for (i = 0; i < 6u; i++) {
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
    pf.lc = PF_TOP;                                          /* the punch-in layer, as the panel leaves it */
    for (i = 0; i < NTRK; i++) pf.mg[i] = 32768;
    pfx_tgt = 0;
    check("the scene asks for exactly ten notes over six tracks (3+2+2+1+1+1)", noted == 10u);
    check("all ten voices are sounding before the first block", voices_busy() == 10u);
    kills0 = voice_kills;
    memset(h, 0, sizeof h);
    for (b = 0; b < blocks; b++) {
        uint32_t ph = b % b2s, punch_now, busy;
        /* the hand: different periods on purpose, so changes land on the same block */
        if (b % 4u == 0u)  { trk[0].p[P_E0] = (uint8_t)((b / 4u) % 128u); moves++; }         /* cutoff sweep */
        if (b % 6u == 0u)  { trk[1].p[P_PAN] = (uint8_t)((b / 6u) % 128u); moves++; }        /* pan */
        if (b % 8u == 0u)  { trk[2].p[P_LEVEL] = (uint8_t)(60u + (b / 8u) % 64u); moves++; } /* level */
        if (b % 9u == 0u)  { trk[3].p[P_REV] = (uint8_t)(10u + (b / 9u) % 80u); moves++; }   /* reverb send */
        if (b % 10u == 0u) { trk[4].p[P_DIST] = (uint8_t)((b / 10u) % 60u); moves++; }       /* drive */
        if (b % bsec == 0u && b) {                            /* one preset switch a second, on the one-note track */
            host_preset_req(&trk[5], 0, (uint32_t)(b / bsec) % 6u);
            trk_note_on(&trk[5], NOTE[5][0], 100);            /* keep the tenth voice sounding */
            preset_changes++;
        }
        if (b % 16u == 0u) {                                  /* MIDI traffic: the one-note track changes pitch */
            trk_note_off(&trk[3], mpitch);
            mpitch = mpitch == 50u ? 52u : 50u;
            trk_note_on(&trk[3], mpitch, 100);
            k++; notes_moved++;
        }
        if (ph == 0u) perf_press(PF_R16, 1);                  /* punch-in: REPEAT 1/16 ... */
        if (ph == 275u) perf_press(PF_R16, 0);
        if (ph == 1378u) perf_press(PF_LPF, 1);               /* ... then LPF, in the same two seconds */
        if (ph == 1653u) perf_press(PF_LPF, 0);
        punch_now = ph < 275u || (ph >= 1378u && ph < 1653u);
        punch_blocks += punch_now;
        dt = now_ns();
        mix_block(out, CTL);
        dt = now_ns() - dt;
        total += dt;
        t[b] = dt;
        if (dt > worst) worst = dt;
        if (dt > deadline) late++;
        busy = voices_busy();
        if (busy > busy_peak) busy_peak = busy;
        if (busy != 10u) lost++;
        {
            uint32_t nz = 0;
            for (i = 0; i < 2u * CTL; i++) {
                int32_t v = out[i], a = v < 0 ? -v : v;
                int32_t d;
                if (v > peak) peak = v;
                if (-v > peak) peak = -v;
                if (a >= 32700) clips++;
                if (a > 32767) beyond++;                      /* a sample outside 16-bit: not a level, a fault */
                if (a == 0) quiet++; else nz++;
                d = prev > v ? (int32_t)(prev - v) : (int32_t)(v - prev);
                prev = v;
                if (d > step_max) step_max = d;
                if (d > 24000 && !punch_now) steps++;          /* a step no filter sweep makes: a click */
                hrun = fnv(hrun, v);
            }
            if (nz == 0) zero_blocks++;
            if (nz < nz_min) nz_min = nz;
        }
        if (b && b % bsec == 0u) h[nsec++] = hrun;            /* one hash per second */
    }
    h[nsec++] = hrun;
    mean = (double)total / (double)blocks;
    pct_quiet = 100.0 * (double)quiet / ((double)blocks * 2.0 * CTL);
    qsort(t, blocks, sizeof t[0], cmp_u64);
    printf("live: 6 tracks, 10 voices, %u blocks of %u frames (%.0f s): %u parameter moves, %u MIDI notes,\n"
           "live:   %u preset switches, %u blocks with a punch-in effect held\n",
           blocks, CTL, (double)blocks * CTL / FS, moves, notes_moved, preset_changes, punch_blocks);
    printf("live: mean %.1f us, p50 %.1f us, p95 %.1f us, p99 %.1f us, p99.9 %.1f us, worst %.1f us, deadline %.1f us\n",
           mean / 1000.0, (double)t[blocks / 2] / 1000.0, (double)t[(uint32_t)((double)blocks * 0.95)] / 1000.0,
           (double)t[(uint32_t)((double)blocks * 0.99)] / 1000.0,
           (double)t[(uint32_t)((double)blocks * 0.999)] / 1000.0, (double)worst / 1000.0, (double)deadline / 1000.0);
    printf("live: blocks over the deadline %u of %u (%.3f %%), peak %d, rail %u, beyond 16-bit %u, silence %.3f %%\n",
           late, blocks, 100.0 * (double)late / (double)blocks, peak, clips, beyond, pct_quiet);
    printf("live: silent blocks %u, fewest non-zero samples in a block %u of %u, largest one-sample step %d, "
           "steps over 24000 outside punch-in %u\n", zero_blocks, nz_min, 2u * CTL, step_max, steps);
    printf("live: heap: nothing allocated by the render path (the firmware links no allocator); the largest "
           "buffer here is the %u-byte block\n", (unsigned)sizeof out);
    check("ten voices stay sounding in every one of the blocks", lost == 0u);
    check("no voice was stolen during the run", voice_kills == kills0);
    check("audio is present in every block and never hits the rail", peak > 1000 && clips == 0u);
    check("no sample leaves the 16-bit range (no overflow in any stage)", beyond == 0u);
    check("no block is entirely silent while the ten voices are held", zero_blocks == 0u);
    check("no step larger than 24000 between samples outside the punch-in (no click)", steps == 0u);
    check("the p99.9 of all blocks fits the audio deadline (host clock)",
          t[(uint32_t)((double)blocks * 0.999)] < deadline);
    check("no systematic overrun: fewer than 0.01 % of blocks late", (double)late / (double)blocks < 1e-4);
    check("the scene really moved: every hand exercised", moves > 1000u && notes_moved > 100u &&
          preset_changes > 50u && punch_blocks > 1000u);
    {   /* the audio itself, one hash a second: a change in what it renders is a failure */
        const char *path = getenv("SIX_LIVE_GOLDEN");
        char line[32];
        uint32_t diff = 0, seen = 0;
        FILE *f;
        if (!path) path = "tests/six_live_golden.txt";
        if (getenv("SIX_LIVE_ACCEPT")) {
            f = fopen(path, "w");
            if (!f) { printf("live: cannot write %s\n", path); bad++; }
            else {
                for (i = 0; i < nsec; i++) fprintf(f, "%016llx\n", (unsigned long long)h[i]);
                fclose(f);
                printf("live: wrote %u audio hashes to %s (SIX_LIVE_ACCEPT)\n", nsec, path);
            }
        } else if (!(f = fopen(path, "r"))) {
            printf("live: no %s: run once with SIX_LIVE_ACCEPT=1 to record it\n", path);
            bad++;
        } else {
            while (fgets(line, sizeof line, f)) {
                unsigned long long v;
                if (sscanf(line, "%llx", &v) != 1) continue;
                if (seen >= nsec || (uint64_t)v != h[seen]) diff++;
                seen++;
            }
            fclose(f);
            printf("live: %u of %u seconds of audio match %s\n", nsec - diff, nsec, path);
            check("the rendered audio is the one recorded, second by second", diff == 0u && seen == nsec);
        }
    }
    printf(bad ? "live: %d FAILED\n" : "live: all passed\n", bad);
    return bad != 0;
}
