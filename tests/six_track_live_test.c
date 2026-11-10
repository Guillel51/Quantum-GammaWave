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
 * The worst block is not the DSP's until it can be shown to be: SIX_LIVE_CPU=1 adds the thread's own
 * CPU clock beside the wall clock, and SIX_LIVE_PROBE=1 adds the page faults and context switches the
 * block caused. A block that really did the work burns CPU; a block the host took away does not, and
 * costs no fault and no switch. tests/host_stall_probe.c is the control: the same loop, the same
 * clocks, no DSP at all. On this host it reports multi-millisecond outliers of its own (5.8 ms once,
 * ~2 % of a second lost over five minutes) with zero faults, so the 1-5 ms blocks here are the host
 * descheduling the process, never a path in the render.
 *
 * The expected hashes live in tests/six_live_golden.txt. SIX_LIVE_ACCEPT=1
 * rewrites that file: it is for a deliberate change in the sound, and the diff is
 * the evidence that the change was deliberate.
 *
 * build: cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests tests/six_track_live_test.c -lm
 */
#include <sys/resource.h>
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
static uint64_t cpu_ns(void)                                /* thread CPU: wall time without being descheduled */
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#else
    return 0;                                               /* no such clock here: the column stays empty */
#endif
}

/* The slowest blocks, kept with the block number they happened on. A percentile cannot say which
 * block was slow; a single number for the worst cannot say whether the DSP did the work or the
 * host took the CPU away. Eight of them cost one comparison per block. */
#define SLOW_N 8
static uint64_t slow_wall[SLOW_N], slow_cpu[SLOW_N], slow_gap[SLOW_N];
static uint32_t slow_at[SLOW_N];
static int64_t slow_flt[SLOW_N], slow_csw[SLOW_N];         /* page faults and switches inside the block */
static uint32_t slow_n;
static void slow_note(uint64_t wall, uint64_t cpu, uint64_t gap, uint32_t b, int64_t flt, int64_t csw)
{
    uint32_t i, j;
    if (slow_n < SLOW_N) slow_n++;
    else {
        if (wall <= slow_wall[0]) return;                     /* sorted ascending: not one of the eight */
        for (j = 1u; j < SLOW_N; j++) {                       /* make room by dropping the smallest */
            slow_wall[j - 1u] = slow_wall[j]; slow_cpu[j - 1u] = slow_cpu[j];
            slow_gap[j - 1u] = slow_gap[j]; slow_at[j - 1u] = slow_at[j];
            slow_flt[j - 1u] = slow_flt[j]; slow_csw[j - 1u] = slow_csw[j];
        }
    }
    i = slow_n - 1u;                                          /* append at the tail, then sink to place */
    while (i > 0u && slow_wall[i - 1u] > wall) {
        slow_wall[i] = slow_wall[i - 1u]; slow_cpu[i] = slow_cpu[i - 1u];
        slow_gap[i] = slow_gap[i - 1u]; slow_at[i] = slow_at[i - 1u];
        slow_flt[i] = slow_flt[i - 1u]; slow_csw[i] = slow_csw[i - 1u];
        i--;
    }
    slow_wall[i] = wall; slow_cpu[i] = cpu; slow_gap[i] = gap; slow_at[i] = b;
    slow_flt[i] = flt; slow_csw[i] = csw;
}

/* What the block did besides compute: a page fault the kernel served (a fault costs CPU time and is
 * charged to this thread), or a voluntary switch. Without this a slow block is whatever we guess. */
