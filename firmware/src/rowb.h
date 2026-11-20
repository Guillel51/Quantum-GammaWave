/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A LINE BUILT WITH ITS OWN LENGTH.
 *
 * The screens that print numbers build a row by appending to a char[]: "12 / 12 VOICES REL 3 FADE 1 PK 12
 * STL 4211 DEN 87". That was written as `str_cpy(b + str_len(b), "/", sizeof b - str_len(b))` around
 * `fmt_int`, and fmt_int has no bound -- it writes where it is told. Once a row outgrew the array, str_len()
 * ran past it and `sizeof b - str_len(b)` wrapped to a huge size_t, so the next str_cpy wrote past the end:
 * the row did not merely lose its tail, it overwrote whatever the compiler had put next to it. On GLO
 * MONITOR that is the row that grows while a sequence runs (STEALS and DENIED keep counting), which is
 * exactly where the fields were reported to overlap and become unreadable.
 *
 * This keeps the length as it goes and never writes past the capacity, so a row that does not fit is cut
 * short but stays a string. tests/rowb_test.c drives it with the longest rows and a canary past the end.
 */
#ifndef QG_ROWB_H
#define QG_ROWB_H

#include <stdint.h>

typedef struct {
    char *b;
    uint32_t n, cap;
} rowb_t;

static inline void rowb_init(rowb_t *r, char *b, uint32_t cap)
{
    r->b = b;
    r->n = 0;
    r->cap = cap;
    if (cap)
        b[0] = 0;
}

static inline void rowb_str(rowb_t *r, const char *s)
{
    while (s && *s && r->n + 1u < r->cap)
        r->b[r->n++] = *s++;
    if (r->cap)
        r->b[r->n] = 0;
}

/* its own integer, so the builder owes the project nothing and tests/rowb_test.c can drive it alone (libc.c's
 * fmt_int is static and unbounded: it is exactly what must not be handed the row's tail) */
static inline void rowb_i32(char *t, int32_t v)
{
    char tmp[12];
    uint32_t u = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v, n = 0;
    do {
        tmp[n++] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u);
    if (v < 0)
        *t++ = '-';
    while (n)
        *t++ = tmp[--n];
    *t = 0;
}

static inline void rowb_int(rowb_t *r, int32_t v)
{
    char t[12];
    rowb_i32(t, v);
    rowb_str(r, t);
}

#endif
