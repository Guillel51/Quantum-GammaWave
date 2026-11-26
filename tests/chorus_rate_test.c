/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (this phase) THE CHORUS'S RATE, CAPPED AT 6.00 Hz, ON EVERY PATH.
 * The bus used to read LFO_INC[G_CRATE], the track LFO's own table, which runs to 40 Hz: the page, an encoder, a
 * CC, an old preset or a restored project could hand the engine any rate up to 40 Hz. The bus reads CH_INC (its own
 * pair of tables, 0.05 .. 6.00 Hz) through chorus_inc(), which clamps the byte instead of wrapping the table, and
 * the page prints what the table holds (F_CHHZ). This checks the table's ends and shape, the clamp on every value
 * the stores can produce, the rate the DSP actually walks at RATE 127, and that the pages the phase merged are the
 * ones PAGES now has.
 *
 * build: cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests tests/chorus_rate_test.c -lm
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *w, int ok)
{
    printf("chorus: %-84s %s\n", w, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the page table's page with this name (0: none) */
static const page_t *page_named(const char *n, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < NELEM(PAGES); i++)
        if (str_len(PAGES[i].title) == len && !memcmp(PAGES[i].title, n, len))
            return &PAGES[i];
    return 0;
}

int main(void)
{
    uint32_t i, n;
    int mono = 1;

    /* 1. the table: 0.05 Hz at 0, 6.00 Hz at 127, never down as the knob goes up */
    check("the bus's table starts at 0.05 Hz and ends at exactly 6.00 Hz",
          CH_HZ_X100[0] == 5u && CH_HZ_X100[127] == 600u);
    for (i = 1; i < 128u; i++)
        mono &= CH_HZ_X100[i] >= CH_HZ_X100[i - 1u];
    check("the table never falls as RATE goes up", mono);

    /* 2. the clamp: every byte a store can hold, and the ones it should not */
    check("chorus_inc clamps a stored byte below 0 and above 127",
          chorus_inc(-9) == CH_INC[0] && chorus_inc(0) == CH_INC[0] &&
          chorus_inc(127) == CH_INC[127] && chorus_inc(1000) == CH_INC[127] &&
          chorus_inc(300) == CH_INC[127]);

    /* 3. the DSP's own rate at RATE 127: one block of fx_buses walks the chorus's phase 6.00 Hz worth */
    {
        static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
        uint32_t before, after;
        double hz;
        memset(trk, 0, sizeof trk);
        host_tracks_init();
        song.g[G_CRATE] = 127;
        song.g[G_CTYPE] = 0;
        for (i = 0; i < CTL; i++)
            c[i] = d[i] = r[i] = 20000;
        fx.cho_ph = 0;
        before = fx.cho_ph;
        fx_buses(c, d, r, w, CTL);
        after = fx.cho_ph;
        /* cho_ph advances chorus_inc() * n / CTL per block, and that value is h * CTL / FS * 2^32: cycles a second */
        hz = (double)(uint32_t)(after - before) * (double)CTL / (double)CTL;
        n = (uint32_t)(after - before);
        hz = (double)n * (double)FS / ((double)CTL * 4294967296.0);
        printf("chorus:   (RATE 127 walks %.3f Hz; 6.00 is the cap)\n", hz);
        check("the audio path's own rate at RATE 127 is 6.00 Hz, never the LFO table's 40", hz > 5.95 && hz < 6.05);
        /* and a byte past the table (a corrupt project) cannot push it further */
        song.g[G_CRATE] = 4000;
        fx.cho_ph = 0;
        before = fx.cho_ph;
        fx_buses(c, d, r, w, CTL);
        n = (uint32_t)(fx.cho_ph - before);
        hz = (double)n * (double)FS / ((double)CTL * 4294967296.0);
        check("a stored byte past 127 still walks 6.00 Hz, not a wrapped rate", hz > 5.95 && hz < 6.05);
    }

    /* 4. the page prints the table: F_CHHZ's formatter at both ends */
    {
        char val[8];
        const char *unit = 0;
        param_format(GP + G_CRATE, 127, val, &unit);
        printf("chorus:   (RATE 127 prints %s%s)\n", val, unit ? unit : "");
        check("the page prints 6.00 Hz at the top of the knob", unit && str_eq(unit, "Hz") && str_eq(val, "6.00"));
        param_format(GP + G_CRATE, 0, val, &unit);
        printf("chorus:   (RATE 0 prints %s%s)\n", val, unit ? unit : "");
        check("and the 0.05 Hz floor at the bottom (the instrument drops the leading zero)",
              unit && str_eq(unit, "Hz") && str_eq(val, ".05"));
    }

    /* 5. the merged pages: one for CURVE + SLEW + INVERT, one for DETUNE + TILT, and the two that went exist no more */
    {
        const page_t *p = page_named("LFO CURVE", 9);
        check("LFO CURVE holds CURVE, SLEW and INVERT", p && p->id[0] == P_LSHP && p->id[1] == P_X2 && p->id[2] == P_X3);
        check("LFO SHAPE is gone", !page_named("LFO SHAPE", 9));
        p = page_named("TILT", 4);
        check("TILT holds DETUNE and TILT", p && p->id[0] == P_DETUNE && p->id[1] == P_HARM);
        check("VOICE 3 (DETUNE alone) is gone", !page_named("VOICE 3", 7));
        p = page_named("CHORUS", 6);
        check("CHORUS draws its own graph", p && p->graph == GR_CHORUS && p->id[0] == G_CRATE);
        /* the knobs of the merged pages answer with their own descriptors and their own bytes */
        {
            int16_t *bp = 0;
            const param_desc_t *d;
            uint32_t pi = 0;
            song.sel = 0;
            for (i = 0; i < NELEM(PAGES); i++)
                if (&PAGES[i] == page_named("LFO CURVE", 9))
                    pi = i;
            d = page_desc(PAGES + pi, 1, &bp);
            check("LFO CURVE knob 2 is SLEW and writes SLEW",
                  d && str_eq(d->label, "SLEW") && bp == &TSEL->p[P_X2]);
            d = page_desc(PAGES + pi, 2, &bp);
            check("LFO CURVE knob 3 is INVERT and writes INVERT",
                  d && str_eq(d->label, "INVERT") && bp == &TSEL->p[P_X3]);
            d = page_desc(PAGES + pi, 0, &bp);
            check("LFO CURVE knob 1 is still CURVE", d && str_eq(d->label, "CURVE") && bp == &TSEL->p[P_LSHP]);
            /* (3.0) the fourth card: the LFO's RETRIG, PHASE read as OFF/ON (-1 FREE = OFF) */
            TSEL->p[P_LPHASE] = 64;
            d = page_desc(PAGES + pi, 3, &bp);
            check("LFO CURVE knob 4 is RETRIG", d && str_eq(d->label, "RETRIG"));
            check("  RETRIG shows ON when a note restarts the LFO (PHASE 64)", bp && *bp == 1);
            TSEL->p[P_LPHASE] = -1;
            d = page_desc(PAGES + pi, 3, &bp);
            check("  RETRIG shows OFF when PHASE is FREE", d && bp && *bp == 0);
            TSEL->p[P_LPHASE] = 0;
        }
    }

    printf("chorus rate test %s\n", bad ? "FAILED" : "passed");
    return bad != 0;
}