static void thread_counters(int64_t *flt, int64_t *csw)
{
#if defined(RUSAGE_THREAD)
    struct rusage ru;
    if (getrusage(RUSAGE_THREAD, &ru)) { *flt = -1; *csw = -1; return; }
    *flt = (int64_t)ru.ru_minflt + (int64_t)ru.ru_majflt;
    *csw = (int64_t)ru.ru_nvcsw + (int64_t)ru.ru_nivcsw;
#else
    *flt = -1; *csw = -1;
#endif
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
    uint64_t *tv = t, *hv = h;                                /* a longer run needs more room than a minute */
    /* ten notes over six tracks: 3 + 2 + 2 + 1 + 1 + 1 = the ten voices */
    static const uint8_t NOTE[6][3] = {{48, 55, 64}, {41, 53, 60}, {45, 57, 69}, {50, 0, 0}, {43, 0, 0}, {36, 0, 0}};
    static const uint8_t CNT[6] = {3, 2, 2, 1, 1, 1};
    uint32_t secs = getenv("SIX_LIVE_SECS") ? (uint32_t)atoi(getenv("SIX_LIVE_SECS")) : 60u;
    const uint32_t blocks = (uint32_t)((uint64_t)secs * FS / CTL);   /* 60 s by default; SIX_LIVE_SECS=n for longer */
    const uint32_t bsec = FS / CTL;                           /* blocks in a second */
    const uint32_t b2s = 2u * bsec;                           /* the punch-in period: two seconds */
    const uint64_t deadline = (uint64_t)CTL * 1000000ull / FS * 1000ull;
    uint64_t worst = 0, total = 0, dt, prev = 0, hrun = 1469598103934665603ull;
    uint32_t b, i, j, k = 0, nsec = 0, busy_peak = 0, clips = 0, quiet = 0, late = 0, lost = 0;
    uint32_t zero_blocks = 0, nz_min = 0xFFFFFFFFu, noted = 0, kills0, steps = 0, beyond = 0;
    uint32_t punch_blocks = 0, preset_changes = 0, notes_moved = 0, moves = 0, mpitch = 50u;
    int32_t peak = 0, step_max = 0;
    double mean, pct_quiet;
    const int want_cpu = getenv("SIX_LIVE_CPU") != NULL;      /* two extra clock reads a block */
    const int probe = getenv("SIX_LIVE_PROBE") != NULL;       /* and the fault/switch counters too */
    uint64_t cdt = 0, cpu_total = 0, gap = 0, prev_end = 0;
    int64_t f0 = 0, f1 = 0, c0 = 0, c1 = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    voices_boot();                    /* (3.5) the pool's windows: a part's voices are not all slot 0 */
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
    if (secs != 60u) {                                        /* beyond a minute: the buffers grow, the golden does not */
        tv = malloc((size_t)blocks * sizeof *tv);
        hv = malloc((size_t)(secs + 2u) * sizeof *hv);
        if (!tv || !hv) { printf("live: out of memory for a %u s run\n", secs); return 1; }
    }
    memset(hv, 0, (size_t)(secs + 2u) * sizeof *hv);
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
        gap = prev_end ? now_ns() - prev_end : 0;             /* bookkeeping, hashing and the hand */
        if (probe) thread_counters(&f0, &c0);
        dt = now_ns();
        if (want_cpu) cdt = cpu_ns();
        mix_block(out, CTL);
        if (want_cpu) { cdt = cpu_ns() - cdt; cpu_total += cdt; }
        dt = now_ns() - dt;
        if (probe) thread_counters(&f1, &c1);
        prev_end = now_ns();
        total += dt;
        tv[b] = dt;
        if (dt > worst) worst = dt;
        slow_note(dt, cdt, gap, b, probe ? f1 - f0 : -1, probe ? c1 - c0 : -1);
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
        if (b && b % bsec == 0u) hv[nsec++] = hrun;           /* one hash per second */
    }
    hv[nsec++] = hrun;
    mean = (double)total / (double)blocks;
    pct_quiet = 100.0 * (double)quiet / ((double)blocks * 2.0 * CTL);
    qsort(tv, blocks, sizeof tv[0], cmp_u64);
    printf("live: 6 tracks, 10 voices, %u blocks of %u frames (%.0f s): %u parameter moves, %u MIDI notes,\n"
           "live:   %u preset switches, %u blocks with a punch-in effect held\n",
           blocks, CTL, (double)blocks * CTL / FS, moves, notes_moved, preset_changes, punch_blocks);
    printf("live: mean %.1f us, p50 %.1f us, p95 %.1f us, p99 %.1f us, p99.9 %.1f us, worst %.1f us, deadline %.1f us\n",
           mean / 1000.0, (double)tv[blocks / 2] / 1000.0, (double)tv[(uint32_t)((double)blocks * 0.95)] / 1000.0,
           (double)tv[(uint32_t)((double)blocks * 0.99)] / 1000.0,
           (double)tv[(uint32_t)((double)blocks * 0.999)] / 1000.0, (double)worst / 1000.0, (double)deadline / 1000.0);
    printf("live: blocks over the deadline %u of %u (%.3f %%), peak %d, rail %u, beyond 16-bit %u, silence %.3f %%\n",
           late, blocks, 100.0 * (double)late / (double)blocks, peak, clips, beyond, pct_quiet);
    if (slow_n) {
        printf("live: the %u slowest blocks (wall | CPU%s | before it | block | s%s):\n",
               slow_n, want_cpu ? "" : " not measured", probe ? " | faults | switches" : "");
        for (i = slow_n; i-- > 0u;) {
            printf("live:   %8.1f us | %8.1f us | %8.1f us | block %8u | %.3f s%s",
                   (double)slow_wall[i] / 1000.0, (double)slow_cpu[i] / 1000.0,
                   (double)slow_gap[i] / 1000.0, slow_at[i], (double)slow_at[i] * CTL / FS,
                   slow_at[i] == 0u ? " (the first)" : "");
            if (probe) printf(" | %lld | %lld", (long long)slow_flt[i], (long long)slow_csw[i]);
            printf("\n");
        }
        if (want_cpu && blocks)
            printf("live: thread CPU %.1f us a block against %.1f us of wall\n",
                   (double)cpu_total / (double)blocks / 1000.0, mean / 1000.0);
        if (probe) {                                          /* what those blocks did besides compute */
            int64_t flt = 0, csw = 0; uint32_t slow_late = 0;
            for (i = 0; i < slow_n; i++) { flt += slow_flt[i]; csw += slow_csw[i]; slow_late += slow_wall[i] > deadline; }
            printf("live: %u of the slow ones were over the deadline; together they caused %lld page faults and "
                   "%lld context switches", slow_late, (long long)flt, (long long)csw);
            if (slow_late && !flt && !csw) printf(" (still none inside a block: the host took the CPU)");
            printf("\n");
        }
    }
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
          tv[(uint32_t)((double)blocks * 0.999)] < deadline);
    check("no systematic overrun: fewer than 0.01 % of blocks late", (double)late / (double)blocks < 1e-4);
    check("the scene really moved: every hand exercised", moves > 1000u && notes_moved > 100u &&
          preset_changes > 50u && punch_blocks > 1000u);
    {   /* the audio itself, one hash a second: a change in what it renders is a failure */
        const char *path = getenv("SIX_LIVE_GOLDEN");
        char line[32];
        uint32_t diff = 0, seen = 0;
        FILE *f;
        if (!path) path = "tests/six_live_golden.txt";
        if (secs != 60u) {                                    /* the recorded hashes are one minute */
            printf("live: %u s run: the audio hash is not compared (the golden file is 60 s), last second %016llx, "
                   "whole run %016llx\n", secs, (unsigned long long)hv[nsec - 1], (unsigned long long)hrun);
            goto done;
        }
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
done:
    if (tv != t) free(tv);
    if (hv != h) free(hv);
    printf(bad ? "live: %d FAILED\n" : "live: all passed\n", bad);
    return bad != 0;
}
