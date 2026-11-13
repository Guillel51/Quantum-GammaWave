/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.1, 6.4, FASE 4) FX > BUSES: the routing console. One page that shows the chain the audio actually takes --
 * the track's sends, the three global buses (delay, reverb, chorus), the slicer on its bus, the drum bus's punch
 * and the master -- and lets the four knobs edit whichever link is selected: KNOB 1 walks TRACK / BUSES / DLY /
 * REV / CHO / SLI / PCH / MST, and KNOB 2..4 are that link's own parameters, the same ids the other FX pages
 * edit (params.c, page_desc). Nothing here is a new DSP stage: every figure is read from the parameter the
 * engine plays and every meter from the peak fx.c measured for that bus (fx_vu), so a block that is off draws
 * no activity and a busy one draws its own signal.
 *
 * (FASE 4) THE PAGE WAS REBUILT AGAIN, and this time the width was the point. The 6.4 cut put six 76 x 40
 * blocks in three columns and left the bottom third of the canvas empty, with a caption line ("1 CHAIN
 * 2-4 PARAMS") under them doing the work of a legend nobody reads twice. Now:
 *   - the chain is a single row of eight 27 px cells, each with the bus's own icon and a three-letter name, and
 *     a dot that travels that row while signal is moving through it: the path is read, not guessed;
 *   - the six blocks are two columns of 116 x 34, so a drawing is three times wider than before (an echo
 *     train, a tail and a chorus sweep all read better sideways) and the canvas' height is used to the foot;
 *   - every block carries its effect's icon, its lamp, the send and the return of its bus as two real meters
 *     (fx_vu, the same peaks the engine publishes) and a floor that warms up (gw_dither, the 16-bit ramp);
 *   - the footer caption is gone: the four knob cards above already say what the knobs do.
 * Each drawing keeps its own kind:
 *   DELAY    the repeats, TIME apart, each one FEEDBK softer, the wet peak as their height;
 *   REVERB   the tail's decay: SIZE sets how long it rings, the wet peak how tall it starts;
 *   CHORUS   three taps as waves, their phase running while the bus carries signal, DEPTH their sweep;
 *   SLICER   the eight gates of PAT, the step the sequencer is on lit;
 *   PUNCH    the drum bus's transient against the master peak, and the dip DUCK takes out of it;
 *   MASTER   the gain stage and the output peak, the clip lamp latched while the DAC sits at full scale.
 * Animation only runs on real signal: a silent engine leaves the page still. */
#define FXB_N 8u
/* (FASE 4) the chain is read by its icons: eight 27 px cells cannot hold a 12 px glyph and three letters
 * without the letters spilling out of their cell, and the four cards above already name the selected link */
/* the six blocks, left to right and top to bottom: which bus each reads */
enum { FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST, FXB_NB };
static const char *const FXB_BLOCKN[FXB_NB] = {"DELAY", "REVERB", "CHORUS", "SLICER", "PUNCH", "MASTER"};
/* (FASE 4) the icon of each block, and of each link of the chain: the effect's own glyph, the same the pages of
 * that effect put beside its title (icons.c) */
static const uint8_t FXB_BLOCKI[FXB_NB] = {ICON_DELAY, ICON_REVERB, ICON_CHORUS, ICON_SLICE, ICON_ACCENT, ICON_LEVEL};
/* the block's own header: its icon, its title, and the lamp that lights with the bus */
static void fxb_head(int32_t x, int32_t y, int32_t w, uint32_t ic, const char *name, uint16_t ink, int32_t lvl)
{
    cv_icon_on(x + 3, y + 2, 12, ic, ink, T_BG);
    cv_text_on(x + 17, y + 2, &AF_S, name, ink, T_BG);   /* (6.4) inside the block's own ground, never above it */
    /* (Solar Heat) the activity lamp: 3 px at rest, 4 px with the bus carrying signal, and the colour walks
     * from the block's own ink to the palette's heat as the level climbs -- the page's only motion a silent
     * engine cannot produce. */
    if (lvl > 0)
        cv_rect(x + w - 8, y + 2, 4, 4, ux_mix(ink, T_ACCENT, clamp(lvl * 100 / 4000, 0, 100)));
    else
        cv_rect(x + w - 7, y + 3, 3, 3, ink);       /* the activity lamp */
}
/* a level 0..7000 (fx_vu's scale) as a 0..56 px bar height */
static int32_t fxb_px(int32_t v) { return clamp(v, 0, 7000) * 56 / 7000; }
/* (Solar Heat) the drawing area of a block as a dithered ramp, cold overhead and warm at the floor: two bands of
 * gw_dither's 4 x 4 pattern, which is what a 16-bit panel had instead of a gradient. The pattern never moves and
 * the colours are the palette's own tokens mixed by ux_mix, so GREY / MONO / FLAT keep working and a still page
 * draws a still frame. */
static void fxb_grad(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t bh = h / 2;
    if (bh <= 0)
        return;
    gw_dither(x, y, w, bh, ux_mix(T_BG, T_SURF, 42), ux_mix(T_BG, T_ACCENT, 8), 50);
    gw_dither(x, y + bh, w, h - bh, ux_mix(T_BG, T_SURF, 62), ux_mix(T_BG, T_ACCENT, 20), 50);
}
/* (FASE 4) the bus's two levels as a pair of meters at the block's right edge: what goes in (SEND) above the
 * line, what comes back (RET) below it. Both are fx_vu's peaks, so a bus with nothing sent draws two dark wells
 * and the page says exactly which side of the effect the signal is on. */
static void fxb_meters(int32_t x, int32_t y, int32_t h, int32_t snd, int32_t wet)
{
    int32_t hh = (h - 1) / 2, n;
    cv_rect(x, y, 3, hh, T_RAISE);
    cv_rect(x, y + hh + 1, 3, hh, T_RAISE);
    if ((n = fxb_px(snd) * hh / 56) > 0)
        cv_rect(x, y + hh - n, 3, n, ux_mix(T_THEME, T_ACCENT, 45));
    if ((n = fxb_px(wet) * hh / 56) > 0)
        cv_rect(x, y + hh + 1 + hh - n, 3, n, T_ACCENT);
    cv_rect(x, y + hh, 3, 1, T_LINE);               /* (a tick between the two, so they never read as one bar) */
}

static void graph_fx_buses(const track_t *t)
{
    static const int32_t BX[2] = {3, 121}, BY[3] = {18, 54, 90};
    static const int32_t BW = 116, BH = 34, IN = 15;   /* IN: the header, so a drawing starts at y + IN */
    uint32_t i;
    int32_t sel = fxb_sel();
    int32_t snd[FXB_NB], wet[FXB_NB], live;
    /* the chain, one row: eight cells, the selected link lit, KNOB 1 walking them, and the signal's own dot
     * running the row while anything is moving through it */
    for (i = 0; i < FXB_N; i++) {
        int32_t x = 2 + (int32_t)i * 29, on = (int32_t)i == sel;
        uint16_t bg = on ? T_ACCENT : T_SURF, ink = on ? T_INK : (i < 2 ? T_MID : T_DIM);
        cv_rrect(x, 1, 27, 14, 3, bg, T_SURF);
        cv_icon_on(x + 7, 2, 12, i < 2 ? (i ? ICON_MOD : ICON_MIX) : FXB_BLOCKI[i - 2u], ink, bg);   /* (the blocks' own) */
        if (i)
            cv_rect(x - 2, 7, 1, 3, T_LINE);          /* the link between two cells of the chain */
    }
    /* the six blocks, each a panel with its own icon, its meters and its drawing of what it does */
    {
        snd[FXB_DLY] = fx_vu[1]; wet[FXB_DLY] = fx_vu[4];
        snd[FXB_REV] = fx_vu[2]; wet[FXB_REV] = fx_vu[5];
        snd[FXB_CHO] = fx_vu[0]; wet[FXB_CHO] = fx_vu[3];
        snd[FXB_SLI] = 0;        wet[FXB_SLI] = fx_vu[0];
        snd[FXB_PCH] = fx_vu[6]; wet[FXB_PCH] = fx_vu[7];
        snd[FXB_MST] = fx_vu[6]; wet[FXB_MST] = fx_vu[7];
        live = 0;
        for (i = 0; i < 8u; i++)
            live |= fx_vu[i];
        for (i = 0; i < 6u; i++) {
            int32_t x = BX[i & 1u], y = BY[i / 2u], on, dx = x + 3, dy = y + IN, dw = BW - 12, dh = BH - IN - 3;
            uint16_t head;
            cv_rrect(x, y, BW, BH, 3, T_SURF, T_BG);
            cv_rrect(x + 2, y + 2, BW - 4, BH - 4, 2, T_BG, T_SURF);   /* the block's ground: title and drawing */
            switch (i) {
            case FXB_DLY: on = (t->p[P_DLY] != 0 || wet[FXB_DLY] > 64) && sys_load != LOAD_ECO; break;
            case FXB_REV: on = t->p[P_REV] != 0 || wet[FXB_REV] > 64; break;
            case FXB_CHO: on = (t->p[P_CHOR] != 0 || wet[FXB_CHO] > 64) && sys_load != LOAD_ECO; break;
            case FXB_SLI: on = song.g[G_SLMODE] != 0 && sys_load != LOAD_ECO; break;
            case FXB_PCH: on = song.g[G_PUNCH] > 0 || song.g[G_DUCK] > 0; break;
            default: on = 1; break;
            }
            head = on ? T_THEME : T_DIM;
            fxb_head(x, y, BW, FXB_BLOCKI[i], FXB_BLOCKN[i], head, on ? wet[i] : 0);
            fxb_meters(dx + dw + 2, dy, dh, on ? snd[i] : 0, on ? wet[i] : 0);
            fxb_grad(dx, dy, dw, dh);                                 /* (Solar Heat) its floor warms up */
            switch (i) {
            case FXB_DLY: {                           /* the repeats, TIME apart, each FEEDBK softer */
                int32_t fb = clamp(song.g[G_DFDBK], 0, 127), k;
                int32_t amp = (clamp(wet[FXB_DLY] + snd[FXB_DLY] / 2, 0, 7000) * dh) / 7000;
                int32_t sp = 9 + 30 * (32 - clamp(song.g[G_DTIME], 0, 32)) / 32;
                for (k = 0; k < 3; k++) {
                    int32_t h = amp - amp * fb * k / 260;
                    if (h <= 0)
                        break;
                    cv_rect(dx + 4 + k * sp, dy + dh - h, 5, h,
                            on ? ux_mix(T_ACCENT, T_THEME, 40 + k * 18) : T_RAISE);
                    if (k < 2)
                        cv_rect(dx + 10 + k * sp, dy + dh - 1, sp - 7, 1, T_LINE);   /* the tap's own time line */
                }
                break;
            }
            case FXB_REV: {                           /* the tail: SIZE sets how far it rings, the wet peak how high */
                int32_t size = clamp(song.g[G_RSIZE], 0, 127), k;
                int32_t amp = (clamp(wet[FXB_REV] + snd[FXB_REV] / 2, 0, 7000) * dh) / 7000;
                int32_t len = 10 + size * (dw - 8) / 127;
                for (k = 0; k < len; k++) {
                    int32_t h = amp * (len - k) / len;
                    if (h <= 0)
                        continue;
                    cv_rect(dx + 2 + k, dy + dh - h, 1, h, ux_mix(T_THEME, T_ACCENT, h * 160 / (dh ? dh : 1)));
                }
                cv_rect(dx + 1, dy + dh, dw - 2, 1, T_LINE);
                break;
            }
            case FXB_CHO: {                           /* three taps as waves, their phase running with the sound */
                static const int16_t SIN16[16] = {0, 98, 181, 236, 256, 236, 181, 98, 0, -98, -181, -236, -256, -236, -181, -98};
                int32_t k, depth = clamp(song.g[G_CDEPTH], 0, 127), ph = (int32_t)(fm1_ms / 12u);
                int32_t amp = 1 + depth * 3 / 127;
                for (k = 0; k < 3; k++) {
                    int32_t j, yy0 = dy + dh / 2;
                    for (j = 0; j < dw - 4; j++) {
                        int32_t yy = dy + dh / 2 + SIN16[(j / 2 + ph + k * 5) & 15] * amp / 256;
                        if (j)
                            cv_line(dx + 2 + j - 1, yy0, dx + 2 + j, yy,
                                    on ? ux_mix(T_THEME, T_ACCENT, k * 30) : T_RAISE);
                        yy0 = yy;
                    }
                }
                break;
            }
            case FXB_SLI: {                           /* the pattern's eight gates, the step's own lit */
                int32_t pat = clamp(song.g[G_SLPAT], 0, 127), dep = clamp(song.g[G_SLDEP], 0, 127);
                int32_t step = song.playing ? (int32_t)(seq_src(t, t->seq_idx) & 7u) : -1, k;
                for (k = 0; k < 8; k++) {
                    int32_t gate = (pat >> (k & 7)) & 1, h = gate ? 3 + dep * (dh - 4) / 127 : 2;
                    uint16_t col = !on ? T_RAISE : gate ? (k == step ? T_ACCENT : T_THEME) : T_LINE;
                    cv_rect(dx + 2 + k * ((dw - 2) / 8), dy + dh - h, (dw - 2) / 8 - 2, h, col);
                }
                if (step >= 0 && on)                 /* (Solar Heat) the sequencer's line under the gates */
                    cv_rect(dx + 2, dy + dh + 1, 2 + step * ((dw - 2) / 8), 1, ux_mix(T_ACCENT, T_BG, 40));
                break;
            }
            case FXB_PCH: {                           /* the drum bus's transient, and the dip it ducks */
                int32_t pk = fx_vu[6] > fx_vu[7] ? fx_vu[6] : fx_vu[7];
                int32_t punch = clamp(song.g[G_PUNCH], 0, 127), duck = clamp(song.g[G_DUCK], 0, 127), k;
                int32_t amp = clamp(punch * dh / 127 + fxb_px(pk) / 4, 0, dh);
                for (k = 0; k < dw - 10; k++) {
                    int32_t h = amp * ((dw - 10) - k) / (dw - 10);
                    if (h <= 0)
                        break;
                    cv_rect(dx + 2 + k, dy + dh - h, 1, h, on ? ux_mix(T_ACCENT, T_THEME, 120 + h * 6) : T_RAISE);
                }
                {
                    int32_t dh2 = duck * (dh - 4) / 127;
                    cv_rect(dx + dw - 6, dy + 1, 3, dh - 1, T_LINE);
                    cv_rect(dx + dw - 6, dy + 1 + dh2, 3, dh - 1 - dh2, on ? T_THEME : T_RAISE);
                }
                break;
            }
            default: {                                /* the master: the gain stage, the output peak, the clip lamp */
                int32_t g = clamp(fx_master_g(), 0, 32767) * (dw - 2) / 32767;
                int32_t pk = audio_out_peak(0) > audio_out_peak(1) ? audio_out_peak(0) : audio_out_peak(1);
                cv_rect(dx + 1, dy + dh - 2 - g, g, 2, ux_mix(T_THEME, T_SURF, 40));   /* (on the gradient) */
                cv_rect(dx + 1, dy + dh - 2 - fxb_px(pk) * (dh - 2) / 56, dw - 2, 1, T_TEXT);   /* the output peak */
                if (pk >= 32700)
                    cv_rect(dx + dw - 9, dy + 2, 4, 4, T_ACCENT);
                else
                    cv_rect(dx + dw - 9, dy + 2, 4, 4, T_LINE);
                break;
            }
            }
            if (sel >= 2 && sel - 2 == (int32_t)i)   /* (the links after TRACK and BUSES are the six blocks) */
                cv_frame(x, y, BW, BH, T_ACCENT);     /* the block these knobs belong to */
            else if (i == FXB_MST && sel >= 1 && sel <= 7)
                cv_frame(x, y, BW, BH, T_LINE);       /* the master is every bus's way out: always in the chain */
        }
        /* (FASE 4) the signal's own dot running the chain: it is the only thing here that says the audio is
         * moving, and it only exists while one of the eight buses carries a peak */
        if (live > 64) {
            int32_t k = (int32_t)((fm1_ms / 24u) % (uint32_t)FXB_N);
            int32_t x = 2 + k * 29 + (int32_t)((fm1_ms / 6u) % 24u) % 25;
            cv_rect(x, 15, 3, 2, ux_mix(T_ACCENT, T_TEXT, 60));
        }
    }
}
