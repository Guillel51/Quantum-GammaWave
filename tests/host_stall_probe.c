/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The control for the six-track live test's slowest block: the same timed loop, the same clock, the
 * same counters, and no DSP at all -- each iteration just spins for the length of a block's budget
 * (about 725 us is the deadline; 10 us is what the scene actually costs).
 *
 * Whatever this program sees, the instrument did not do. If the six-track test's 1-5 ms outliers
 * show up here at the same wall-clock spacings, they are the host taking the CPU away, and the
 * render path is not implicated. If they do not, the slow block is really in the DSP and the
 * six-track numbers stand as a finding.
 *
 * build: cc -O2 -w tests/host_stall_probe.c -o build/stallprobe
 * run:   STALL_SECS=600 ./build/stallprobe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static uint64_t cpu_ns(void)
{
#if defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#else
    return 0;
#endif
}

static void counters(int64_t *flt, int64_t *csw)
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

#define SLOW_N 12
static uint64_t sw[SLOW_N], sc[SLOW_N], sg[SLOW_N];
static int64_t sf[SLOW_N], sx[SLOW_N];
static uint32_t sat[SLOW_N], sn;

static void note(uint64_t wall, uint64_t cpu, uint64_t gap, uint32_t it, int64_t flt, int64_t csw)
{
    uint32_t i, j;
    if (sn < SLOW_N) sn++;
    else {
        if (wall <= sw[0]) return;
        for (j = 1u; j < SLOW_N; j++) {
            sw[j - 1u] = sw[j]; sc[j - 1u] = sc[j]; sg[j - 1u] = sg[j];
            sf[j - 1u] = sf[j]; sx[j - 1u] = sx[j]; sat[j - 1u] = sat[j];
        }
    }
    i = sn - 1u;
    while (i > 0u && sw[i - 1u] > wall) {
        sw[i] = sw[i - 1u]; sc[i] = sc[i - 1u]; sg[i] = sg[i - 1u];
        sf[i] = sf[i - 1u]; sx[i] = sx[i - 1u]; sat[i] = sat[i - 1u];
        i--;
    }
    sw[i] = wall; sc[i] = cpu; sg[i] = gap; sf[i] = flt; sx[i] = csw; sat[i] = it;
}

int main(void)
{
    uint32_t secs = getenv("STALL_SECS") ? (uint32_t)atoi(getenv("STALL_SECS")) : 600u;
    uint64_t spin = getenv("STALL_SPIN_NS") ? (uint64_t)atoll(getenv("STALL_SPIN_NS")) : 10000ull;
    uint64_t deadline = 725000ull, t0, dt, cpu, gap, prev_end = 0, total = 0, worst = 0, end;
    uint32_t it = 0, late = 0;
    int64_t f0, f1, c0, c1;
    setvbuf(stdout, NULL, _IONBF, 0);
    t0 = now_ns();
    end = t0 + (uint64_t)secs * 1000000000ull;
    while (now_ns() < end) {
        counters(&f0, &c0);
        gap = prev_end ? now_ns() - prev_end : 0;
        dt = now_ns();
        cpu = cpu_ns();
        {   /* the block's worth of work, without any of the block */
            uint64_t stop = dt + spin;
            while (now_ns() < stop) { }
        }
        cpu = cpu_ns() - cpu;
        dt = now_ns() - dt;
        counters(&f1, &c1);
        prev_end = now_ns();
        total += dt;
        if (dt > worst) worst = dt;
        if (dt > deadline) late++;
        note(dt, cpu, gap, it, f1 - f0, c1 - c0);
        it++;
    }
    printf("stall: %u iterations spinning %llu ns, %.0f s of wall, mean %.1f us, worst %.1f us, "
           "over the %llu us deadline %u (%.4f %%)\n", it, (unsigned long long)spin,
           (double)(prev_end - t0) / 1e9, (double)total / (double)(it ? it : 1u) / 1000.0,
           (double)worst / 1000.0, (unsigned long long)(deadline / 1000u), late,
           100.0 * (double)late / (double)(it ? it : 1u));
    printf("stall: the %u slowest iterations (wall | CPU | before it | iteration | s | faults | switches):\n", sn);
    {
        uint32_t i;
        for (i = sn; i-- > 0u;)
            printf("stall:   %8.1f us | %8.1f us | %8.1f us | %8u | %9.3f s | %lld | %lld\n",
                   (double)sw[i] / 1000.0, (double)sc[i] / 1000.0, (double)sg[i] / 1000.0,
                   sat[i], (double)(sat[i] + 1u) * (double)spin / 1e9,
                   (long long)sf[i], (long long)sx[i]);
    }
    return late > it / 100u;                                  /* a whole percent would not be a host stall */
}
