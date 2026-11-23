/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 7) QUANTUM TUNING.
 *
 * A temperament is twelve numbers: how many cents each pitch class sits away from equal temperament. That is
 * all this file keeps -- 12 signed bytes a profile, plus one cents offset for the whole profile (A 440, A 432,
 * a tape a quarter tone flat) -- and it is applied in exactly one place: where a note's pitch in 1/16 semitone
 * becomes a phase increment (tune_inc() below, and cents_inc() in dsp.c for the engines that detune in cents).
 *
 * Why there and not at note-on: pitch16 is 1/16 semitone, 6.25 cents, so a temperament written into it would
 * land on the nearest 6.25 cents and a JUST third would be as wrong as equal temperament. tune_inc() adds the
 * remainder the way cents_inc() already does -- (inc >> 12) * cents * 2367 / 1000, ~0.06 cents of resolution --
 * so the deviation is what was asked for and not what the grid allows.
 *
 * Cost, and why it is this shape:
 *   - RAM: 14 bytes for the live profile and 14 a slot on the shelf (8 slots: five factory, three the player
 *     saves onto). Nothing per voice, per track or per note: the note class comes out of the pitch itself,
 *     (pitch16 >> 4) % 12, so a held note, a glide, a transposed chord and a MIDI note all read the same table.
 *   - CPU: one table load, one multiply and one add, inside the pitch computation each engine already does once
 *     per block (not per sample). With every deviation at 0 the multiply is skipped entirely (tune_inc returns
 *     pitch_inc's value untouched), so EQUAL temperament costs a branch and nothing else.
 *   - XIP: five temperaments of twelve bytes, no strings per note, no per-engine copy of the maths.
 *
 * What it does not touch: pitch bend (pfx_rpit and MIDI's RPN ride on top of pitch16 and are added after), the
 * song format, presets, the scale and chord machinery (a temperament is a frequency, a scale is a set of notes:
 * the two compose), and the drums, whose pitch is a kit parameter and not a note of the keyboard.
 *
 * A profile is saved in the settings object (settings_persist.c, PER5) and not in the song: a temperament is a
 * property of the instrument in the room, the way the palette is, and loading someone else's project must not
 * re-tune the player's instrument. An older settings record imports as every deviation 0 = EQUAL, which is what
 * it sounded like. */
#ifndef TUNE_C_INCLUDED      /* one copy a translation unit: engines.c and ui_scl.c both ask for it, and the
                             * firmware and every host test compile it through whichever comes first */
#define TUNE_C_INCLUDED

#define TUNE_FACT 5u           /* EQUAL JUST PYTH MEAN WERCK: the tables are code, never data */
#define TUNE_USER 3u           /* USER 1..3: the only slots RAM (and the settings object) keeps */
#define TUNE_SLOTS (TUNE_FACT + TUNE_USER)
#define TUNE_CMAX 99           /* the page's own limit: a semitone is 100, and past it the class is the next one */
static void settings_save(void);   /* project.c: the settings object (FELUCA_FLASH), declared before the page uses it */
static int32_t tune_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

typedef struct {
    int8_t ct[12];             /* cents away from equal temperament, per pitch class, C first */
    int8_t all;                /* cents added to every note: A 440 -> A 432 is -32 */
} tune_t;                  /* 13 bytes: the factories are TUNE_TEMP's rodata, only the user slots are stored */

/* The factory temperaments, in cents against equal temperament, C first. JUST is the 5-limit C major scale
 * (its third 13.7 cents flat, its fifth 2 cents sharp); PYTH is the Pythagorean chain from C; MEAN is the
 * quarter-comma meantone whose fifths are 5.4 cents narrow; WERCK is Werckmeister III. Compiled in, never
 * written: SAVE refuses a factory slot rather than let a project quietly redefine "JUST". */
static const int8_t TUNE_TEMP[TUNE_FACT][12] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                        /* EQUAL */
    {0, 12, 4, 16, -14, -2, -10, 2, 14, -16, 18, -12},            /* JUST */
    {0, -10, 4, -6, 8, -2, 6, 2, -8, 10, -4, 12},                 /* PYTH */
    {0, -24, -7, 10, -14, 3, -21, -3, 14, -10, 7, -17},           /* MEAN */
    {0, -10, -3, 10, -14, -2, -10, -3, 14, -16, 6, -12},          /* WERCK */
};
static const char *const N_TUNE[] = {"EQUAL", "JUST", "PYTHAG", "MEAN", "WERCK", "USER 1", "USER 2", "USER 3"};

static tune_t tune_live;                 /* what the engines read */
static tune_t tune_slot[TUNE_USER];      /* USER 1..3: the shelf (settings_persist.c keeps it) */
static uint8_t tune_dirty;               /* the live table has been edited away from its slot */

/* the cents of the pitch class p16 belongs to, plus the profile's whole offset. p16 is 1/16 semitone. */
static int32_t tune_cents(int32_t p16)
{
    int32_t c = (p16 >> 4) % 12;
    if (c < 0)
        c += 12;
    return tune_live.ct[c] + tune_live.all;
}

/* phase increment for pitch16 with the temperament applied: pitch_inc()'s value, plus the part of a semitone the
 * 1/16 grid cannot hold. 1 cent is 2367/1000 of (inc >> 12): the same constant cents_inc() uses. */
static uint32_t tune_inc(int32_t pitch16)
{
    int32_t ct = tune_cents(pitch16), k;
    uint32_t inc = pitch_inc(tune_clamp(pitch16, 0, 2047));
    if (!ct)
        return inc;
    /* (FASE 7) the second term of 2^x - 1 = x + x^2/2 + ...: without it a 99 cent deviation lands 2.8 cents off,
     * which is a temperament that beats. 683 per cent squared per million is x^2/2 in the same 1/4096 units the
     * first term is in, and the error at the knob's end is then 0.06 cents (tests/tune_test.c measures it). */
    k = ct * 2367 / 1000 + ct * ct * 683 / 1000000;
    return inc + (uint32_t)((int32_t)(inc >> 12) * k);
}

/* the shelf: a slot loaded into the live table is what sounds; UNC (an index past the factories) never is */
static void tune_pick(uint32_t slot)
{
    uint32_t n;
    if (slot >= TUNE_SLOTS)
        return;
    if (slot < TUNE_FACT) {                 /* a factory temperament: twelve bytes of rodata, copied */
        for (n = 0; n < 12u; n++)
            tune_live.ct[n] = TUNE_TEMP[slot][n];
        tune_live.all = 0;
    } else {
        tune_live = tune_slot[slot - TUNE_FACT];
    }
    tune_dirty = 0;
}
static uint32_t tune_slot_of(void)
{
    uint32_t k, n;
    for (k = 0; k < TUNE_FACT; k++) {
        for (n = 0; n < 12u && tune_live.ct[n] == TUNE_TEMP[k][n]; n++)
            ;
        if (n == 12u && !tune_live.all)
            return k;
    }
    for (k = 0; k < TUNE_USER; k++) {
        for (n = 0; n < 12u && tune_live.ct[n] == tune_slot[k].ct[n]; n++)
            ;
        if (n == 12u && tune_live.all == tune_slot[k].all)
            return TUNE_FACT + k;
    }
    return 0xFFu;
}
static void tune_set(int32_t note, int32_t ct)
{
    if (note < 0 || note > 11)
        return;
    tune_live.ct[note] = (int8_t)tune_clamp(ct, -TUNE_CMAX, TUNE_CMAX);
    tune_dirty = tune_slot_of() == 0xFFu;
}
static int32_t tune_get(int32_t note)
{
    return note < 0 || note > 11 ? 0 : tune_live.ct[note];
}
/* SAVE: a user slot only, and the whole live table goes in (its twelve cents and its offset) */
static int tune_save(uint32_t slot)
{
    if (slot < TUNE_FACT || slot >= TUNE_SLOTS)
        return 0;
    tune_slot[slot - TUNE_FACT] = tune_live;
    tune_dirty = 0;
    return 1;
}
/* back to equal temperament, in the live table and in slot 0: the way out of a profile that does not fit */
static void tune_equal(void)
{
    memset(&tune_live, 0, sizeof tune_live);
    tune_dirty = 0;
}
static void tune_init(void)
{
    memset(tune_slot, 0, sizeof tune_slot);   /* USER 1..3 start at EQUAL */
    tune_pick(0);
}

/* ============================================================================================
 * (FASE 7) THE PAGE'S FOUR KNOBS, AS VALUES A KNOB CAN HOLD.
 *
 * params.c's page_desc() hands these four numbers to the encoders -- PROFILE, NOTE, CENTS, REF -- and
 * tune_ui_sync() moves whatever changed into the live table. The knobs write a number; this decides what the
 * number means, so the page cannot edit a table the engines do not read and the test can drive the model
 * without a display.
 *
 * A user slot stores itself: a temperament is a setting of the instrument, like the palette, so an edit made
 * on USER 1 is kept in that slot and flushed to the settings object a couple of seconds after the last turn
 * (tune_flush(), called once a frame by the page): one flash write per edit, not one per detent. An edit made
 * over a factory slot sounds and stays marked as edited, but never overwrites the temperament it started
 * from.
 * ========================================================================================== */
static int16_t tune_ui_slot, tune_ui_note, tune_ui_ct, tune_ui_all;
static int16_t tune_ui_slot_l = -1, tune_ui_note_l = -1, tune_ui_ct_l, tune_ui_all_l;
static uint32_t tune_edit_frame;         /* the frame of the last edit: the flush waits on it */
static uint8_t tune_flush_pend;          /* something to write in the settings object */

static void tune_ui_from_live(int32_t note)
{
    tune_ui_note = tune_ui_note_l = (int16_t)note;
    tune_ui_ct = tune_ui_ct_l = (int16_t)tune_get(note);
    tune_ui_all = tune_ui_all_l = tune_live.all;
}

/* whatever the encoders left in the four numbers, applied once. Returns 1 when the live table changed. */
static int tune_ui_sync(uint32_t frame)
{
    uint32_t slot = (uint32_t)tune_clamp((int32_t)tune_ui_slot, 0, (int32_t)TUNE_SLOTS - 1);
    uint32_t note = (uint32_t)tune_clamp((int32_t)tune_ui_note, 0, 11);
    int32_t ct = tune_clamp((int32_t)tune_ui_ct, -TUNE_CMAX, TUNE_CMAX);
    int32_t all = tune_clamp((int32_t)tune_ui_all, -TUNE_CMAX, TUNE_CMAX);
    int changed = 0;
    if ((int32_t)slot != tune_ui_slot_l) {          /* a profile was picked: the note's cents follow it */
        tune_pick(slot);
        tune_ui_slot_l = (int16_t)slot;
        tune_ui_from_live((int32_t)note);
        tune_ui_slot = (int16_t)slot;
        tune_flush_pend = 1;
        tune_edit_frame = frame;
        return 1;
    }
    if ((int32_t)note != tune_ui_note_l) {
        tune_ui_note_l = (int16_t)note;
        tune_ui_ct = tune_ui_ct_l = (int16_t)tune_get((int32_t)note);
    }
    if (ct != (int32_t)tune_ui_ct_l) {
        tune_ui_ct_l = (int16_t)ct;
        tune_set((int32_t)note, ct);
        changed = 1;
    }
    if (all != (int32_t)tune_ui_all_l) {
        tune_ui_all_l = (int16_t)all;
        tune_live.all = (int8_t)all;
        tune_dirty = 1;
        changed = 1;
    }
    if (changed) {
        tune_edit_frame = frame;
        tune_flush_pend = 1;
        if (slot >= TUNE_FACT)                      /* the user slot keeps what was just played */
            tune_slot[slot - TUNE_FACT] = tune_live;
    }
    return changed;
}
/* one flash write per edit gesture: the settings object, once the knobs have been still for two seconds */
static void tune_flush(uint32_t frame)
{
    if (tune_flush_pend && frame - tune_edit_frame > 120u) {
        tune_flush_pend = 0;
        settings_save();
    }
}
/* tapping a note of the circle picks it for KNOB 2 (and the cents knob follows it) */
static void tune_note_pick(int32_t note, uint32_t frame)
{
    if (note < 0 || note > 11)
        return;
    tune_ui_from_live(note);
    tune_ui_sync(frame);
}
/* "back to standard tuning": EQUAL, the slot the page starts on */
static void tune_back(uint32_t frame)
{
    tune_ui_slot = tune_ui_slot_l = 0;
    tune_pick(0);
    tune_ui_from_live(tune_ui_note);
    tune_flush_pend = 1;
    tune_edit_frame = frame;
}
#endif /* TUNE_C_INCLUDED */
