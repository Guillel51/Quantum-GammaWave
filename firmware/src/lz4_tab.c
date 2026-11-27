/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 5c) THE TWO POINTER-FREE TABLES, PACKED WITH LZ4 AND REBUILT IN .bss AT BOOT.
 *
 * PRESET_NAMES (3 142 B) and FM6_ROMBANK (1 408 B) cost 4 550 B of XIP and are pure bytes: the first is text
 * read through PR_NAME(), the second is eleven 128-byte FM6 banks copied out by fvb_rom_read(). tools/gen_lz4_tables.py
 * packs them from the compiler's own object (and refuses a table that carries a relocation, which is what a
 * pointer in .rodata is -- the object's bytes would hold the linked address and no memcpy could rebuild it),
 * decompresses its own output to prove the round trip, and writes build/gen/lz4_tables.h. Here the decoder and
 * the two buffers live: 3 142 + 1 408 = 4 550 B of .bss, expanded once at boot by lz4_tables_init(), which
 * voices_boot() calls before anything reads a preset name or a built-in FM6 bank.
 *
 * The .bss is the price and it was measured: RAM goes from 12 232 B free to 7 682 B, above the 4 096 B floor
 * (tests/target_budget.py, tests/resources_test.py). The other three candidates the phase looked at -- DSTYLE,
 * RECIPE, ICON_MAP -- hold 21, 87 and 145 pointers between them and cannot be packed this way at all. */
#if !defined(QG_PRESET_SOURCE) && !defined(LZ4_TAB_C)   /* the packing pass has the tables themselves (build.py, two steps) */
#define LZ4_TAB_C 1

#include "lz4_tables.h"

char PRESET_NAMES_BUF[LZ4_PN_LEN];
uint8_t FM6_ROMBANK_B[LZ4_FM6_LEN];

static uint8_t lz4_tab_done;

/* LZ4 block format: token (4 bits of literal length, 4 of match length - 4), the literals, a 2-byte
 * little-endian offset back into the output and the match itself, which may overlap what it is copying
 * (hence byte by byte, not memcpy). */
static void lz4_expand(uint8_t *dst, const uint8_t *src, uint32_t clen)
{
    const uint8_t *ip = src, *iend = src + clen;
    uint8_t *op = dst;
    while (ip < iend) {
        uint32_t tok = *ip++, lit = tok >> 4, mlen, b, off;
        if (lit == 15)
            do { b = *ip++; lit += b; } while (b == 255);
        while (lit--)
            *op++ = *ip++;
        if (ip >= iend)                             /* the block ends on its last literal run */
            break;
        off = (uint32_t)ip[0] | (uint32_t)ip[1] << 8;
        ip += 2;
        mlen = tok & 15u;
        if (mlen == 15)
            do { b = *ip++; mlen += b; } while (b == 255);
        mlen += 4;
        {
            const uint8_t *m = op - off;
            while (mlen--)
                *op++ = *m++;                       /* overlaps on purpose: the match may reach the copy */
        }
    }
}

void lz4_tables_init(void)
{
    if (lz4_tab_done)
        return;
    lz4_tab_done = 1;
    lz4_expand((uint8_t *)PRESET_NAMES_BUF, LZ4_TAB_BLOB, LZ4_PN_CLEN);
    lz4_expand(FM6_ROMBANK_B, LZ4_TAB_BLOB + LZ4_PN_CLEN, LZ4_FM6_CLEN);
}

#endif
