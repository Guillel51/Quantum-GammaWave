/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (3.0) HOME's own screens beyond the ecosystem and the visualizer.
 *
 * HOME is the instrument's front door: the first tap from a section comes back here, and every tap after that walks
 * the same list of screens, the last one returning to the first (ui.c go_home). Two of them are here:
 *
 *   OVERVIEW  what is loaded and what it is doing: the track's engine and sound, the tempo, the transport, and the
 *             six tracks' patterns as 64 cells each, so a project's shape is read at a glance.
 *   MIXER     the six tracks' levels, pan, mute and voices at once, so a desk's last look is the same on HOME.
 *   SYSTEM    how the instrument itself is: the version, the audio ISR's measured load, the voices in use, the
 *             battery's step, the clock source and the uptime -- every row a number the firmware already keeps,
 *             none of them invented.
 *
 * Both are read-only: no parameter, no page, no store changes, so a project saved before them loads and sounds
 * exactly as it did. They paint the whole body (rows 26..201) in two 240 x 88 passes, the visualizer's own way, so
 * each frame stays inside CV_MAX (240 x 124).
 *
 * Geometry: x 4..236 is the panel; pass 0 covers rows 26..113, pass 1 rows 114..201. Text is AF_S except the sound
 * name, which is AF_M: it is the one thing a player looks for on this screen. */
/* HV_HOME .. HV_COUNT live in ui.c, above the field that holds one (ui_homeviews.c is included after it) */

/* append to a line that is being built (libc.c has str_cpy, not a cat) */
static void hv_cat(char *b, const char *s, uint32_t sz)
{
    uint32_t n = str_len(b);
    if (n < sz)
        str_cpy(b + n, s, sz - n);
}
/* a bar: w px wide, filled by v/max (a meter, a level, the load) */
static void hv_bar(int32_t x, int32_t y, int32_t w, int32_t h, int32_t v, int32_t max, uint16_t fg, uint16_t bg)
{
    int32_t f = (max > 0 && v > 0) ? (v * w) / max : 0;
    cv_rect(x, y, w, h, bg);
    cv_rect(x, y, f > w ? w : f, h, fg);
}

/* (FASE 2) the ambience (defined below, after the views that now start with it) */
static void hv_air(int32_t y0, int32_t h, uint16_t c, uint32_t seed);

/* (4.1) the ground every HOME screen sits on: four bands dithered from the panel's ceiling down to its foot, the
 * accent rising as the rows come down (gw_dither is what a 16-bit panel has instead of a blend). Drawn first, under
 * the embers and the text, and built only from palette tokens mixed by ux_mix, so GREY / MONO / FLAT still work. */
static void hv_grad(void)
{
    int32_t top = -cv_oy, bot = top + cv_h;         /* (the body is drawn in two passes: only this pass's rows) */
    /* (FASE 3) row by row, solid: the twelve dithered bands were 14 px of checker (the "pixels"), and 40 solid
     * bands were 4 px stripes. ux_ground_rows walks the same two ramps one row at a time and fills one run per
     * colour the palette has, so the steps are 1-4 px and there is no pattern. Same endpoints: 22..55 % of the
     * panel ramp, 2..24 % of the accent. */
    ux_ground_rows(6, Y_LABEL, 228, 200 - Y_LABEL, 22, 55, 2, 24, top, bot, 100);
}

/* (4.1) which of HOME's screens is on, as icons: three cells in the title's right corner (the three pages drawn here) -- where a caption used to
 * say the same thing in words -- the one on screen lit in the accent with a bar under it that breathes, the others
 * dim. The order is the order a tap walks them (HV_OVERVIEW .. HV_ROUT), so the strip is the walk itself. */
