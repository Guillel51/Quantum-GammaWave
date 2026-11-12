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

/* (3.3) the air behind HOME's screens: slow embers that drift upward and a floor of bars fed by each track's own
 * level. Everything is 1 px rects in the panel's own steps (SURF/RAISE/LINE), drawn first so the text sits on top,
 * so no palette can make it unreadable and it costs nothing but rects -- no allocation, no per-pixel path. */
static uint32_t hv_fr;
static void hv_air(int32_t y0, int32_t h, uint16_t c, uint32_t seed)
{
    uint32_t i;
    hv_fr++;
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
    hv_air(26, 176, c, 1u);
    cv_text(14, 30, &AF_S, "MIXER", T_THEME);
    fmt_int(q, (int32_t)song.sel + 1);
    {   char s2[16];
        str_cpy(s2, "SEL T", sizeof s2);
        hv_cat(s2, q, sizeof s2);
        cv_text_r(226, 30, &AF_S, s2, c, T_SURF);
    }
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
    cv_text(16, 150, &AF_S, "MASTER", T_THEME);
    for (k = 0; k < 2u; k++) {
        int32_t y = 148 + (int32_t)k * 12, v = fx_vu[6u + k] > 0 ? (int32_t)((uint32_t)fx_vu[6u + k] * 100u / 32767u) : 0;
        cv_text(72, y, &AF_S, k ? "R" : "L", T_DIM);
        hv_seg(82, y + 1, 132, 7, v, 100, 16, k ? T_THEME : c, T_RAISE);
        fmt_int(b, v);
        hv_cat(b, "%", sizeof b);
        cv_text_r(226, y, &AF_S, b, T_MID, T_SURF);
    }
    cv_text(16, 174, &AF_S, "LEVEL", T_DIM);
    {   int32_t mv = (int32_t)(song.master_q12 >> 4);
        hv_seg(72, 175, 132, 6, mv, 100, 16, c, T_RAISE);
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
    hv_air(26, 176, c, 2u);
    cv_text(14, 30, &AF_S, "PERFORMANCE", T_THEME);
    cv_text_r(226, 30, &AF_S, "LIVE", c, T_SURF);
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
static void hv_engine(const track_t *t, uint16_t c)
{
    char b[28], pn[20];
    uint32_t i;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    hv_sound_name(t, pn);
    hv_air(26, 176, c, 3u);
    cv_text(14, 30, &AF_S, e->name, T_THEME);
    fmt_int(b, (int32_t)(t->preset % (e->npresets ? e->npresets : 1u)) + 1);
    hv_cat(b, "/", sizeof b);
    {   char n[8];
        fmt_int(n, (int32_t)e->npresets);
        hv_cat(b, n, sizeof b);
    }
    cv_text_r(226, 30, &AF_S, b, T_MID, T_SURF);
    cv_text(14, 44, &AF_S, pn[0] ? pn : "NO NAME", c);
    cv_rect(14, 58, 212, 1, T_LINE);
    for (i = 0; i < 8u; i++) {
        int32_t col = (int32_t)(i & 1u), row = (int32_t)(i >> 1);
        int32_t x = 16 + col * 108, y = 64 + row * 20;
        const param_desc_t *d = &e->edit[i];
        int32_t v = t->p[P_E0 + i];
        cv_text(x, y, &AF_S, d->label, T_DIM);
        if (d->fmt == F_ENUM && d->names && v >= 0)
            str_cpy(b, d->names[v], 12);
        else {
            fmt_int(b, v);
            if (d->unit)
                hv_cat(b, d->unit, sizeof b);
        }
        cv_text(x, y + 9, &AF_S, b, col ? T_MID : T_TEXT);
    }
    cv_rect(14, 148, 212, 1, T_LINE);
    cv_text(14, 154, &AF_S, "VOICES", T_DIM);
    fmt_int(b, (int32_t)hv_voices(t));
    hv_cat(b, " / ", sizeof b);
    {   char n[8];
        fmt_int(n, (int32_t)(e->poly ? e->poly : NVOICE));
        hv_cat(b, n, sizeof b);
    }
    cv_text(80, 154, &AF_S, b, T_TEXT);
    cv_text(150, 154, &AF_S, "MORPH", T_DIM);
    fmt_int(b, (int32_t)t->p[P_MORPH]);
    cv_text(200, 154, &AF_S, b, T_MID);
    cv_text(14, 168, &AF_S, "FILTER", T_DIM);
    fmt_int(b, (int32_t)t->p[P_E4]);
    hv_cat(b, " CUT", sizeof b);
    cv_text(80, 168, &AF_S, b, T_TEXT);
    fmt_int(b, (int32_t)t->p[P_E5]);
    hv_cat(b, " RES", sizeof b);
    cv_text(160, 168, &AF_S, b, T_MID);
}

/* MODULATION (3.2): the four matrix slots the engine renders (src -> dst, with depth) and the LFO that feeds them.
 * The rows are the parameters the DSP reads; a slot whose source or destination is off is drawn as it is: off. */
static void hv_mod(const track_t *t, uint16_t c)
{
    char b[24], q[24];
    uint32_t i;
    static const uint32_t SRC[4] = {P_M1SRC, P_M2SRC, P_M3SRC, P_M4SRC};
    static const uint32_t DST[4] = {P_M1DST, P_M2DST, P_M3DST, P_M4DST};
    static const uint32_t AMT[4] = {P_M1AMT, P_M2AMT, P_M3AMT, P_M4AMT};
    hv_air(26, 176, c, 4u);
    cv_text(14, 30, &AF_S, "MODULATION", T_THEME);
    cv_text_r(226, 30, &AF_S, "MATRIX", T_MID, T_SURF);
    cv_text(14, 44, &AF_S, "LFO", T_THEME);
    fmt_int(b, (int32_t)t->p[P_LRATE]);
    cv_text(52, 44, &AF_S, b, T_TEXT);
    fmt_int(b, (int32_t)t->p[P_LWAVE]);
    str_cpy(q, "W", 4);
    hv_cat(q, b, sizeof q);
    cv_text(120, 44, &AF_S, q, T_MID);
    fmt_int(b, (int32_t)t->p[P_LPHASE]);
    str_cpy(q, "PH ", 4);
    hv_cat(q, b, sizeof q);
    cv_text(168, 44, &AF_S, q, T_MID);
    cv_rect(14, 58, 212, 1, T_LINE);
    for (i = 0; i < 4u; i++) {
        int32_t y = 64 + (int32_t)i * 20;
        int32_t src = t->p[SRC[i]], dst = t->p[DST[i]], amt = t->p[AMT[i]];
        int32_t on = src > 0 && amt != 0;
        fmt_int(b, (int32_t)i + 1);
        cv_text(16, y, &AF_S, b, on ? c : T_DIM);
        fmt_int(b, src);
        cv_text(30, y, &AF_S, b, on ? T_TEXT : T_DIM);
        cv_text(52, y, &AF_S, ">", T_DIM);
        fmt_int(b, dst);
        cv_text(64, y, &AF_S, b, on ? T_TEXT : T_DIM);
        hv_seg(88, y + 1, 96, 6, amt < 0 ? -amt : amt, 127, 12, on ? (amt > 96 ? T_ACCENT : c) : T_RAISE, T_RAISE);
        fmt_int(b, amt);
        cv_text_r(226, y, &AF_S, b, on ? T_MID : T_DIM, T_SURF);
    }
    cv_rect(14, 148, 212, 1, T_LINE);
    /* the LFO's own four destinations (P_LD_*), the ones the engine renders without a matrix slot */
    cv_text(14, 154, &AF_S, "LFO TO", T_THEME);
    {   static const char *const L[4] = {"PITCH", "FILTER", "SHAPE", "AMP"};
        static const uint32_t ID[4] = {P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
        for (i = 0; i < 4u; i++) {
            int32_t y = 168 + (int32_t)(i >= 2 ? 12 : 0);
            int32_t x = 16 + (int32_t)(i & 1u) * 108;
            int32_t v = t->p[ID[i]];
            cv_text(x, y, &AF_S, L[i], T_DIM);
            hv_seg(x + 52, y + 1, 44, 6, v < 0 ? -v : v, 127, 6, v ? c : T_RAISE, T_RAISE);
        }
    }
}

/* the screen HOME is showing (HV_OVERVIEW, HV_MIX, HV_SYSTEM): one 240 x 176 body in two 88-row passes */
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
        else if (ui.home_view == HV_MIX)
            hv_mix(TSEL, c);
        else if (ui.home_view == HV_PERF)
            hv_perf(TSEL, c);
        else if (ui.home_view == HV_ENG)
            hv_engine(TSEL, c);
        else if (ui.home_view == HV_MOD)
            hv_mod(TSEL, c);
        else
            hv_system(c);
        cv_blit(0, (uint32_t)top_y);
    }
}
