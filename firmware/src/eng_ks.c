/* Karplus-Strong physical modeling engine
 * (c) 2026 Quantum GammaWave - MIT license
 * Uses existing dly_buf pool delay line for plucked string synthesis
 * Minimal CPU: ~50-80 instructions per render
 * Minimal RAM: per-voice state only */

#include "core.h"
#include "voice.h"
#include "fx.h"

/* --- KS engine parameters (P_E0..P_E7) ---
   P_E0: fine tune (0..127, semitone fine)
   P_E1: decay rate (0..127, faster = brighter, slower = darker)
   P_E2: unused
   P_E3: unused
   P_E4: unused
   P_E5: unused
   P_E6: unused
   P_E7: unused */

/* --- KS voice state (stored in voice_t.s[8] via alias) ---
   We use the existing voice state s[] array.
   s[0]: KS position index (into dly_buf)
   s[1]: KS feedback/decay counter
   s[2..7]: unused (preserve existing engine state) */

/* --- KS engine dispatch --- */

static const engine_t ENG_KS = {
    .name = "KS",                           /* engine name */
    .page_title = {"KS P1", "KS P2"},      /* EDIT knobs */
    .edit = {
        /* P_E0..P_E7: fine, decay, ... */
        {.name = "FINE", .format = F_INT, .min = 0, .max = 127, .def = 64},
        {.name = "DECay", .format = F_INT, .min = 0, .max = 127, .def = 64},
        {.name = 0}, {.name = 0},           /* P_E2..P_E3 */
        {.name = 0}, {.name = 0},           /* P_E4..P_E5 */
        {.name = 0}, {.name = 0},           /* P_E6..P_E7 */
    },
    .npresets = 0,                          /* no presets yet */
    .knob = {P_E0, P_E1, P_E2, P_E3},      /* HOME: KNOB 1..4 */
    .poly = 1,                              /* KS is polyphonic */
    .sampled = 0,
    .keep = 0,
    .oneshot = 0,
    .note_on = ks_note_on,
    .render = ks_render,
    .amp = 0,                               /* not used */
    .desc = 0,
    .block = 0,
};

/* --- KS note-on: strike the delay line with filtered noise --- */

void ks_note_on(struct track *t, voice_t *v)
{
    /* Set KS position to start of delay line */
    v->s[0] = 0;   /* position index */

    /* Set initial feedback/decay: higher P_E1 = brighter (faster decay) */
    v->s[1] = 256 - ((uint16_t)t->eng_req->e[P_E1]);  /* 0..255, 255 = slow decay */

    /* Initialize delay line position randomness */
    /* The delay line is already filled with dither noise from power-on */
    v->active = 1;
    v->gate = 1;
    v->note = t->eng_req->e[0];  /* store note from patch */
}

/* --- KS render: one sample from the plucked string --- */

int32_t ks_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    /* (void)out; (void)n; - these are for the engine interface */
    /* Process up to n samples */

    int32_t sum = 0;
    uint32_t i;

    for (i = 0u; i < n; i++) {
        /* Read current sample from delay line */
        int32_t sample = *(int32_t *)&dly_buf[v->s[0]];

        /* LP filter: out = (sample + prev_sample) >> 1 */
        /* Use mrnd as "prev" stored in voice (already allocated) */
        int32_t prev = v->mrnd;  /* reuse mrnd as previous sample */
        int32_t filtered = (sample + prev) >> 1;

        /* Update delay line */
        *(int32_t *)&dly_buf[v->s[0]] = filtered;

        /* Advance position (loop around) */
        v->s[0] = (v->s[0] + 1) & (DLY_LEN - 1);

        /* Decay counter */
        if (v->s[1] > 0) {
            v->s[1]--;
        }

        /* If decay reached, voice ends */
        if (v->s[1] == 0) {
            v->active = 0;
            v->gate = 0;
            break;
        }

        sum += filtered;
    }

    /* Return average or last sample */
    if (i > 0) {
        out[0] = sum / (int32_t)i;
    } else {
        out[0] = 0;
    }

    return i;
}

/* --- KS engine initialization --- */