static void hv_strip(int32_t y, uint16_t c)
{
    static const uint8_t IC[3] = {ICON_X_HGRID, ICON_X_MIXER, ICON_X_PERF};
    int32_t i, x0 = 190, on = (int32_t)ui.home_view - HV_OVERVIEW;   /* (5.0) the three pages that draw it: 190..227 */
    gw_dither(x0, y + 12, 3 * 13 - 1, 2, T_SURF, ux_mix(T_SURF, T_ACCENT, 34), 50);   /* its own track */
    for (i = 0; i < 3; i++) {
        int32_t x = x0 + i * 13;
        if (i == on) {
            /* (FASE 4) ANIM OFF MEANS STILL. The breathing bar under the lit icon kept stepping from ui.frame
             * with ANIM OFF (and in LOAD LITE / ECO), so these three screens re-blitted their whole body every
             * eighth frame for a pulse the setting says should not be there -- the flicker the report names on
             * OVERVIEW, MIXER and PERFORMANCE "regardless of FLAT/LINE or ANIM ON/OFF". Still is the setting. */
            int32_t ph = ANIM_SNAP() ? 0 : (int32_t)((ui.frame / 8u) % 12u), pulse = ph < 6 ? ph : 12 - ph;
            cv_rrect(x - 1, y - 1, 13, 13, 2, c, ux_mix(c, T_TEXT, 35 + pulse * 8));   /* a 12 px icon needs a 13 px cell */
            cv_icon_on(x, y, 12, IC[i], ux_mix(c, T_BG, 88), c);                       /* dark on the accent: it reads */
            cv_rect(x + 1, y + 12, 9, 1, ux_mix(c, T_BG, 100 - pulse * 14));
        } else
            cv_icon_on(x, y, 12, IC[i], T_MID, T_SURF);
    }
}

/* the sound's name on a track: a user preset's or the engine's (ui_draw.c sound_name does the same for the footer;
 * ui_draw.c is included after this file, so it is written again here, four lines) */
static void hv_sound_name(const track_t *t, char *b)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    b[0] = 0;
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 16);
}


/* the voices a part holds now (its window into the pool, the ones still sounding) */
static uint32_t hv_voices(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        if (V(t, i).active)
            n++;
    return n;
}

/* (1.3.2) THE SCREEN'S NAME, drawn where it fits: the cap pass owns 20..45, so the title is drawn there and not
 * in the 26..114 pass, whose canvas cut the row the glyphs reach above their baseline (that was the "name cut" on
 * OVERVIEW, MIXER and PERFORMANCE). */
/* (1.4) THE BODY IS DRAWN ONLY WHEN WHAT IT SHOWS CHANGES. These four screens redrew their whole 240 x 176 body on
 * every frame -- three canvases and three blits over SPI, ~60 times a second -- whether anything had moved or not:
 * on the unit that is the flicker and the lag the owner reported, and it also spends the frame time the audio needs.
 * The signature below is a hash of the values the screens actually put on the panel, quantized the way they are drawn
 * (a meter advances by a whole segment, the embers by 1/8 of a frame), so a steady screen costs nothing and a moving
 * one repaints exactly when a pixel would differ. `ui.force` still wins: a page change, a theme change or a pop-up
 * repaints regardless. */
