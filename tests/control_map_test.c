/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The page -> control -> parameter map, swept as a whole.
 *
 * The 1.4.1 report left the "every control -> DSP" audit partial, and the LFO SHAPE bug of that release was
 * exactly this class: a page whose knobs shared a value, so turning one moved the others. This test walks every
 * page of every section, in every engine, at every state a computed page can be in, and asserts:
 *
 *   1. two slots of one page never address the same value;
 *   2. a slot that returns a descriptor always returns the value it reads and writes, and vice versa;
 *   3. a slot the page table leaves empty (0xFF) returns neither -- unless the page computes its slots (the
 *      CHARACTER bank, LFO SHAPE, the FX buses, the macros, MOD, MSEQ, the slicer's pitch), which is the map;
 *   4. the value a slot points at is the one the page names: for a plain slot the offset is the page's id, in
 *      the track's parameters or in the song's globals depending on the page's scope;
 *   5. MASTER > STEREO M/S edits the master width and not a track parameter: P_DXIMG (P_FM3_REL) and G_MWIDTH are
 *      both 74, and the ENV VEL branch of page_desc() matched the bare number before the scope check, so the
 *      width page answered VEL and wrote the selected track's FM3 release (found by this sweep, fixed in 1.4.1).
 *
 * Class: host x86-64, the real sources (tests/hostsim.c includes core.h, params.c, voice.c, seq.c). Nothing here
 * says anything about the FM-1's own CPU, flash or screen.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad, checks;
static void check(const char *what, int ok, const char *page, uint32_t slot)
{
    checks++;
    if (ok) return;
    printf("controls: FAIL %-58s page '%s' slot %u\n", what, page, slot);
    bad++;
}

/* the slots a page computes instead of taking them from its id[] table */
static int computed_page(const page_t *pg)
{
    return pg->id[0] == P_X0 ||          /* CHARACTER: the engine's bank, and MORPH on slot 3 */
           pg->id[0] == P_X2 ||          /* LFO SHAPE: SLEW / INVERT and two empty slots */
           pg->graph == GR_FX_BUSES ||   /* FX > BUSES: KNOB 1 links, 2..4 edit the link's parameters */
           pg->graph == GR_SEQTOOLS ||   /* SEQ > TOOLS: the four actions, three of them not parameters */
           pg->graph == GR_MOD ||        /* MOD: the slot shown picks ids, and 5..8 have none of their own */
           pg->graph == GR_MSEQ ||       /* the level of the step shown */
           pg->graph == GR_SLPIT ||      /* the pitch of the step shown */
           pg->graph == GR_SCL_PROG;     /* KNOB 1 is the progression, not a parameter id */
}

static void sweep_page(const page_t *pg)
{
    int16_t *vp[4] = {0, 0, 0, 0};
    const param_desc_t *d[4] = {0, 0, 0, 0};
    uint32_t s, k;
    for (s = 0; s < 4u; s++)
        d[s] = page_desc(pg, s, &vp[s]);
    for (s = 0; s < 4u; s++) {
        check("a descriptor comes with its value", !d[s] || vp[s], pg->title, s);
        check("a value comes with its descriptor", !vp[s] || d[s], pg->title, s);
        check("a label is printed", !d[s] || d[s]->label[0], pg->title, s);
        /* 3. an empty slot in the table answers with nothing */
        if (!computed_page(pg) && pg->id[s] == 0xFF)
            check("an empty slot has no control", !d[s] && !vp[s], pg->title, s);
        /* 4. a plain slot edits the parameter the page names */
        /* (FASE 6) the SCENES page's four rows are virtual ids too: their values are morph.c's (morph_test
         * checks that a turn edits them), not a p[] or song.g slot */
        if (!computed_page(pg) && pg->id[s] != 0xFF && vp[s] && !IS_MAC_ID(pg->id[s]) && !IS_MSC_ID(pg->id[s])) {
            if (pg->scope == SC_TRACK)
                check("the slot edits the id the page names", vp[s] == &TSEL->p[pg->id[s]], pg->title, s);
            else if (pg->scope == SC_GLOBAL && pg->id[s] != G_TRKS && pg->id[s] != G_LOADP)
                check("the slot edits the global the page names", vp[s] == &song.g[pg->id[s]], pg->title, s);
        }
        /* 1. two knobs of one page never share one target */
        if (!vp[s]) continue;
        for (k = s + 1u; k < 4u; k++)
            check("two knobs never share one value", vp[s] != vp[k], pg->title, s);
    }
}

static void sweep_all(void)
{
    uint32_t i;
    for (i = 0; i < NELEM(PAGES); i++)
        sweep_page(&PAGES[i]);
    /* the computed pages, at every state they compute from */
    for (i = 0; i < 8u; i++) {                       /* FX > BUSES: the eight links, KNOB 2..4 each */
        fxb_tab = (uint8_t)i;
        {
            uint32_t p;
            for (p = 0; p < NELEM(PAGES); p++)
                if (PAGES[p].graph == GR_FX_BUSES)
                    sweep_page(&PAGES[p]);
        }
    }
    fxb_tab = 0;
    for (i = 0; i < 8u; i++) {                       /* MOD: the eight matrix slots */
        mod_ui_slot = (uint8_t)i;
        {
            uint32_t p;
            for (p = 0; p < NELEM(PAGES); p++)
                if (PAGES[p].graph == GR_MOD)
                    sweep_page(&PAGES[p]);
        }
    }
    mod_ui_slot = 0;
    for (i = 0; i < 16u; i++) {                      /* MSEQ / SL PITCH: the step shown */
        uint32_t p;
        ms_ui_step = (uint8_t)i;
        sl_ui_step = (uint8_t)i;
        for (p = 0; p < NELEM(PAGES); p++)
            if (PAGES[p].graph == GR_MSEQ || PAGES[p].graph == GR_SLPIT)
                sweep_page(&PAGES[p]);
    }
    ms_ui_step = 0;
    sl_ui_step = 0;
}

static const page_t *page_named(const char *title)
{
    uint32_t i;
    for (i = 0; i < NELEM(PAGES); i++)
        if (!strcmp(PAGES[i].title, title))
            return &PAGES[i];
    return 0;
}

/* 5. the collision the sweep found: STEREO M/S is the master width, VOICE 2's slot 3 is ENV VEL, and neither
 * owns the other's value. */
static void stereo_width(void)
{
    const page_t *pg = page_named("STEREO M/S");
    int16_t *vp = 0;
    const param_desc_t *d;
    int16_t before;
    check("STEREO M/S exists", pg != 0, "STEREO M/S", 0);
    if (!pg) return;
    d = page_desc(pg, 0, &vp);
    check("STEREO M/S is the master width", d && vp == &song.g[G_MWIDTH] && !strcmp(d->label, "M-WIDE") &&
                                                 d->def == 64, "STEREO M/S", 0);
    before = TSEL->p[P_DXIMG];
    if (vp) *vp = 100;
    check("the width knob does not touch the track's FM3 release", TSEL->p[P_DXIMG] == before, "STEREO M/S", 0);
    check("the width knob writes the width", song.g[G_MWIDTH] == 100, "STEREO M/S", 0);
    /* the same number on the track page is still ENV VEL */
    pg = page_named("VOICE 2");
    check("VOICE 2 exists", pg != 0, "VOICE 2", 0);
    if (!pg) return;
    d = page_desc(pg, 2, &vp);
    check("VOICE 2's slot 3 is still ENV VEL",
          d && vp == &TSEL->p[P_DXIMG] && !strcmp(d->label, "VEL"), "VOICE 2", 2);
}

int main(void)
{
    uint32_t e, k, seen = 0;
    host_tracks_init();
    for (k = 0; k < NPART; k++)
        host_preset(&trk[k], 0, 0);
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e))
            continue;
        TSEL->eng_req = (int16_t)e;
        sweep_all();
        seen++;
    }
    stereo_width();
    printf("controls: %u engines x %u pages at every computed state, %d checks, %d failed\n",
           seen, (unsigned)NELEM(PAGES), checks, bad);
    return bad ? 1 : 0;
}
