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
/* the block's header: 15 rows, so a 12 px icon and a 12 px title sit inside it with a row of air over the
 * separator (13 rows cut the top of the capitals and put the glyph's ink on the block's own edge) */
#define FXB_IN 15u
/* (FASE 4) the chain is read by its icons: eight 27 px cells cannot hold a 12 px glyph and three letters
 * without the letters spilling out of their cell, and the four cards above already name the selected link */
/* the six blocks, left to right and top to bottom: which bus each reads */
enum { FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST, FXB_NB };
static const char *const FXB_BLOCKN[FXB_NB] = {"DELAY", "REVERB", "CHORUS", "SLICER", "PUNCH", "MASTER"};
/* (FASE 4) the icon of each block, and of each link of the chain: the effect's own glyph, the same the pages of
 * that effect put beside its title (icons.c) */
static const uint8_t FXB_BLOCKI[FXB_NB] = {ICON_DELAY, ICON_REVERB, ICON_CHORUS, ICON_SLICE, ICON_X_PUNCH,
                                            ICON_X_MASTER};   /* (4.1) the two drawn glyphs: the drum bus's
                                                                * transient and the master's fader */
/* the block's own header: its icon, its title, and the lamp that lights with the bus */
static void fxb_head(int32_t x, int32_t y, int32_t w, uint32_t ic, const char *name, uint16_t ink, int32_t lvl)
{
    /* (this round) THE ICON AND THE TITLE SHARE A LINE, BY INK. cv_icon_on() draws the glyph where the font left
     * it: the chain's ink starts two rows above the rest and climbed into the block's top edge, and the title sat
     * at y + 2 with its capitals touching the separator. cv_icon_in() centres each glyph's own ink box on the
     * title's 12 px line and the label starts one pixel right of the 12 px cell, so no glyph can cover a letter. */
    cv_icon_in(x + 3, y + 1, 12, FXB_IN - 2, 12, ic, ink, T_BG);
    cv_text_on(x + 18, y + 2, &AF_S, name, ink, T_BG);
    /* (Solar Heat) the activity lamp: 3 px at rest, 4 px with the bus carrying signal, and the colour walks
     * from the block's own ink to the palette's heat as the level climbs -- the page's only motion a silent
     * engine cannot produce. Inside the 15-row header, never on its edge. */
    if (lvl > 0)
        cv_rect(x + w - 8, y + 3, 4, 4, ux_mix(ink, T_ACCENT, clamp(lvl * 100 / 4000, 0, 100)));
    else
        cv_rect(x + w - 7, y + 4, 3, 3, ink);       /* the activity lamp */
}
/* A level in fx_vu's units (the Q15 peak every other FX bar already reads through fx_meter_target) as a
 * 0..56 px bar height. (2.16.1) THE CONSOLE KEPT THE PRE-v1.8.4 CEILING: it divided by 7000, the scale the four
 * bars had before they were fixed, so a send bus under ~80 % of that imaginary range drew an empty bar and the
 * page's meters only woke up when the mix was already loud. And it is a square root, which is the cheap dB a
 * 56 px bar needs: 10 % of full scale reads 17 px, a quarter 28, full scale 56, and the first chunk lights as
 * soon as the DSP measures anything. */
static int32_t fxb_px(int32_t v)
{
    uint32_t t = (uint32_t)fx_meter_target(v) << 8, r = 0, b = 1u << 15;
    while (b) {                                     /* integer sqrt of t << 8: fx_meter_target*16, 0..255 */
        uint32_t c = r + b;
        if (c * c <= t)
            r = c;
        b >>= 1;
    }
    return (int32_t)((r * 56u + 128u) >> 8);
}
/* the block's readout: its value and a per-cent sign (DELAY, REVERB, CHORUS, PUNCH all print one) */
static void fxb_pct(char *b, int32_t v)
{
    /* the parameter is MIDI 0..127; the panel says %, as every card above it does (it printed the raw value
     * with a % sign: DELAY at 90 read "90 %" while the card's own MIX read 71 %) */
    fmt_int(b, (clamp(v, 0, 127) * 100 + 63) / 127);
    str_cpy(b + str_len(b), "%", 3);
}
/* (Solar Heat) the drawing area of a block as a dithered ramp, cold overhead and warm at the floor: two bands of
 * gw_dither's 4 x 4 pattern, which is what a 16-bit panel had instead of a gradient. The pattern never moves and
 * the colours are the palette's own tokens mixed by ux_mix, so GREY / MONO / FLAT keep working and a still page
 * draws a still frame. */
