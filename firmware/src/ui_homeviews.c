/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (3.0) HOME's own screens beyond the ecosystem and the visualizer.
 *
 * HOME is the instrument's front door: the first tap from a section comes back here, and every tap after that walks
 * the same list of screens, the last one returning to the first (ui.c go_home). Two of them are here:
 *
 *   OVERVIEW  what is loaded and what it is doing: the track's engine and sound, the tempo, the transport, and the
 *             six tracks' patterns as 64 cells each, so a project's shape is read at a glance.
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

/* ui_draw.c's battery steps (batt_raw: 531 / 561 / 591 are the thresholds its icon uses), so HOME and the footer
 * never disagree about the charge */
static int32_t hv_batt(void)
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
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

static void hv_overview(const track_t *t, uint16_t c)
{
    char b[28], pn[20];
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    hv_sound_name(t, pn);

    /* pass 0: what is loaded and what it is doing */
    cv_text(14, 30, &AF_S, "OVERVIEW", T_THEME);
    fmt_int(b, (int32_t)song.sel + 1);
    {   char s[20];
        str_cpy(s, "TRACK ", sizeof s);
        hv_cat(s, b, sizeof s);
        hv_cat(s, " / 6", sizeof s);
        cv_text_r(226, 30, &AF_S, s, c, T_SURF);
    }
    cv_rect(14, 43, 212, 1, T_LINE);

    cv_text(14, 50, &AF_S, "ENGINE", T_DIM);
    cv_text(86, 47, &AF_M, e->name, T_TEXT);
    {   char q[20];
        fmt_int(q, (int32_t)t->preset + 1);
        hv_cat(q, "/", sizeof q);
        fmt_int(q + str_len(q), (int32_t)e->npresets);
        cv_text_r(226, 50, &AF_S, q, T_MID, T_SURF);
    }

    cv_text(14, 64, &AF_S, "SOUND", T_DIM);
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

static void hv_system(uint16_t c)
{
    char b[28];
    uint32_t v = 0, k;
    for (k = 0; k < NTRK; k++)
        v += hv_voices(&trk[k]);

    cv_text(14, 30, &AF_S, "SYSTEM", T_THEME);
    cv_text_r(226, 30, &AF_S, FELUCCA_VERSION, T_MID, T_SURF);
    cv_rect(14, 43, 212, 1, T_LINE);

    /* the audio ISR's load: audio.c song.cpu_q8, averaged over CPU_AVG blocks, as the graphs' five-segment bar
     * (ui_draw.c cpu_seg) and GLOBAL's own row read it */
    cv_text(14, 50, &AF_S, "AUDIO CPU", T_DIM);
    hv_bar(96, 51, 96, 6, (int32_t)(song.cpu_q8 * 100u / 256u), 100, c, T_RAISE);
    fmt_int(b, (int32_t)(song.cpu_q8 * 100u / 256u));
    hv_cat(b, "%", sizeof b);
    cv_text_r(226, 50, &AF_S, b, T_TEXT, T_SURF);

    cv_text(14, 64, &AF_S, "VOICES", T_DIM);
    hv_bar(96, 65, 96, 6, (int32_t)v, (int32_t)(NVOICE * NTRK), c, T_RAISE);
    fmt_int(b, (int32_t)v);
    cv_text_r(226, 64, &AF_S, b, T_TEXT, T_SURF);

    cv_text(14, 78, &AF_S, "BATTERY", T_DIM);
    for (k = 0; k < 4u; k++) {
        uint16_t fg = (int32_t)k <= hv_batt() ? ((int32_t)k == 0 && hv_batt() == 0 ? T_RAISE : c) : T_RAISE;
        cv_rect(96 + (int32_t)k * 13, 78, 11, 6, fg);
    }
    if (!song.batt_raw)                                  /* (0: the ADC has not read it yet) */
        cv_text(150, 78, &AF_S, "NOT READ", T_DIM);

    cv_text(14, 92, &AF_S, "UPTIME", T_DIM);
    fmt_int(b, (int32_t)(fm1_ms / 3600000u));
    hv_cat(b, "h ", sizeof b);
    {   uint32_t m = (fm1_ms / 60000u) % 60u;
        char t[4];
        if (m < 10u)
            hv_cat(b, "0", sizeof b);
        fmt_int(t, (int32_t)m);
        hv_cat(b, t, sizeof b);
        hv_cat(b, "m", sizeof b);
    }
    cv_text(96, 92, &AF_S, b, T_TEXT);

    cv_text(14, 106, &AF_S, "CLOCK", T_DIM);
    cv_text(96, 106, &AF_S, song.g[G_CLOCK] ? "EXTERNAL (MIDI)" : "INTERNAL", T_TEXT);

    /* pass 1: what each part is using right now */
    cv_text(14, 118, &AF_S, "PARTS", T_THEME);
    cv_text_r(226, 118, &AF_S, "[HOME] NEXT", T_DIM, T_SURF);
    cv_rect(14, 131, 212, 1, T_LINE);
    for (k = 0; k < NTRK; k++) {
        const track_t *q = &trk[k];
        int32_t y = 136 + (int32_t)k * 10, n = (int32_t)hv_voices(q);
        const engine_t *e = ENGINES[q->eng_req % NENGINES];
        str_cpy(b, "T", sizeof b);
        fmt_int(b + 1, (int32_t)k + 1);
        cv_text(14, y, &AF_S, b, k == song.sel ? c : T_MID);
        cv_text(32, y, &AF_S, e->name, k == song.sel ? T_TEXT : T_MID);
        hv_bar(120, y + 1, 60, 4, n, NVOICE, k == song.sel ? c : T_THEME, T_RAISE);
        fmt_int(b, n);
        cv_text_r(226, y, &AF_S, b, n ? T_TEXT : T_DIM, T_SURF);
    }
}

/* the screen HOME is showing (HV_OVERVIEW, HV_SYSTEM): one 240 x 176 body in two 88-row passes */
static void graph_home_view(uint16_t c)
{
    uint32_t pass;
    for (pass = 0; pass < 2u; pass++) {
        int32_t top_y = pass ? 114 : 26;
        cv_begin(240, 88, T_BG);
        cv_oy = -top_y;
        cv_bg = T_SURF;
        cv_rrect(4, 26, 232, 176, 6, T_SURF, T_BG);
        if (ui.home_view == HV_OVERVIEW)
            hv_overview(TSEL, c);
        else
            hv_system(c);
        cv_blit(0, (uint32_t)top_y);
    }
}