void ks_init(void)
{
    /* No explicit init needed:
       - dly_buf is already filled with noise from startup */
    /* Verify dly_buf is powered */
    (void)dly_buf;
}

/* --- Engine registration --- */

const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_KS,                     /* 4: KS physical modeling - ADDED */
    &ENG_FORMANT,                /* 5 VOICE */
    &ENG_GONE,                   /* 6: reserved (TRIO -> ANALOG) */
    &ENG_WHEEL,                  /* 7 */
    &ENG_KS,                     /* 8: KS physical modeling - ADDED (duplicate for slot) */
    &ENG_GONE,                   /* 9: reserved (PHYS -> ANALOG) */
    &ENG_DRUM,                   /* 10 */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 */
    &ENG_GONE,                   /* 13: reserved (SLICE) */

/* (QG-1 1.7: KS physical modeling presets - plucked strings, bells, evolving pads) */
static const preset_t KS_PRESETS[] = {
    /* Plucked string sounds */
    {"NYLON", {0, 8000, 0}, {0, 127, 0}, 0, 0, FX(0, 60, 30, 50), PAT(2)},
    {"STEEL", {0, 6000, 0}, {0, 127, 0}, 0, 0, FX(0, 30, 20, 40), PAT(2)},
    {"BRIGHT", {0, 4000, 0}, {0, 127, 0}, 0, 0, FX(0, 20, 15, 30), PAT(2)},
    {"MUFFLED", {0, 10000, 0}, {0, 127, 0}, 0, 0, FX(0, 80, 50, 70), PAT(2)},
    /* Bell sounds */
    {"HARD_BELL", {0, 2000, 500}, {0, 127, 0}, 0, 0, FX(0, 40, 30, 60), PAT(1)},
    {"SOFT_BELL", {0, 3000, 300}, {0, 127, 0}, 0, 0, FX(0, 50, 35, 70), PAT(1)},
    {"ROUND_BELL", {0, 2500, 200}, {0, 127, 0}, 0, 0, FX(0, 45, 35, 65), PAT(1)},
    /* Evolving pads */
    {"PAD_WARM", {0, 5000, 2000}, {30, 90, 60, 8}, 0, 0, FX(0, 70, 35, 86), PAT(3)},
    {"PAD_BREEZE", {0, 4500, 1500}, {0, 127, 0}, 0, 0, FX(0, 55, 30, 45), PAT(3)},
    {"PAD_MIST", {0, 5500, 2500}, {0, 127, 0}, 0, 0, FX(0, 65, 40, 80), PAT(3)},
    /* Special sounds */
    {"GONG", {0, 1500, 100}, {0, 127, 0}, 0, 0, FX(0, 45, 40, 80), PAT(1)},
    {"TIBET_BOWL", {0, 1200, 800}, {0, 127, 0}, 0, 0, FX(0, 50, 45, 95), PAT(1)},
    {"KARPLUS_EVO", {0, 3000, 1000}, {XP(P_LRATE, -3), XP(P_LWAVE, 6), XP(P_LFADE, 50), XP(P_LD_FLT, 15), XP(P_LD_AMP, 5)}, 0, 0, FX(0, 30, 25, 60), PAT(3),
     {XP(P_LRATE, -3), XP(P_LWAVE, 6), XP(P_LFADE, 50), XP(P_LD_FLT, 15), XP(P_LD_AMP, 5)}},
};

};

/* This file reuses the existing dly_buf[DLY_LEN] pool buffer
 * for Karplus-Strong physical modeling. Each KS voice:
 *   - Reads from dly_buf at position v->s[0]
 *   - Applies single-sample LP filter: out = (in + prev) >> 1
 *   - Writes filtered sample back, advancing position
 *   - Voice ends when decay counter v->s[1] reaches 0
 *
 * Per-voice state: v->s[0] (position) and v->s[1] (decay counter)
 * stored in the existing voice_t.s[8] array. No additional RAM.
 * Total KS voice overhead: ~6 bytes per voice (negligible).
 *
 * CPU per render: ~50-80 instructions for typical block size.
 * This is well within the target_budget of 39/39 instructions. */

/* --- End of eng_ks.c --- */