static void fxb_grad(int32_t x, int32_t y, int32_t w, int32_t h)
{
    /* (4.1) the box's own floor, accent rising towards the bottom. (3.9) SIX steps over the whole body, from
     * under the header to the bottom rim, not four steps inside the drawing area: the boxes wore a flat T_SURF
     * panel around a small ramp, which is why they still read as grey next to a page that is a gradient. The
     * loop is gfx.c's ux_ground_bands, the same primitive HOME's views use. */
    ux_ground_rows(x, y, w, h, 22, 62, 10, 45, y, y + h, 100);   /* (FASE 3) row by row, solid, no checker */
}
static void fxb_col(int32_t x, int32_t y, int32_t h, int32_t lvl, int32_t lo, int32_t hi)
{
    int32_t seg = 3, q = h / (seg + 1), k, n;
    if (q <= 0)
        return;
    n = (fxb_px(lvl) * q + 55) / 56;                 /* round up: the first chunk lights with the first sample */
    for (k = 0; k < q; k++) {
        int32_t cy = y + h - (k + 1) * seg - k;      /* k = 0 is the foot */
        uint16_t c = k < n ? ux_mix(T_THEME, T_ACCENT, lo + (hi - lo) * k / (q > 1 ? q - 1 : 1)) : T_RAISE;
        cv_rect(x, cy, 3, seg, c);
    }
}

static void fxb_meters(int32_t x, int32_t y, int32_t h, int32_t snd, int32_t wet)
{
    int32_t hh = (h - 1) / 2;
    fxb_col(x, y, hh, snd, 24, 70);                  /* SEND: what goes in, cooler */
    fxb_col(x, y + hh + 1, hh, wet, 45, 100);        /* RET: what comes back, at the accent */
    cv_rect(x, y + hh, 3, 1, T_LINE);               /* (a tick between the two, so they never read as one bar) */
}

/* (v2.1) THE CONSOLE. The page was a 2 x 3 list of wide, short blocks: a graphic 16 px tall inside a 116 px card
 * is a thumbnail, and the six effects read as one page of text with little pictures between the words. This is the
 * console: three columns by two rows, each block 76 x 53, with its own header, a drawing half the block's height and
 * the value that matters underneath -- the composition the reference uses, and the room to make each effect's
 * picture its own thing rather than a variation of the cell it lives in. Nothing new is measured: every drawing is
 * still fed by the same fx_vu peaks, the same song.g values and the same seq_src step the DSP publishes. */
