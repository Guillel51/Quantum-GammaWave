/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 7) QUANTUM TUNING: the twelve cents, where they are applied and what they must not disturb.
 *
 * A temperament is invisible until it is wrong: a note that lands 3 cents off its own deviation sounds fine on
 * its own and beats against the fifth. These are the checks that catch it:
 *   - precision. The grid pitches live on is 1/16 semitone, 6.25 cents; a deviation has to reach the engine as
 *     the cents it was asked for, so every case is measured as a frequency ratio and converted back to cents.
 *   - limits. +/-99 cents is the knob's end; a corrupted byte is clamped, never a note class off.
 *   - the right note. The table is read from the pitch itself, so a transposed note, an octave up and a
 *     detuned second oscillator all get their own class's cents and not the one that happened to sound first.
 *   - pitch bend and the standard tuning. Bend is added on top of the temperament and stays what it was; with
 *     every deviation at zero the increment is exactly pitch_inc()'s, bit for bit.
 *   - live change and recovery. Picking a profile changes what the engines read without touching a voice, and a
 *     settings record from any older version imports as EQUAL rather than as a made-up temperament.
 * Host only: this is the model, not a device and not the emulator. Exit 1 on any assertion.
 *   cc -O1 -w -DUI_NO_DRIFT=1 -Ibuild/gen -Ifirmware/src -Itests -o build/host/tune_test tests/tune_test.c -lm
 */
#include "../build/capture/ui_render_nomain.c"

static int bad;

static void ok(const char *what, int cond)
{
    printf("tune: %s %s\n", what, cond ? "ok" : "FAILED");
    if (!cond)
        bad++;
}

/* the deviation a temperament gives a note, measured on the increment the engine will use: the ratio between
 * it and equal temperament's, in cents */
static double cents_of(int32_t p16)
{
    double a = (double)tune_inc(p16), b = (double)pitch_inc((uint32_t)clamp(p16, 0, 2047));
    return 1200.0 * (a > b ? log(a / b) : -log(b / a)) / log(2.0);
}

