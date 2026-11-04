/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM1_INLINE: register helpers that must inline into their caller, so the
 * SFR access sequence (and its timing, in ISRs and IRQ-off sections) is the
 * one written at the call site; -Os would otherwise out-line a helper with
 * several callers. RAM-resident code (.ram_text) may only use these, never
 * a called helper (build.py checks .ram_text for calls). */
#pragma once

#define FM1_INLINE static inline __attribute__((always_inline))

/* (QG-1 v1.8.8) Cache-sync barriers as a named pair.
 *
 * On pi32v2 these are the chip's own instructions (`csync` before a DMA reads a buffer the
 * CPU wrote, `ssync` before the CPU reads a buffer a DMA wrote). A host build of the same
 * sources has no such instructions and no second bus master, so the right host meaning is a
 * compiler barrier with the same ordering — that is why `tests/persistence_test.c`,
 * `tests/backup_test.c` and `tests/fm6bank_test.c` could not be compiled with a plain cc
 * before this: the inline asm was assembled verbatim for x86. Naming them here keeps a single
 * definition and makes the target expansion byte-identical to what was written at each call
 * site. */
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__arm__) || defined(__EMSCRIPTEN__)
#define FM1_CSYNC() __asm__ volatile("" ::: "memory")
#define FM1_SSYNC() __asm__ volatile("" ::: "memory")
#else
#define FM1_CSYNC() __asm__ volatile("csync" ::: "memory")
#define FM1_SSYNC() __asm__ volatile("ssync" ::: "memory")
#endif