static __attribute__((noinline)) void graph_fx_buses(const track_t *t)
{
    static const int32_t BX[3] = {3, 82, 161}, BY[2] = {17, 69};
    static const int32_t BW = 76, BH = 52, FT = 13;            /* FT: the value's row; FXB_IN: the header */
    uint32_t i;
    int32_t sel = fxb_sel();
    int32_t snd[FXB_NB], wet[FXB_NB];
    for (i = 0; i < FXB_N; i++) {
        int32_t x = 2 + (int32_t)i * 29, on = (int32_t)i == sel;
        uint16_t bg = on ? T_ACCENT : T_SURF, ink = on ? T_INK : (i < 2 ? T_MID : T_DIM);
        cv_rrect(x, 1, 27, 14, 3, bg, T_SURF);
        cv_icon_on(x + 7, 2, 12, i < 2 ? (i ? ICON_X_BUS : ICON_X_CHAIN) : FXB_BLOCKI[i - 2u], ink, bg);
        if (i)
            cv_rect(x - 2, 7, 1, 3, T_LINE);
    }
    snd[FXB_DLY] = fx_vu[1]; wet[FXB_DLY] = fx_vu[4];
    snd[FXB_REV] = fx_vu[2]; wet[FXB_REV] = fx_vu[5];
    snd[FXB_CHO] = fx_vu[0]; wet[FXB_CHO] = fx_vu[3];
    snd[FXB_SLI] = 0;        wet[FXB_SLI] = fx_vu[0];
    snd[FXB_PCH] = fx_vu[6]; wet[FXB_PCH] = fx_vu[7];
    snd[FXB_MST] = fx_vu[6]; wet[FXB_MST] = fx_vu[7];
    for (i = 0; i < 6u; i++) {
        int32_t x = BX[i % 3u], y = BY[i / 3u], on, dx = x + 4, dy = y + FXB_IN, dw = BW - 16,
                dh = BH - FXB_IN - FT - 1;                 /* 1 px of air over the value's row */
        char b[12];
        uint16_t head;
        cv_rrect(x, y, BW, BH, 3, ux_mix(T_SURF, T_THEME, 26), T_BG);        /* the block's bezel */
        cv_rect(x + 2, y + 2, BW - 4, FXB_IN - 2, T_BG);                     /* the header's own ground */
        fxb_grad(x + 2, y + FXB_IN, BW - 4, BH - FXB_IN - 2);
        cv_rect(x + 2, y + FXB_IN - 1, BW - 4, 1, ux_mix(T_BG, T_ACCENT, 30));
        switch (i) {
        /* (this round) the three buses are GLOBAL: the page drew them off the selected track's send, so selecting
         * a track with DLY / REV / CHO at 0 froze the delay's ring and dimmed the block while the bus itself was
         * carrying sound. The return fx.c measured for the bus is the only thing that can animate it. */
        case FXB_DLY: on = wet[FXB_DLY] > 64 && sys_load != LOAD_ECO; break;
        case FXB_REV: on = wet[FXB_REV] > 64; break;
        case FXB_CHO: on = wet[FXB_CHO] > 64 && sys_load != LOAD_ECO; break;
        case FXB_SLI: on = song.g[G_SLMODE] != 0 && sys_load != LOAD_ECO; break;
        case FXB_PCH: on = song.g[G_PUNCH] > 0 || song.g[G_DUCK] > 0; break;
        default: on = 1; break;
        }
        head = on ? T_THEME : T_DIM;
        fxb_head(x, y, BW, FXB_BLOCKI[i], FXB_BLOCKN[i], head, on ? wet[i] : 0);
        b[0] = 0;
        switch (i) {
        case FXB_DLY: {
            /* THE ECHO TRAIN, WITH A PULSE IN IT. The picture was the only one on the page that never moved: the
             * repeats sat still while every other block animated, which read as a broken block. Now a ring leaves
             * the head every 64 frames while the bus carries signal -- the echo the delay is making, spaced by
             * FEEDBK's softness -- and the head itself is a filled lamp so the train has a source. MIX is what
             * the number shows, because MIX is the value this console's own knob 2 moves. */
            int32_t fb = clamp(song.g[G_DFDBK], 0, 127), mx = clamp(song.g[G_DMIX], 0, 127), k;
            int32_t cx = dx + dw / 2, cy = dy + dh / 2, ph = (int32_t)(ui.frame * 3u & 63u);
            for (k = 3; k >= 0; k--) {
                int32_t rx = (dw / 3) - k * 3, ry = dh / 2 - k * 2;
                if (rx < 2 || ry < 2)
                    continue;
                /* the ring the pulse is on, while the bus carries signal, is the accent's: one travelling
                 * outward every 64 frames, so the train is seen to repeat and not just to exist */
                cv_frame(cx - rx, cy - ry, rx * 2, ry * 2,
                         on ? ((mx && k == 3 - ph / 16) ? T_ACCENT :
                               ux_mix(T_BG, ux_mix(T_THEME, T_ACCENT, 30 + k * 22), 55 + fb * k / 6)) : T_RAISE);
            }
            /* (this round) THE PULSE ITSELF. One of the four rings was the accent's, so with four rings over a
             * 64-frame cycle the picture stepped four times and read as still between them. A 2 x 2 head walks
             * the outer ring's perimeter instead, one side every eight frames: the eye can follow the repeat. */
            if (on && mx) {
                static const int8_t RING[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
                int32_t q = (ph >> 3) & 7, rx = dw / 3 - 1, ry = dh / 2 - 1;
                if (rx > 2 && ry > 2)
                    cv_rect(cx + RING[q][0] * rx - 1, cy + RING[q][1] * ry - 1, 2, 2, T_TEXT);
            }
            fxb_pct(b, mx);
            break;
        }
        case FXB_REV: {
            /* the tail as a decay, and CONTINUOUS in SIZE. It used to be 4 + size * 5 / 127 bars, so the drawing
             * re-quantised itself five times over the knob's whole travel: turning SIZE made the tail jump from
             * four bars to five with their widths and heights all changing at once, which is what read as the
             * line leaping when it was touched. Nine bars now, always, and SIZE sets the slope between them, so
             * the tail slides a bar at a time and never multiplies. */
            int32_t size = clamp(song.g[G_RSIZE], 0, 127), k, h;
            int32_t amp = clamp(wet[FXB_REV] + snd[FXB_REV] / 2, 0, 7000);
            int32_t top = (dh - 2) * (300 + amp * 970 / 7000) / 1000;      /* the tail's head, from the return */
            int32_t fall = 300 - size * 250 / 127;                         /* per-mille per bar: SIZE 0 gone at the end, 127 still ringing */
            h = top;
            for (k = 0; k < 9; k++) {
                if (h > 0)
                    cv_rect(dx + k * dw / 9, dy + dh - h, dw / 9 - 1, h,
                            on ? ux_mix(T_ACCENT, T_THEME, clamp(k * 26, 0, 100)) : T_RAISE);
                h = h * (1000 - fall) / 1000;
            }
            fxb_pct(b, song.g[G_RSIZE]);
            break;
        }
        case FXB_CHO: {
            /* TWO LANES THAT READ. The sweep ran at frame * 4 -- a full cycle in a sixteenth of a second, which
             * is a flicker, and was reported as one. It is frame * 1 now: three quarters of a second a wave, the
             * rate of the modulation it draws. Each lane is two pixels thick over a centre line, so the picture
             * has a horizon to read against and its crests are not a dotted line, and DEPTH still sets how far
             * they swing: the same knobs, a legible picture. */
            /* (this round) THE RATE THE KNOB ASKS FOR, AND A FLOOR UNDER THE WAVES. RATE is exponential in the
             * engine (0.05 .. 40 Hz), so one pixel a frame is right at slow and a strobe at fast: the display's
             * phase walks 1 .. 4 units a frame with the knob, a smooth 4:1 over its travel. The lane under the
             * accents is filled every other column so the two sweeps read as a body with depth, not two wires. */
            int32_t k, cy = dy + dh / 2, amp = 2 + clamp(song.g[G_CDEPTH], 0, 127) * (dh / 2 - 3) / 127;
            int32_t spd = 1 + clamp(song.g[G_CRATE], 0, 127) * 3 / 127, fr = (int32_t)ui.frame * spd;
            for (k = 0; k <= dw - 4; k++) {
                int32_t w1 = (k * 900 / (dw > 1 ? dw : 1) + fr) & 255, w2 = (w1 + 128) & 255;
                int32_t t1 = w1 < 128 ? w1 : 255 - w1, t2 = w2 < 128 ? w2 : 255 - w2;      /* the triangle */
                int32_t s1 = t1 * (128 - t1) / 4, s2 = t2 * (128 - t2) / 4;                /* .. rounded to a wave:
                                                                                            * 0..1024 over a cycle */
                int32_t yl = cy - 1 - s1 * amp / 1024, yr = cy + 2 + s2 * amp / 1024;
                if (on && !(k & 1))
                    cv_rect(dx + 2 + k, cy + 2, 1, (yr > cy + 2 ? yr - cy - 2 : 0), ux_mix(T_BG, T_ACCENT, 24));
                cv_rect(dx + 2 + k, yl, 1, 2, on ? T_THEME : T_RAISE);
                cv_rect(dx + 2 + k, yr, 1, 2, on ? T_ACCENT : T_RAISE);
            }
            fxb_pct(b, song.g[G_CDEPTH]);
            break;
        }
        case FXB_SLI: {
            /* the pattern's eight gates, the one playing lit */
            int32_t pat = clamp(song.g[G_SLPAT], 0, 127), dep = clamp(song.g[G_SLDEP], 0, 127);
            int32_t step = song.playing ? (int32_t)(seq_src(t, t->seq_idx) & 7u) : -1, k;
            for (k = 0; k < 8; k++) {
                int32_t gate = (pat >> (k & 7)) & 1, h = gate ? 3 + dep * (dh - 4) / 127 : 2;
                cv_rect(dx + 1 + k * (dw - 1) / 8, dy + dh - h, (dw - 1) / 8 - 1, h,
                        !on ? T_RAISE : gate ? (k == step ? T_ACCENT : T_THEME) : T_LINE);
            }
            str_cpy(b, !on ? "OFF" : song.g[G_SLMODE] == 2 ? "STUT" : "GATE", sizeof b);
            break;
        }
        case FXB_PCH: {
            /* the transient and the dip it ducks */
            int32_t punch = clamp(song.g[G_PUNCH], 0, 127), duck = clamp(song.g[G_DUCK], 0, 127), k;
            int32_t amp = clamp(punch * dh / 127 + fxb_px(wet[FXB_PCH]) / 4, 0, dh);
            for (k = 0; k < dw - 8; k++) {
                int32_t h = amp * ((dw - 8) - k) / (dw - 8);
                if (h <= 0)
                    break;
                cv_rect(dx + k, dy + dh - h, 1, h, on ? ux_mix(T_ACCENT, T_THEME, clamp(h * 5, 0, 100)) : T_RAISE);
            }
            cv_rect(dx + dw - 4, dy + duck * (dh - 2) / 127, 2, dh - duck * (dh - 2) / 127, on ? T_THEME : T_RAISE);
            fxb_pct(b, punch);
            break;
        }
        default: {
            /* the master: its meter, the output peak and the clip lamp */
            /* (FASE 1) one read per channel: the ternary called audio_out_peak() twice for the winner and
             * twice for the loser, and each call walks the 256-sample ring of the DAC (audio.c). The page
             * redraws per frame, so the peak is now two scans instead of up to four, with the same value. */
            int32_t pk0 = audio_out_peak(0), pk1 = audio_out_peak(1);
            int32_t pk = pk0 > pk1 ? pk0 : pk1;
            int32_t g = clamp(fx_master_g(), 0, 32767) * (dw - 2) / 32767;
            cv_rect(dx, dy + dh / 2 - 2, dw - 2, 5, T_RAISE);                              /* the meter's well */
            cv_rect(dx, dy + dh / 2 - 2, g, 5, ux_mix(T_THEME, T_SURF, 40));
            cv_rect(dx, dy + dh - 3 - fxb_px(pk) * (dh - 6) / 56, dw - 2, 1, T_TEXT);        /* the output peak */
            cv_rect(dx, dy + 1, dw - 2, 1, pk >= 32700 ? T_ACCENT : T_LINE);
            str_cpy(b, pk >= 32700 ? "CLIP 1" : "CLIP 0", sizeof b);
            break;
        }
        }
        if (b[0])
            cv_text_on(dx, y + BH - FT, &AF_S, b, on ? T_TEXT : T_DIM, T_BG);
        fxb_meters(x + BW - 7, dy, dh, on ? snd[i] : 0, on ? wet[i] : 0);
        if (sel >= 2 && sel - 2 == (int32_t)i)
            cv_frame(x, y, BW, BH, T_ACCENT);
        else if (i == FXB_MST && sel >= 1 && sel <= 7)
            cv_frame(x, y, BW, BH, T_LINE);
    }
}