static uint32_t hv_sig_was;
static uint32_t hv_view_sig(void)
{
    uint32_t h = 2166136261u, i, v = 0;
    h = (h ^ (uint32_t)ui.home_view) * 16777619u;
    h = (h ^ (uint32_t)settings.palette) * 16777619u;
    /* (FASE 4) AND WITH ANIM OFF THE PHASE IS NOT PART OF THE STATE: hashing it repainted the body (and its top
     * line) every eighth frame over a picture that had not changed -- the header's band flickered with nothing
     * new to show. Meters, names and the selection still move the hash as they should. */
    if (!ANIM_SNAP())
        h = (h ^ (ui.frame / 8u)) * 16777619u;      /* the embers' phase, as hv_air steps it */
    /* (FASE 2, issue #106) THE LOAD IS NOT ON THESE SCREENS. Only the SYSTEM screen of older builds printed
 * song.cpu_q8 (it is gone: nothing reached it), and the three
     * screens that draw this body do not: hashing it repainted the whole 240 x 176 body -- three SPI blits, the
     * top line first -- on every frame the load moved, which is what flickered at the top of OVERVIEW, MIXER and
     * PERFORMANCE while the rest of the screen had nothing new to show. */
    h = (h ^ (uint32_t)song.batt_raw) * 16777619u;
    h = (h ^ (uint32_t)(song.sel + (song.rec << 4))) * 16777619u;
    h = (h ^ (uint32_t)snd_edited(TSEL)) * 16777619u;   /* (1.4.2) TSEL's engine and preset are already in the
                                                         * loop below: it is one of the tracks */
    for (i = 0; i < NTRK; i++) {
        const track_t *q = &trk[i];
        v += hv_voices(q);
        /* (FASE 2, issue #106) QUANTIZED TO THE PIXELS THAT ARE DRAWN. `>> 9` was a step of 512 units: a meter
         * whose fill had not moved a single pixel still forced the whole body (and its top line) to be blitted
         * again, up to sixty times a second. MIXER's bar has nine segments, OVERVIEW's cap twelve pixels. */
        h = (h ^ (uint32_t)((uint32_t)q->vu * 9u / 32768u)) * 16777619u;
        h = (h ^ (uint32_t)((uint32_t)q->peak * 12u / 32768u)) * 16777619u;
        h = (h ^ (uint32_t)(q->eng_req * 8u + q->preset)) * 16777619u;
        h = (h ^ (uint32_t)(q->engine * 4u + q->user)) * 16777619u;
    }
    h = (h ^ v) * 16777619u;
    return h;
}
static void hv_overview(const track_t *t, uint16_t c)
{
    char b[28], pn[20];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    hv_sound_name(t, pn);
    hv_grad();                                         /* (4.1) the ground */
    hv_air(Y_LABEL, 202 - Y_LABEL, c, 6u);                            /* (FASE 2) OVERVIEW had no ambience at all */

    /* pass 0: what is loaded and what it is doing */
    hv_strip(29, c);                                   /* (4.1) which screen this is, as an icon */
    cv_text(14, 28, &AF_S, "OVERVIEW", T_THEME);       /* (1.3.9) y = 28, not 34: AF_S's capitals are rows 3..11
                                                        * own row, clear of the panel's border and of the rule */      /* (3.5) the screen's own name, in the row the knob
                                                        * strip used to take (K1..K4 showed values nothing
                                                        * acted on: they are gone, not moved) */
    cv_rect(14, 41, 212, 1, T_LINE);                   /* (1.3.9) under the name's ink (31..39), not through it */

    cv_text(14, 50, &AF_S, "ENGINE", T_DIM);
    cv_text(86, 47, &AF_M, e->name, T_TEXT);
    {   char q[20];
        fmt_int(q, (int32_t)preset_rank(e, t->preset) + 1);   /* (FASE 2) the sound's place in the list, as
                                                              * PRESETS counts it: the legacy aliases do not */
        hv_cat(q, "/", sizeof q);
        fmt_int(q + str_len(q), (int32_t)engine_library(e));
        cv_text_r(226, 50, &AF_S, q, T_MID, T_SURF);
    }

    cv_text(14, 64, &AF_S, "SOUND", T_DIM);
    if (pn[0] && pc_side(t) == 1u) {                   /* (FASE 4/9) PRESET COMPARE: the preset side is sounding */
        uint32_t n = str_len(pn);
        if (n < sizeof pn - 2u) { pn[n] = ' '; pn[n + 1] = 'A'; pn[n + 2] = 0; }
    } else if (pn[0] && snd_edited(t)) {                /* (1.2.6) SOUND EDITED: the star the footer writes */
        uint32_t n = str_len(pn);
        if (n < sizeof pn - 1u) { pn[n] = '*'; pn[n + 1] = 0; }
    }
    cv_text(86, 62, &AF_S, pn[0] ? pn : "--", c);

    cv_text(14, 78, &AF_S, "TEMPO", T_DIM);
    {   char q[24];
        fmt_int(q, (int32_t)song.g[G_BPM]);
        hv_cat(q, " BPM", sizeof q);
        cv_text(86, 78, &AF_S, q, T_TEXT);
        str_cpy(q, "SWING ", sizeof q);
        fmt_int(q + 6, (int32_t)song.g[G_SWING]);
        hv_cat(q, "%", sizeof q);
        cv_text(150, 78, &AF_S, q, T_DIM);
    }

    cv_text(14, 92, &AF_S, "STATE", T_DIM);
    {   char q[24];
        uint32_t rec = (uint32_t)((song.rec >> song.sel) & 1u);
        str_cpy(q, song.playing ? "PLAY" : "STOP", sizeof q);
        if (rec)
            hv_cat(q, " REC", sizeof q);
        cv_text(86, 92, &AF_S, q, song.playing ? c : T_MID);
        cv_text(150, 92, &AF_S, song.g[G_CLOCK] ? "CLOCK EXT" : "CLOCK INT", T_DIM);
    }

    /* the master, as the 0..100 % the mixer shows it */
    cv_text(14, 106, &AF_S, "MASTER", T_DIM);
    hv_bar(86, 107, 100, 5, (int32_t)(song.master_q12 >> 4), 100, c, T_RAISE);
    {   char q[12];
        fmt_int(q, (int32_t)(song.master_q12 >> 4));
        hv_cat(q, "%", sizeof q);
        cv_text_r(226, 106, &AF_S, q, T_MID, T_SURF);
    }

    /* pass 1: the six parts' patterns, 64 steps each (a cell every 3 px, 2 wide) */
    cv_text(14, 118, &AF_S, "STEPS", T_THEME);
    cv_text_r(226, 118, &AF_S, "[HOME] NEXT", T_DIM, T_SURF);
    cv_rect(14, 131, 212, 1, T_LINE);
    {   uint32_t k, i;
        for (k = 0; k < NTRK; k++) {
            const track_t *q = &trk[k];
            int32_t y = 136 + (int32_t)k * 10;
            int32_t used = 0;
            b[0] = 'T';
            fmt_int(b + 1, (int32_t)k + 1);
            cv_text(14, y, &AF_S, b, k == song.sel ? c : T_MID);
            for (i = 0; i < NSTEP; i++) {
                int32_t on = q->step[i].n || q->step[i].hit;
                if (on)
                    used = 1;
                cv_rect(32 + (int32_t)i * 3, y, 2, 6, on ? (k == song.sel ? c : T_THEME) : T_RAISE);
            }
            /* the level it is making now: the peak the meters use (trk[].peak), a 12 px cap */
            hv_bar(224, y + 1, 12, 4, q->peak > 0 ? q->peak * 12 / 32767 : 0, 12,
                   k == song.sel ? c : T_THEME, T_RAISE);
            if (!used)
                cv_text(32, y, &AF_S, "-", T_DIM);
        }
    }
}


