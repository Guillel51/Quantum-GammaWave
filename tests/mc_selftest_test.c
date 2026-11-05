/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host check of the second-core boot self-test payload (hal/fm1_mc_selftest.h).
 *
 * The payload runs on core 1 and its expected answers are computed on core 0. Here both
 * sides are the same process, so what is checked is the property the boot path relies on:
 * a core that copies the job number and writes the sequence produces exactly the words core
 * 0 expects, and any single wrong word (the known AC791N misread, 0x00200000 for 0), a
 * changed job number or a stale buffer is refused instead of being joined into the mix. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/hal/fm1_mc_selftest.h"

static int bad;

static void ok(int cond, const char *what)
{
    printf("%-62s %s\n", what, cond ? "ok" : "FAIL");
    bad += !cond;
}

int main(void)
{
    uint32_t buf[FM1_MC_SELFTEST_WORDS], ref[FM1_MC_SELFTEST_WORDS];
    uint32_t j;

    ok(FM1_MC_SELFTEST_JOBS >= 4u, "covers several jobs");
    ok(FM1_MC_SELFTEST_WORDS >= 2u && FM1_MC_SELFTEST_WORDS <= 12u,
       "buffer fits the mailbox's spare words");

    /* the sequence is deterministic: the same twice, different between jobs */
    ok(fm1_mc_selftest_word(1, 0) == fm1_mc_selftest_word(1, 0), "sequence is deterministic");
    ok(fm1_mc_selftest_word(1, 0) != fm1_mc_selftest_word(2, 0), "each job has its own sequence");
    ok(fm1_mc_selftest_word(1, 0) != fm1_mc_selftest_word(1, 1), "each word of a job differs");
    ok(fm1_mc_selftest_word(1, 0) != 0u && fm1_mc_selftest_word(1, 0) != 0xFFFFFFFFu,
       "no degenerate answers");

    /* a correct core: every job passes */
    for (j = 0; j < FM1_MC_SELFTEST_JOBS; j++) {
        fm1_mc_selftest_prepare(buf, j + 1u);
        fm1_mc_selftest_body(buf);
        if (fm1_mc_selftest_check(buf, j + 1u)) {
            printf("job %u refused\n", j + 1u);
            bad++;
        }
    }
    ok(1, "a core that answers every job passes");

    /* the known misread: 0x00200000 where the word holds 0, and the reverse */
    fm1_mc_selftest_prepare(buf, 3);
    fm1_mc_selftest_body(buf);
    memcpy(ref, buf, sizeof ref);
    for (j = 1; j < FM1_MC_SELFTEST_WORDS; j++) {
        buf[j] ^= 0x00200000u;
        ok(fm1_mc_selftest_check(buf, 3) < 0, "a word read with the 0x00200000 bit set is refused");
        memcpy(buf, ref, sizeof ref);
    }
    buf[1] = 0x00200000u;
    ok(fm1_mc_selftest_check(buf, 3) < 0, "0x00200000 in place of 0 is refused");

    /* a mailbox that came back with another job number is refused too */
    {
        uint32_t job = buf[0];
        buf[0] = job + 1u;
        ok(fm1_mc_selftest_check(buf, job) < 0, "a changed job number is refused");
    }

    /* a stale buffer (nothing written) can never match the expected words */
    fm1_mc_selftest_prepare(buf, 5);
    ok(fm1_mc_selftest_check(buf, 5) < 0, "an unanswered job is refused");

    puts(bad ? "second-core self-test FAILED" : "second-core self-test passed");
    return bad != 0;
}
