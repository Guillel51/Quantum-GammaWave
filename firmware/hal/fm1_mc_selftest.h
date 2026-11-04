/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Second-core boot self-test payload (pure C: no asm, no registers, host-testable).
 *
 * The AC791N's second core is known to misread words of shared RAM on some units
 * (Melodee #17 / #21, fm1-x0x #10): a 32-bit load can come back as 0x00200000 where the
 * word holds 0, more often under load. `fm1_mc_start()` used to trust `alive` alone, so a
 * unit that misreads could be handed audio parts on its first block. Before the core is
 * enabled this payload runs a fixed sequence of jobs whose expected answers are computed
 * on core 0: each job writes its results into the shared mailbox and core 0 compares
 * them word for word. A unit that fails any job is held for good and the firmware plays
 * on one core, which is the documented, audible-safe fallback.
 *
 * The payload is deterministic (no clock, no random source) so core 1 and core 0 agree
 * on the expected bytes; it touches only the buffer it is given, so it cannot disturb
 * any audio state. */
#pragma once
#include <stdint.h>

#define FM1_MC_SELFTEST_JOBS 8u
#define FM1_MC_SELFTEST_WORDS 8u

/* one step of a small, fixed pseudo-random sequence (xorshift32: same on both cores) */
static inline uint32_t fm1_mc_self_xor(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

/* the word a job must produce at index i */
static inline uint32_t fm1_mc_selftest_word(uint32_t job, uint32_t i)
{
    uint32_t x = 0x9E3779B9u ^ (job * 0x85EBCA6Bu);
    uint32_t k;
    for (k = 0; k <= i; k++)
        x = fm1_mc_self_xor(x + k);
    return x;
}

/* core 1's job body: the shared buffer (FM1_MC_SELFTEST_WORDS words); the job number
 * travels in the first word, which core 0 sets. Reads it back through the same shared
 * memory the check word protects, so a misread of the mailbox is caught too. */
static inline void fm1_mc_selftest_body(uint32_t *p)
{
    uint32_t job = p[0], i;
    for (i = 1; i < FM1_MC_SELFTEST_WORDS; i++)
        p[i] = fm1_mc_selftest_word(job, i - 1u);
}

/* the same, as the mailbox hands it over (a 32-bit address on pi32v2) */
static inline void fm1_mc_selftest_run(uint32_t arg)
{
    fm1_mc_selftest_body((uint32_t *)(uintptr_t)arg);
}

/* core 0: fill the buffer for `job`, leaving room for the answer in word 0 */
static inline void fm1_mc_selftest_prepare(uint32_t *p, uint32_t job)
{
    uint32_t i;
    for (i = 0; i < FM1_MC_SELFTEST_WORDS; i++)
        p[i] = 0xDEADBEEFu ^ job;              /* a wrong answer cannot match by accident */
    p[0] = job;
}

/* core 0: 0 when every answer word is the expected one */
static inline int fm1_mc_selftest_check(const uint32_t *p, uint32_t job)
{
    uint32_t i;
    if (p[0] != job)
        return -1;                             /* the mailbox came back changed */
    for (i = 1; i < FM1_MC_SELFTEST_WORDS; i++)
        if (p[i] != fm1_mc_selftest_word(job, i - 1u))
            return -(int)(i + 1);
    return 0;
}