/* (3.3) the air behind HOME's screens: slow embers that drift upward and a floor of bars fed by each track's own
 * level. Everything is 1 px rects in the panel's own steps (SURF/RAISE/LINE), drawn first so the text sits on top,
 * so no palette can make it unreadable and it costs nothing but rects -- no allocation, no per-pixel path. */
static uint32_t hv_fr;
static void hv_air(int32_t y0, int32_t h, uint16_t c, uint32_t seed)
{
    uint32_t i;
    /* (3.1.2) THE PHASE IS THE FRAME, NOT A PRIVATE COUNTER. This used to be hv_fr++, so the embers stepped
     * once per call: a redraw of the same frame moved the whole field, a page drawn in two passes ran at
     * double speed, and the same state never came back to the same pixels. Drawn from ui.frame the field
     * advances with time like every other animation in the UI, and drawing a frame twice is idempotent. */
    /* (1.4) 1/8 of a frame: the field drifts slowly instead of repainting the whole body on every frame, which is
     * what keeps the signature of a quiet HOME screen still (hv_view_sig) and the SPI bus free for the audio. */
    hv_fr = ui.frame / 8u;
    for (i = 0; i < 16u; i++) {                        /* the embers: hashed, one to three px a frame */
        uint32_t s = 0x9E3779B9u * (i + seed * 7u + 1u);
        int32_t x = 8 + (int32_t)((s >> 8) % 224u);
        int32_t y = y0 + (int32_t)((uint32_t)(h - 2) - (((s >> 3) & 63u) + hv_fr * (1u + (i % 3u))) % (uint32_t)(h - 2));
        cv_rect(x, y, (i & 7u) == 0 ? 2 : 1, 1, (i & 3u) == 0 ? T_LINE : T_RAISE);
    }
    for (i = 0; i < NTRK; i++) {                       /* the floor: six bars, each the level it makes now */
        const track_t *q = &trk[i];
        int32_t v = q->vu > 0 ? (int32_t)((uint32_t)q->vu * 18u / 32767u) : 0;
        if (v > 0)
            cv_rect(20 + (int32_t)i * 34, y0 + h - v, 26, v, i == song.sel ? c : T_RAISE);
    }
}

/* MIXER (3.2): the desk, drawn like the rest of the instrument -- a frame per track, the engine it is running, a
 * segmented meter with a peak cap, the pan on a centred scale, and a MUTE pill instead of a word. Every value is one
 * the firmware already keeps (fx.c writes vu; trk[].peak is the block's peak): the screen stores nothing. */
