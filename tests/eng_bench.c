/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Per-engine cost of a block, on the host, one engine at a time. The static count in
 * tests/target_budget.py says how much work a loop holds; tests/regress says whether the whole mix of a
 * heavy song fits; neither says what ONE engine costs at full polyphony, which is the number needed before
 * adding a tenth engine or a second partial bank to an existing one.
 *
 *   tests/eng_bench [blocks]           (default 200 blocks of 32 samples: 725 us each on the device)
 *
 * Builds the real firmware (web/emu/felucca_web.c, the same translation unit the emulator runs), picks each
 * selectable engine on T1 through the emulator's own test hook, holds eight keys so the engine is at
 * polyphony, and prints process CPU time per block. T2..T6 are silent and the sequencer is stopped, so what
 * it measures is that engine's voices plus the shared per-block work (FX, master): compare the difference
 * between engines, not the absolute number. HOST x86-64 numbers: they order the engines, they do not
 * predict the FM-1. It measures; it does not approve anything.
 *   cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/eng_bench tests/eng_bench.c -lm
 */
#include <time.h>
#include "../web/emu/felucca_web.c"

static double cpu_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    uint32_t blocks = argc > 1 ? (uint32_t)atoi(argv[1]) : 2000u, e, b, r, n = 0;
    double silent;
    if (blocks < 50u) blocks = 50u;
    web_seed(1);
    web_boot();
    web_keys(0);
    web_render(4096);                            /* every track silent, every voice free: the floor */
    silent = 1e9;
    for (r = 0; r < 3u; r++) {
        double t0 = cpu_s(), t1;
        for (b = 0; b < blocks; b++) web_render(32);
        t1 = cpu_s();
        if ((t1 - t0) / (double)blocks < silent) silent = (t1 - t0) / (double)blocks;
    }
    printf("eng_bench: host x86-64, best of 3 runs of %u blocks of 32 samples, 8 keys held on T1, T2..T6 silent\n",
           blocks);
    printf("%-10s %12s %12s\n", "engine", "us/block", "minus silent");
    printf("%-10s %12.3f %12s\n", "(silent)", silent * 1e6, "-");
    for (e = 0; e < NENGINES; e++) {
        double best = 1e9;
        if (!web_test_engine(e))
            continue;                            /* retired or not selectable: nothing to measure */
        for (r = 0; r < 3u; r++) {
            double t0, t1;
            web_keys(0);
            web_render(8192);                    /* its voices out, before and between the runs */
            web_keys(0xFF);                      /* eight keys down: the engine at polyphony */
            web_render(256);                     /* the attack and the first stage settle */
            t0 = cpu_s();
            for (b = 0; b < blocks; b++) web_render(32);
            t1 = cpu_s();
            if ((t1 - t0) / (double)blocks < best) best = (t1 - t0) / (double)blocks;
        }
        printf("%-10s %12.3f %12.3f\n", ENGINES[e]->name, best * 1e6, best * 1e6 - silent * 1e6);
        n++;
    }
    web_keys(0);
    printf("eng_bench: %u engines measured\n", n);
    return 0;
}