int main(void)
{
    uint32_t n;
    int32_t k;
    char big[64];

    /* --- 1. EQUAL is exactly what the instrument played before this existed ------------------------------ */
    tune_init();
    {
        int same = 1;
        for (k = 0; k < 128; k++)
            same &= tune_inc(k * 16) == pitch_inc((uint32_t)(k * 16));
        ok("EQUAL: the increment is pitch_inc()'s, every note of the keyboard, bit for bit", same);
    }

    /* --- 2. precision: the cents asked for are the cents measured --------------------------------------- */
    {
        double worst = 0;
        static const int32_t want[] = {1, -1, 5, -5, 12, -12, 14, -14, 25, -31, 50, -50, 99, -99};
        for (n = 0; n < sizeof want / sizeof want[0]; n++) {
            double got;
            tune_equal();
            tune_set(0, want[n]);                   /* C */
            got = cents_of(60 * 16);
            if (fabs(got - (double)want[n]) > worst)
                worst = fabs(got - (double)want[n]);
        }
        snprintf(big, sizeof big, "precision: worst error %.3f cents over the knob's whole sweep (1..99, both ways)",
                 worst);
        ok(big, worst < 0.5);
    }

    /* --- 3. limits: the knob's ends, and a corrupted table ---------------------------------------------- */
    {
        int okv = 1;
        tune_equal();
        tune_set(0, 400);
        okv &= tune_get(0) == 99;
        tune_set(0, -400);
        okv &= tune_get(0) == -99;
        tune_set(-1, 10);                           /* a note that does not exist is not a crash */
        tune_set(12, 10);
        okv &= tune_get(0) == -99 && tune_get(11) == 0;
        ok("limits: +-99 is the end of the sweep; a note past the twelve is refused, not written", okv);
    }

    /* --- 4. the right note: class by class, octave after octave, transposed ------------------------------ */
    {
        int okv = 1;
        tune_equal();
        for (n = 0; n < 12u; n++)
            tune_set((int32_t)n, (int32_t)n - 6);   /* -6 .. +5, twelve different values */
        for (n = 0; n < 12u; n++) {
            double c3 = cents_of((int32_t)(48 + n) * 16), c5 = cents_of((int32_t)(72 + n) * 16);
            double c8 = cents_of((int32_t)(96 + n) * 16);
            okv &= fabs(c3 - (double)((int32_t)n - 6)) < 0.5;
            okv &= fabs(c5 - c3) < 0.05 && fabs(c8 - c3) < 0.05;   /* the same class, three octaves apart */
        }
        ok("the right note: each class keeps its own cents, and an octave up keeps it too", okv);
    }

    /* --- 5. transposition and the sequencer's own pitches ----------------------------------------------- */
    {
        int okv = 1, tr;
        tune_equal();
        tune_set(2, 20);                            /* D */
        for (tr = -24; tr <= 24; tr += 12) {        /* the track's TRANS, in semitones */
            int32_t p16 = (62 + tr) * 16;           /* D4 */
            okv &= fabs(cents_of(p16) - 20.0) < 0.5;
        }
        okv &= fabs(cents_of(62 * 16 + 0) - 20.0) < 0.5;   /* and the 1/16 offsets the pfx lane adds */
        okv &= fabs(cents_of(62 * 16 + 7) - 20.0) < 0.6;
        ok("transposition: a D is a D at any TRANS setting, and with a 1/16 sub-step it stays D", okv);
    }

    /* --- 6. pitch bend rides on top, and does not eat the temperament ------------------------------------ */
    {
        int32_t p16 = 64 * 16;                      /* E4 */
        double plain, bent, diff;
        tune_equal();
        tune_set(4, 15);                            /* E, 15 cents sharp */
        plain = cents_of(p16);
        bent = 1200.0 * log((double)cents_inc(p16, 50, 0) / (double)pitch_inc((uint32_t)p16)) / log(2.0);
        diff = bent - plain;
        snprintf(big, sizeof big, "bend: 50 cents of wheel on top of a 15 cent deviation gives %.2f more cents",
                 diff);
        ok(big, fabs(plain - 15.0) < 0.5 && fabs(diff - 50.0) < 0.5);
    }

    /* --- 7. live change: picking a profile is what the engines read, next block -------------------------- */
    {
        int okv, slots_ok;
        tune_init();
        tune_pick(4);                               /* WERCK */
        okv = fabs(cents_of(64 * 16) - (double)TUNE_TEMP[4][4]) < 0.6;
        tune_pick(1);                               /* JUST: its third is the one that is famously flat */
        okv &= cents_of(64 * 16) < -10.0;
        tune_pick(0);
        okv &= tune_inc(64 * 16) == pitch_inc(64 * 16);
        slots_ok = !tune_save(0) && !tune_save(4);  /* a factory slot is never written */
        tune_equal();
        tune_set(9, 33);
        slots_ok &= tune_save(5) && tune_get(9) == 33;
        tune_equal();
        slots_ok &= tune_get(9) == 0;
        tune_pick(5);                               /* USER 1 gives back what was saved */
        slots_ok &= tune_get(9) == 33;
        ok("live: a profile picked is a profile sounding, and USER 1 keeps what was saved onto it, "
           "a factory slot refuses to be written", okv && slots_ok);
    }

    /* --- 8. recovery: an old settings record, and a corrupt one ----------------------------------------- */
    {
        persist_t p;
        int r;
        memset(&p, 0, sizeof p);
        p.magic = PERSIST_MAGIC;
        p.panel = PANEL_DEFAULT;
        memcpy(p.tune[0].ct, "\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63", 12);   /* 99 everywhere */
        p.tune[0].all = 0;
        r = settings_import(&p, (int)sizeof p);
        {
            int okv = r == 1;
            for (n = 0; n < 12u; n++)
                okv &= tune_slot[0].ct[n] == 99;    /* 99 is the knob's end, not a clamp away from it */
            memcpy(&p.tune[0].ct, "\x7f\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 12);   /* 127, -128 */
            okv &= settings_import(&p, (int)sizeof p) == 1;
            okv &= tune_slot[0].ct[0] == 99 && tune_slot[0].ct[1] == -99;
            memset(&p, 0, sizeof p);
            p.magic = 0x50455234u;                  /* PER4: the record before the temperaments existed */
            p.panel = PANEL_DEFAULT;
            r = settings_import(&p, (int)(sizeof p - sizeof p.tune));
            okv &= r == 2;
            {
                int eq = 1;
                for (n = 0; n < 12u; n++)
                    eq &= tune_slot[0].ct[n] == 0;
                okv &= eq && tune_inc(60 * 16) == pitch_inc(60 * 16);
            }
            ok("recovery: 99 is 99, a corrupt byte is clamped to the knob's end, and a PER4 record imports "
               "as EQUAL", okv);
        }
        memset(&p, 0, sizeof p);
        p.magic = 0xDEADBEEFu;
        ok("recovery: a record with no magic we know is refused, and the table keeps what it had",
           settings_import(&p, (int)sizeof p) == 0 && tune_inc(60 * 16) == pitch_inc(60 * 16));
    }

    /* --- 9. the page is where the model is: SCL > TUNE exists and its four knobs are the model's --------- */
    {
        uint32_t i, found = 0;
        int okv = 0;
        for (i = 0; i < NPAGES; i++) {
            const page_t *pg = &PAGES[i];
            int16_t *vp;
            const param_desc_t *d0, *d1, *d2, *d3;
            if (pg->graph != GR_SCL_TUNE)
                continue;
            found = 1;
            d0 = page_desc(pg, 0, &vp);
            d1 = page_desc(pg, 1, &vp);
            d2 = page_desc(pg, 2, &vp);
            d3 = page_desc(pg, 3, &vp);
            okv = d0 && d1 && d2 && d3 && d0->fmt == F_ENUM && d0->max == TUNE_SLOTS - 1 &&
                  !strcmp(d0->label, "PROFILE") && d1->fmt == F_NOTE && d2->fmt == F_SEMI &&
                  d2->min == -TUNE_CMAX && d2->max == TUNE_CMAX && d3->fmt == F_SEMI && found;
        }
        ok("the page: SCL has a TUNE page whose PROFILE NOTE CENTS REF are the model's own four numbers",
           okv && found);
    }

    /* --- 10. the page's knobs reach the table, and the page draws what the table says ------------------- */
    {
        int okv;
        tune_init();
        tune_ui_slot = 0;
        tune_ui_slot_l = -1;
        tune_ui_sync(0);                            /* the page's first frame: the slot settles */
        tune_ui_note = 7;                           /* G */
        tune_ui_ct = -22;
        tune_ui_all = -32;                          /* A 432 */
        tune_ui_sync(0);                            /* then the knobs, one turn each */
        okv = tune_get(7) == -22 && tune_live.all == -32;
        okv &= fabs(cents_of(67 * 16) - (-22.0 - 32.0)) < 0.7;
        tune_back(0);                               /* the way out: EQUAL, in the table and in the knobs */
        okv &= tune_get(7) == 0 && tune_live.all == 0 && tune_ui_slot == 0 && tune_ui_ct == 0 && tune_ui_all == 0;
        okv &= tune_inc(67 * 16) == pitch_inc(67 * 16);
        ok("the page's knobs: NOTE, CENTS and REF reach the table, and BACK gives equal temperament again", okv);
    }

    printf("tune: %d FAILED\n", bad);
    return bad != 0;
}