static void hv_seg(int32_t x, int32_t y, int32_t w, int32_t h, int32_t v, int32_t max, int32_t nseg,
                   uint16_t on, uint16_t off)
{
    int32_t i, lit = (max > 0 && v > 0) ? (v * nseg) / max : 0;
    if (lit > nseg) lit = nseg;
    for (i = 0; i < nseg; i++) {
        int32_t sx = x + (i * (w + 1)) / nseg;
        int32_t ex = x + ((i + 1) * (w + 1)) / nseg - 1;
        cv_rect(sx, y, ex - sx > 1 ? ex - sx : 1, h, i < lit ? on : off);
    }
}
static void hv_mix(const track_t *t, uint16_t c)
{
    char b[28], q[16];
    uint32_t k;
    (void)t;
    hv_grad();
    hv_air(Y_LABEL, 202 - Y_LABEL, c, 1u);
    hv_strip(29, c);
    cv_text(14, 28, &AF_S, "MIXER", T_THEME);          /* (1.3.9) y = 28, not 34: AF_S's capitals are rows 3..11
                                                        * own row, clear of the panel's border and of the rule */         /* (3.5) its own title, where the knob strip was */
    cv_rect(14, 41, 212, 1, T_LINE);
    (void)q;
    for (k = 0; k < NTRK; k++) {
        const track_t *q2 = &trk[k];
        const engine_t *e = ENGINES[q2->eng_req % NENGINES];
        int32_t y = 44 + (int32_t)k * 17;
        int32_t pan = q2->p[P_PAN];
        uint16_t lc = k == song.sel ? c : T_THEME;
        int32_t lvl = q2->vu > 0 ? (int32_t)((uint32_t)q2->vu * 100u / 32767u) : 0;
        int32_t pk = q2->peak > 0 ? (int32_t)((uint32_t)q2->peak * 100u / 32767u) : 0;
        str_cpy(b, "T", sizeof b);                      /* one string, so no "T" sits on its own digit */
        fmt_int(b + 1, (int32_t)k + 1);
        cv_text(16, y, &AF_S, b, lc);
        /* the engine it is running: three letters of its own name */
        str_cpy(b, e->name, 4);
        cv_text(34, y, &AF_S, b, T_DIM);
        /* the level: nine cells, hot ones lit in the accent, the peak held beside them */
        hv_seg(62, y + 1, 62, 6, lvl, 100, 9, lvl > 84 ? T_ACCENT : lc, T_RAISE);
        cv_rect(127, y + 2, 2, 4, pk >= 97 ? T_ACCENT : pk > 0 ? T_MID : T_RAISE);
        /* the pan: a ruler, C in the middle, the tick on it (P_PAN is -64..63) */
        cv_rect(134, y + 4, 18, 1, T_RAISE);
        cv_rect(143, y + 5, 1, 2, T_LINE);
        cv_rect(142 + pan * 8 / 64, y + 2, 2, 6, lc);     /* 2 px: it stays visible when it is at C */
        /* muted (a pill, as the keys draw their state) or the voices it holds now */
        if (q2->p[P_MUTE]) {
            cv_rrect(158, y - 1, 30, 12, 3, T_RAISE, T_SURF);
            cv_text(162, y, &AF_S, "MUTE", T_ACCENT);
        } else {
            fmt_int(b, (int32_t)hv_voices(q2));
            hv_cat(b, "V", sizeof b);
            cv_text(158, y, &AF_S, b, T_MID);
        }
        /* where the track's fader is, in the same cells */
        hv_seg(180, y + 1, 44, 6, q2->p[P_LEVEL], 127, 6, k == song.sel ? c : T_MID, T_RAISE);
    }
    /* the mix itself: its own two peaks, with a scale, as a desk's last look */
    /* (v1.8.3) THE METERS STOP WHERE THE NUMBERS BEGIN. Each bar ran 132 px from x 82 to 214 while its
     * percentage is right-aligned at 226 and, at three digits plus the sign, starts around x 202: twelve
     * pixels of meter sat under the number on every row of the master band, and the LEVEL bar (from 72) came
     * within two. The bars now share one column -- x 82, 112 px wide: 194 at most -- and the numbers keep the
     * 32 px strip from 196 to 228, so a hundred-per-cent row ends a pixel before its text begins. The bar and
     * the number are the same height and the same baseline, so the row still reads as one line. */
    cv_text(16, 150, &AF_S, "MASTER", T_THEME);
    for (k = 0; k < 2u; k++) {
        int32_t y = 148 + (int32_t)k * 12, v = fx_vu[6u + k] > 0 ? (int32_t)((uint32_t)fx_vu[6u + k] * 100u / 32767u) : 0;
        cv_text(72, y, &AF_S, k ? "R" : "L", T_DIM);
        hv_seg(82, y + 1, 112, 7, v, 100, 16, k ? T_THEME : c, T_RAISE);
        fmt_int(b, v);
        hv_cat(b, "%", sizeof b);
        cv_text_r(226, y, &AF_S, b, T_MID, T_SURF);
    }
    cv_text(16, 174, &AF_S, "LEVEL", T_DIM);
    {   int32_t mv = (int32_t)(song.master_q12 >> 4);
        hv_seg(82, 175, 112, 6, mv, 100, 16, c, T_RAISE);
        fmt_int(b, mv);
        hv_cat(b, "%", sizeof b);
        cv_text_r(226, 174, &AF_S, b, T_MID, T_SURF);
    }
}

