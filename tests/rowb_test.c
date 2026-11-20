/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* THE BOUNDED ROW BUILDER (firmware/src/rowb.h), the GLO MONITOR's rows included.
 *
 * The defect this guards: the diag page built each row with `str_cpy(b + str_len(b), .., sizeof b - str_len(b))`
 * around fmt_int, which has no bound. While a sequence ran, STEALS and DENIED grew the row past the 48 bytes
 * the array had, str_len() ran past it, `sizeof b - str_len(b)` wrapped and the next str_cpy wrote past the
 * array -- the fields after it were the ones reported as overlapping.
 *
 * Class: host x86-64, the real rowb.h and lib.c fmt_int.
 */
#include "../firmware/src/rowb.h"
#include <stdio.h>
#include <string.h>

static int bad;
static void check(const char *what, int ok, const char *got)
{
    printf("rowb: %-64s %s  \"%s\"\n", what, ok ? "ok" : "FAIL", got);
    bad += !ok;
}

/* a buffer with a guard right after it: any write past cap flips it */
typedef struct { char b[64]; uint32_t guard; } buf_t;

static buf_t sbuf;
static void row_start(void)
{
    rowb_t r;
    sbuf.guard = 0xA5A5A5A5u;
    rowb_init(&r, sbuf.b, sizeof sbuf.b);
    (void)r;
}

int main(void)
{
    buf_t s;
    /* the real GLO MONITOR line with every counter at its worst: it is 55 characters with two-digit figures
     * and 61 with four-digit ones -- past the 48 the array used to have */
    {
        rowb_t r;
        s.guard = 0xA5A5A5A5u;
        rowb_init(&r, s.b, sizeof s.b);
        for (int k = 0; k < 2; k++) {
            rowb_int(&r, 12);
            rowb_str(&r, " / 12 VOICES REL ");
            rowb_int(&r, 12);
            rowb_str(&r, " FADE 12 PK 12 STL 12345 DEN 6789");
        }
        rowb_str(&r, " 123456/123456 BAR");
        check("POLYPHONY row twice over plus a bar stays a string", s.guard == 0xA5A5A5A5u, s.b);
        printf("rowb: %-64s %s  %u chars (cap 63), guard %s\n", "the widest rows there are", "info",
               (unsigned)strlen(s.b), s.guard == 0xA5A5A5A5u ? "intact" : "BROKEN");
    }
    {
        rowb_t r;
        s.guard = 0xA5A5A5A5u;
        rowb_init(&r, s.b, sizeof s.b);
        rowb_int(&r, 12);
        rowb_str(&r, " / 12 VOICES REL 12 FADE 12 PK 12 STL 12345 DEN 6789");
        check("the GLO MONITOR POLYPHONY row, every counter up, is whole",
              strcmp(s.b, "12 / 12 VOICES REL 12 FADE 12 PK 12 STL 12345 DEN 6789") == 0, s.b);
    }
    printf("rowb: %-64s %s  %u chars, guard %s\n", "that row's length", "info", (unsigned)strlen(s.b),
           s.guard == 0xA5A5A5A5u ? "intact" : "BROKEN");

    /* the cap is the cap: cutting a row short may lose its tail, never the byte after the array */
    {
        char small[12];
        buf_t *p = (buf_t *)0;
        rowb_t r;
        (void)p;
        rowb_init(&r, small, sizeof small);
        rowb_str(&r, "12345678901234567890");
        check("a row longer than its buffer is cut at the cap",
              strlen(small) == sizeof small - 1u && small[sizeof small - 1u] == 0, small);
    }
    {   /* cap 0: no write at all, and no read of an unterminated array */
        char zero[4] = {'X', 'X', 'X', 'X'};
        rowb_t r;
        rowb_init(&r, zero, 0);
        rowb_str(&r, "abc");
        rowb_int(&r, 99);
        check("cap 0 writes nothing", zero[0] == 'X' && zero[1] == 'X' && zero[2] == 'X' && zero[3] == 'X', "-");
    }
    {   /* a number that exactly fills the buffer still leaves room for the terminator */
        char one[2];
        rowb_t r;
        rowb_init(&r, one, sizeof one);
        rowb_int(&r, 7);
        check("a one-byte buffer holds \"7\"", one[0] == '7' && one[1] == 0, one);
    }
    /* the other rows of the page, at their widest: N/A CPU, a split with a job time, three returns and LATE */
    {
        buf_t t;
        rowb_t r;
        t.guard = 0x5A5A5A5Au;
        rowb_init(&r, t.b, sizeof t.b);
        rowb_str(&r, "N/A");
        rowb_str(&r, " ");
        rowb_str(&r, "C0 ");
        rowb_int(&r, 6);
        rowb_str(&r, " C1 ");
        rowb_int(&r, 0);
        rowb_str(&r, " 99999us 9999 TK");
        check("CORE SPLIT row fits", t.guard == 0x5A5A5A5Au && strcmp(t.b, "N/A C0 6 C1 0 99999us 9999 TK") == 0, t.b);
    }
    {
        buf_t u;
        rowb_t r;
        u.guard = 0x0F0F0F0Fu;
        rowb_init(&r, u.b, sizeof u.b);
        rowb_int(&r, 99);
        rowb_str(&r, " 99 99  LATE ");
        rowb_int(&r, 12345);
        rowb_str(&r, "/");
        rowb_int(&r, 12345);
        check("FX RET row fits", u.guard == 0x0F0F0F0Fu && strcmp(u.b, "99 99 99  LATE 12345/12345") == 0, u.b);
    }
    {
        buf_t w;
        rowb_t r;
        w.guard = 0x12345678u;
        rowb_init(&r, w.b, sizeof w.b);
        rowb_str(&r, "STOP ");
        rowb_int(&r, 64);
        rowb_str(&r, "/");
        rowb_int(&r, 64);
        rowb_str(&r, "  ");
        rowb_int(&r, 123456);
        rowb_str(&r, " BAR");
        check("SEQ row fits", w.guard == 0x12345678u && strcmp(w.b, "STOP 64/64  123456 BAR") == 0, w.b);
    }
    printf("rowb: %s\n", bad ? "FAILED" : "all rows stay inside their buffer");
    return bad != 0;
}
