/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 6) MORPH SCENES: four snapped states of the *same* sound (a track's p[] and the matrix's
 * slots 5..8) and a continuous position between the pair A..B. The position interpolates every id the
 * motion system already classifies as sound (motion_param), so what morphs is the same set a step can
 * automate: the engine's parameters, the envelope's times, the filters, the sends, the CHARACTER bank.
 *
 * What it never touches, on purpose: track_t.engine (an engine swap is a discrete, already-faded event and
 * mixing two engines' parameter tables would be meaningless), the routing, the transport, and the note
 * allocator. The matrix's SRC/DST of the four spare slots follow scene B from the half point; only their
 * AMT, a plain number, is interpolated.
 *
 * The preset is not touched either: scenes copy the edit buffer and the morph writes the edit buffer, so
 * the stored sound is the sound until the user saves it - and STORE A on an untouched sound is exactly
 * the "snapshot before editing" this page is for.
 *
 * Storage: MSC_N x (P_COUNT + 12) int16 = 1 096 B of .bss, no flash, no heap. At most one track morphs
 * at a time (the selected one): six simultaneous morphs would be 6x the RAM for a page one track owns. */
#define MSC_N 4
static int16_t msc_p[MSC_N][P_COUNT];        /* a scene: the track's whole parameter set */
static int16_t msc_mx[MSC_N][4][3];          /* .. and the matrix's slots 5..8 (track_t.mx) */
static uint8_t msc_have;                     /* one bit per scene: captured */
static uint8_t msc_a, msc_b;                 /* the pair that morphs, 0..3 */
static int16_t msc_pos, msc_want;            /* the position, 0..127 (0 = A, 127 = B) */
static uint8_t msc_on;                       /* both scenes of the pair are captured */
static int16_t msc_ui_p = 64, msc_ui_a = 1, msc_ui_b = 2, msc_ui_g;   /* the page's four knobs */
static int motion_param(uint32_t id);        /* (motion.c, included by seq.c: the sound ids) */
static unsigned str_len(const char *s);      /* (libc.c, included after this) */
static void str_cpy(char *d, const char *s, unsigned max);
static void fmt_int(char *b, int32_t v);

static uint32_t msc_guard(void)
{
#if defined(FM1_IRQ_TARGET)
    uint32_t f = fm1_icfg();
    fm1_irq_off();
    return f;
#else
    return 0;
#endif
}
static void msc_unguard(uint32_t f)
{
#if defined(FM1_IRQ_TARGET)
    fm1_icfg_set(f);
#else
    (void)f;
#endif
}

/* where a position puts a value: A + (B - A) * pos / 127, in the id's own integers */
static int16_t msc_lerp(int16_t a, int16_t b, int32_t pos)
{
    return (int16_t)(a + ((int32_t)b - a) * pos / 127);
}

/* one scene = the selected track's sound as it is now */
static __attribute__((noinline)) void msc_capture(uint32_t slot)
{
    uint32_t f, i;
    if (slot >= MSC_N)
        return;
    f = msc_guard();
    for (i = 0; i < P_COUNT; i++)
        msc_p[slot][i] = TSEL->p[i];
    for (i = 0; i < 4u; i++) {
        msc_mx[slot][i][0] = TSEL->mx[i][0];
        msc_mx[slot][i][1] = TSEL->mx[i][1];
        msc_mx[slot][i][2] = TSEL->mx[i][2];
    }
    msc_unguard(f);
    msc_have |= (uint8_t)(1u << slot);
    msc_on = (uint8_t)(((msc_have >> msc_a) & 1u) && ((msc_have >> msc_b) & 1u));
}

/* the sounding values for a position: called from the main loop, never the ISR */
/* noinline: the 125-id loop is four hundred bytes and an inline copy per caller (the four knob rows, the
 * capture, the tick) cost 3.7 KB on the AC791N link; one copy and a call is the whole saving */
static __attribute__((noinline)) void msc_apply(int32_t pos)
{
    uint32_t f, i;
    if (!msc_on)
        return;
    f = msc_guard();
    for (i = 0; i < P_COUNT; i++) {
        if (motion_param(i))                          /* a number: blended, one 127th at a time */
            TSEL->p[i] = msc_lerp(msc_p[msc_a][i], msc_p[msc_b][i], pos);
        else                                          /* a choice (MUTE, a routing): the near scene, so
                                                       * position 0 is scene A exactly and 127 is B */
            TSEL->p[i] = msc_p[pos < 64 ? msc_a : msc_b][i];
    }
    for (i = 0; i < 4u; i++) {
        TSEL->mx[i][2] = msc_lerp(msc_mx[msc_a][i][2], msc_mx[msc_b][i][2], pos);
        TSEL->mx[i][0] = (int16_t)(pos < 64 ? msc_mx[msc_a][i][0] : msc_mx[msc_b][i][0]);
        TSEL->mx[i][1] = (int16_t)(pos < 64 ? msc_mx[msc_a][i][1] : msc_mx[msc_b][i][1]);
    }
    msc_unguard(f);
}

/* the pair changed (a knob or a stored scene): arm it, and land on A unless the user moved off it */
static void msc_pair(void)
{
    if (msc_a == msc_b)
        msc_b = (uint8_t)((msc_a + 1u) % MSC_N);      /* a pair of one scene is no morph: the next one */
    msc_on = (uint8_t)(((msc_have >> msc_a) & 1u) && ((msc_have >> msc_b) & 1u));
    if (msc_on)
        msc_apply(msc_pos);
}

/* the page's knobs: params.c hands page_desc the four values above and ui_input writes a turn straight into
 * them, so nothing here needs a hook in the knob path (a call per knob cost 3.7 KB on the link, the inline
 * decision of the whole UI cascading). Once a frame this reads them and does what changed. */
static __attribute__((noinline)) void msc_sync(void)
{
    if (msc_ui_a != (int16_t)(msc_a + 1u) || msc_ui_b != (int16_t)(msc_b + 1u)) {
        msc_a = (uint8_t)clamp(msc_ui_a - 1, 0, MSC_N - 1);
        msc_b = (uint8_t)clamp(msc_ui_b - 1, 0, MSC_N - 1);
        msc_pair();
    }
    if (msc_ui_p != msc_want)                         /* the position: a knob turn is one step */
        msc_want = msc_ui_p;
    if (msc_ui_g) {                                   /* KNOB 4: an action, run once */
        if (msc_ui_g == 1 || msc_ui_g == 2) {
            msc_capture(msc_ui_g == 1 ? msc_a : msc_b);
        }
        msc_ui_g = 0;
    }
}

/* once a frame: the slew. A knob move is already one step at a time; a jump (a stored position, an
 * automation pass) is walked at 3 of 127 a frame, ~0.7 s end to end, so nothing bigger than 1/127 of
 * the pair's distance reaches the engines between two blocks */
static void msc_tick(void)
{
    int32_t d;
    msc_sync();
    if (!msc_on || msc_pos == msc_want)
        return;
    d = (int32_t)msc_want - msc_pos;
    msc_pos = (int16_t)(msc_pos + (d > 3 ? 3 : d < -3 ? -3 : d));
    msc_apply(msc_pos);
}