/* the four knobs HOME shows are parameter ids (eng_analog.c .knob = {P_E4, P_E5, P_ATK, P_REL}), not edit slots:
 * the engine's own eight have a descriptor with a label and a unit, a common one is named here. */
static const engine_t *e_knob_eng(const track_t *t) { return ENGINES[t->eng_req % NENGINES]; }
static uint32_t e_knob(const track_t *t, uint32_t i) { return e_knob_eng(t)->knob[i & 3u]; }
static const char *p_short(uint32_t id, char *b)
{
    switch (id) {
    case P_ATK: str_cpy(b, "ATK", 10); break;
    case P_DEC: str_cpy(b, "DEC", 10); break;
    case P_SUS: str_cpy(b, "SUS", 10); break;
    case P_REL: str_cpy(b, "REL", 10); break;
    case P_LEVEL: str_cpy(b, "LEVEL", 10); break;
    case P_PAN: str_cpy(b, "PAN", 10); break;
    case P_LRATE: str_cpy(b, "LFO RATE", 10); break;
    case P_MORPH: str_cpy(b, "MORPH", 10); break;
    default: str_cpy(b, "PARAM", 10);
    }
    return b;
}

/* PERFORMANCE (3.2): the settings a player changes between songs, on one screen -- octave, transpose, the scale the
 * keys snap to, the tempo, and the selected track's arpeggiator, which is what the four knobs on HOME turn. All of
 * them are live parameters: nothing here is a display of something that does not exist. */
