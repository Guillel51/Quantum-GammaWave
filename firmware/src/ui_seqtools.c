/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.2) SEQ > TOOLS: the edits a sequence needs and no knob page offered -- duplicate it, halve it, turn it, reverse
 * it, clear it -- applied to the selected track's pattern in place, with the UNDO point marked first (undo_do(), the
 * same one GEN and NOW use). The page draws the pattern it is about to change, so nothing is edited blind, and the
 * amount is what the page's own knob says. Only notes are touched: the steps' timing, gates and chance keep their
 * place, which is what makes DUP a musical repeat and not a reset.
 *
 * SEQUENCER TOOLS in the sense of the pending list: real operations on seq_steps with undo, not a menu. */

/* the pattern as the page shows it: 16 rows of the current bank, a mark a note (its length in the width) */
static __attribute__((noinline)) void graph_seq_tools(const track_t *t, uint16_t c)
{
    char b[24];
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    (void)c;
    cv_text_on(12, 3, &AF_M, N_SQOP[(uint32_t)clamp(sq_op, 0, SQOP_COUNT - 1)], T_ACCENT, T_SURF);
    {   /* (v1.5.18) the amount in the operator's own unit, and a line saying what it will do to the pattern */
        uint32_t op = (uint32_t)clamp(sq_op, 0, SQOP_COUNT - 1), a = (uint32_t)clamp(sq_amt, 1, 8);
        const char *hint;
        if (op == SQOP_EUCLID) {
            str_cpy(b, "HITS ", sizeof b);
            fmt_int(b + 5, (int32_t)(a * 2u > NSTEP ? NSTEP : a * 2u));
            hint = "EVEN ONSETS, ITS NOTES IN ORDER";
        } else if (op == SQOP_MIRROR) {
            str_cpy(b, "IN KEY", sizeof b);
            hint = "ITS LINE INVERTED AROUND THE TONIC";
        } else if (op == SQOP_THIN) {
            str_cpy(b, "1 OF ", sizeof b);
            fmt_int(b + 5, (int32_t)a);
            hint = "FEWER ONSETS, THE SAME SHAPE";
        } else if (op == SQOP_CRESC) {
            str_cpy(b, "RISE ", sizeof b);
            fmt_int(b + 5, (int32_t)(a * 12));
            hint = "A CRESCENDO ACROSS THE LENGTH";
        } else {
            str_cpy(b, "x", sizeof b);
            fmt_int(b + 1, (int32_t)a);
            hint = "NOTES KEPT: TIMING GATE CHANCE";
        }
        cv_text_r(228, 3, &AF_M, b, T_MID, T_SURF);
        cv_text_on(12, 58, &AF_S, hint, T_DIM, T_SURF);
    }
    /* the sixteen steps: on, tie, accent, chance */
    for (i = 0; i < 16u; i++) {
        uint32_t si = ui.bank * 16u + i;
        int32_t x = 10 + (int32_t)i * 14, y = 24;
        uint16_t bg = T_SURF;
        if (si < len && step_on(&seq_steps(t)[si])) {
            bg = (seq_steps(t)[si].flags & SF_ACCENT) ? T_ACCENT : T_THEME;
            cv_rrect(x, y, 12, 26, 2, bg, T_SURF);
            if (seq_steps(t)[si].n > 1u)             /* a chord: its notes stacked */
                cv_rect(x + 2, y + 3, 8, (int32_t)seq_steps(t)[si].n * 3, T_INK);
        } else {
            cv_rrect(x, y + 14, 12, 12, 2, T_RAISE, T_SURF);
        }
        if (si == ui.cursor)
            cv_frame(x, y, 12, 26, T_TEXT);
    }
    /* the two operations the page applies, and what they will touch */
    cv_text_on(12, 70, &AF_S, "UNDO MARKED BEFORE EACH TURN", T_DIM, T_SURF);
    str_cpy(b, "LEN ", sizeof b);
    fmt_int(b + 4, (int32_t)len);
    cv_text_on(12, 82, &AF_S, b, T_MID, T_SURF);
    cv_text_on(12, 98, &AF_S, "KNOB 1 OP  2 TIMES  3 APPLY  4 UNDO", T_DIM, T_SURF);
}

/* the edits themselves: the operator on the selected track's steps, on its whole length (the notes' time and gate are
 * never rewritten; DUP/HALF change where the notes sit, ROT/REV move them, CLEAR takes them out) */
static void sq_do(uint32_t op, uint32_t amt)
{
    seq_tool_apply(TSEL, op, amt);             /* (the operators live in seq.c: the test drives them there) */
}