static void hv_perf(const track_t *t, uint16_t c)
{
    char b[28], q[20];
    uint32_t i;
    hv_grad();
    hv_air(Y_LABEL, 202 - Y_LABEL, c, 2u);
    hv_strip(29, c);
    cv_text(14, 28, &AF_S, "PERFORMANCE", T_THEME);    /* (1.3.9) y = 28, not 34: AF_S's capitals are rows 3..11
                                                        * own row, clear of the panel's border and of the rule */   /* (3.5) its own title, where the knob strip was */
    cv_rect(14, 41, 212, 1, T_LINE);
    cv_text(14, 48, &AF_S, "OCTAVE", T_DIM);
    fmt_int(b, (int32_t)song.octave);
    cv_text(96, 48, &AF_S, b, T_TEXT);
    cv_text(130, 48, &AF_S, "TRANSPOSE", T_DIM);
    fmt_int(b, (int32_t)t->p[P_TRANS]);
    cv_text(210, 48, &AF_S, b, T_TEXT);
    cv_text(14, 62, &AF_S, "ROOT", T_DIM);
    str_cpy(b, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 8);
    cv_text(96, 62, &AF_S, b, T_TEXT);
    cv_text(130, 62, &AF_S, "SCALE", T_DIM);
    str_cpy(b, N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 10);
    cv_text(180, 62, &AF_S, b, T_TEXT);
    cv_text(14, 76, &AF_S, "TEMPO", T_DIM);
    fmt_int(b, (int32_t)song.g[G_BPM]);
    hv_cat(b, " BPM", sizeof b);
    cv_text(96, 76, &AF_S, b, T_TEXT);
    {   char n[8];
        fmt_int(n, (int32_t)song.g[G_SWING]);
        str_cpy(q, "SWING ", sizeof q);
        hv_cat(q, n, sizeof q);
        hv_cat(q, "%", sizeof q);
    }
    cv_text(150, 76, &AF_S, q, T_MID);
    cv_rect(14, 90, 212, 1, T_LINE);
    /* the arpeggiator: the four knobs HOME turns, and what they are set to */
    cv_text(14, 96, &AF_S, "ARPEGGIO", T_THEME);
    cv_text_r(226, 96, &AF_S, N_AMODE[clamp(t->p[P_AMODE], 0, (int32_t)(sizeof N_AMODE / sizeof N_AMODE[0]) - 1)],
              T_MID, T_SURF);
    {   static const char *const L[4] = {"RATE", "OCT", "GATE", "SWING"};
        uint32_t id[4];
        id[0] = P_ARATE; id[1] = P_AOCT; id[2] = P_AGATE; id[3] = P_ASWING;
        for (i = 0; i < 4u; i++) {
            int32_t y = 110 + (int32_t)i * 14;
            cv_text(16, y, &AF_S, L[i], T_DIM);
            hv_seg(72, y + 1, 128, 6, t->p[id[i]], 127, 14, c, T_RAISE);
            fmt_int(b, (int32_t)t->p[id[i]]);
            cv_text_r(226, y, &AF_S, b, T_MID, T_SURF);
        }
    }
    /* the four knobs HOME shows: their names, so the hand knows what it will turn */
    cv_text(14, 164, &AF_S, "KNOBS", T_THEME);
    for (i = 0; i < 4u; i++) {
        uint32_t id = e_knob(t, i);                    /* knob[] holds parameter ids, not edit slots */
        const param_desc_t *d = id >= P_E0 && id < P_E0 + 8u ? &e_knob_eng(t)->edit[id - (uint32_t)P_E0] : 0;
        int32_t x = 16 + (int32_t)i * 54;
        char n[10];
        const char *lab = (d && d->label) ? d->label : p_short(id, n);   /* a descriptor may carry no label */
        cv_text(x, 176, &AF_S, lab, i == 0 ? c : T_MID);
        cv_text(x, 186, &AF_S, "=", T_DIM);
        if (d) {
            if (d->fmt == F_ENUM && d->names && t->p[id] >= 0)
                str_cpy(n, d->names[t->p[id]], sizeof n);
            else {
                fmt_int(n, (int32_t)t->p[id]);
                if (d->unit) hv_cat(n, d->unit, sizeof n);
            }
        } else
            fmt_int(n, (int32_t)t->p[id]);
        cv_text(x + 9, 186, &AF_S, n, T_TEXT);
    }
}

/* SOUND ENGINE (3.2): the engine's own eight parameters, the preset it is playing, and the state of the voices --
 * a quick way to see what the sound is made of without walking the EDIT pages. */
/* MODULATION (3.2): the four matrix slots the engine renders (src -> dst, with depth) and the LFO that feeds them.
 * The rows are the parameters the DSP reads; a slot whose source or destination is off is drawn as it is: off. */
/* DYNAMICS (4.3): the part's amplitude envelope, drawn from the four times the engine actually plays (P_ATK,
 * P_DEC, P_SUS, P_REL), with a playhead walking it while the part holds a voice, and under it every part's peak as
 * a ladder. Nothing here is invented: the shape is the sum of those four parameters, the playhead only exists while
 * hv_voices() says the part is sounding, and the ladders are trk[].peak, the same number MIXER and OVERVIEW draw. */
/* the third column of a bus row: which model or time that bus is set to, from the parameter's own names */
static const char *hv_route_name(uint32_t i)
{
    if (i == 0)
        return GP[G_CTYPE].names[clamp(song.g[G_CTYPE], 0, 2)];
    if (i == 1)
        return GP[G_DTIME].names[clamp(song.g[G_DTIME], 0, GP[G_DTIME].max)];
    return GP[G_RTYPE].names[clamp(song.g[G_RTYPE], 0, 4)];
}

/* ROUTING (4.0): the path the audio of the selected part really takes, drawn from the numbers the DSP reads.
 *
 *   PART (the engine's voices) -> DIST (the insert, track_dist) -> the three sends (P_CHOR, P_DLY, P_REV)
 *        -> CHORUS / DELAY / REVERB buses (fx_buses) -> MASTER (EQ, WIDTH, clip, limiter) -> L / R
 *
 * Every figure on it is a live one, not a diagram's decoration: the insert and the sends are the part's own
 * parameters, and each bus row carries two bars -- what the bus received this block (fx_vu[0..2], the peaks
 * fx_buses measures from the sends) and what it returned (fx_vu[3..5]) -- so a send turned down shows its
 * input collapsing and a bus that is not in the path reads zero because no audio reached it. The two bars at
 * the foot are the same master peaks the MIXER and the WAVES page draw (fx_vu[6..7]).
 *
 * The rail on the left is the flow: one stage a row, the chain read top to bottom across both passes. */
/* the screen HOME is showing (HV_OVERVIEW, HV_MIX, HV_SYSTEM): one 240 x 176 body in two 88-row passes */
static void graph_home_view(uint16_t c)
{
    uint32_t pass, sig = hv_view_sig();
    if (!ui.force && sig == hv_sig_was)
        return;                                     /* (1.4) nothing on this screen would differ: no blits at all */
    hv_sig_was = sig;
    /* (1.3.2) THE BODY OPENS AT Y_LABEL (20), where HOME's cards do, not at 26: the six rows the panel left
     * uncovered showed the top edge of the four cards on every screen but the main one, and the screen's own
     * name -- drawn on the first row inside the panel -- had its cap row cut by the panel's border. A 26-row cap
     * paints the panel's rounded top from 20 and the two 88-row passes cover 26..202 as before; each pass draws
     * the panel with its rounded ends off its own window, so no corner lands in the middle of the body. The cap
     * stops at 20 and never reaches the header's band (0..16), whose icons the lint watches. */
    /* (1.3.2) THE PLATE OPENS AT Y_LABEL (20), where HOME's cards do, not at 26: those six rows kept the cards'
     * upper edge on every screen but the main one. A 26-row cap paints the rounded top from 20 and the two 88-row
     * passes cover 26..202 as they always did -- the boundaries sit between text rows, which is what the render
     * lint measures -- and each pass draws the panel with its rounded ends outside its own window, so no corner
     * lands in the middle of the body. The screens' names moved to y = 34 so their ink (30..39) starts below the
     * first pass's own top instead of on it. */
    cv_begin(240, 6, T_BG);                            /* (v1.5.15) rows 20..26, was 20..46: the first pass below
                                                        * draws 26..114 in this same frame, so the cap's 26..46 were
                                                        * painted and then painted over -- 20 rows of panel traffic a
                                                        * step, and the two windows' pixels differed, so the shared
                                                        * rows were sent twice. 6 rows: the plate's top edge only */
    cv_oy = -Y_LABEL;
    cv_bg = T_SURF;
    cv_rrect(4, Y_LABEL, 232, 202 - Y_LABEL, 6, T_SURF, T_BG);
    /* (1.3.9) THE CAP PAINTS THE SAME GROUND AS THE BODY. It used to fill the plate with T_SURF flat while the
     * 88-row passes painted hv_grad()/hv_air() over the same rows, so the six rows above the first pass read as a
     * lighter band across the top of the screen -- the "top of HOME showing through" on OVERVIEW, MIXER and
     * PERFORMANCE. The gradient runs from Y_LABEL on the same ramp, so rows 20..25 continue into row 26. */
    hv_grad();                                        /* (1.3.9) and no hv_air in the cap: the field's bodies and
                                                        * dust start at 26 with the first pass, as they always did,
                                                        * and six rows of separate specks would only show the seam */
    cv_blit(0, Y_LABEL);
    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? 114 : 26;
        cv_begin(240, 88, T_BG);
        cv_oy = -top_y;
        cv_bg = T_SURF;
        cv_rrect(4, 6, 232, 196, 6, T_SURF, T_BG);
        if (ui.home_view == HV_OVERVIEW)
            hv_overview(TSEL, c);
        else if (ui.home_view == HV_MIX)
            hv_mix(TSEL, c);
        else if (ui.home_view == HV_PERF)
            hv_perf(TSEL, c);
        else
            hv_perf(TSEL, c);                        /* (5.0) the walk's last screen: PERFORM */
        cv_blit(0, (uint32_t)top_y);
    }
}
