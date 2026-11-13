/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.1) FX > BUSES: the routing console. One page that shows the chain the audio actually takes -- the track's
 * sends, the three global buses (delay, reverb, chorus), the slicer on its bus, the drum bus's punch and the
 * master -- and lets the four knobs edit whichever link is selected: KNOB 1 walks TRACK / BUSES / DLY / REV /
 * CHO / SLI / PUNCH / MASTER, and KNOB 2..4 are that link's own parameters, the same ids the other FX pages
 * edit (params.c, page_desc). Nothing here is a new DSP stage: every figure is read from the parameter the
 * engine plays and every meter from the peak fx.c measured for that bus (fx_vu), so a block that is off draws
 * no activity and a busy one draws its own signal.
 *
 * The six blocks carry a drawing of their own kind, not one graph recoloured:
 *   DELAY    the repeats at the time the line is set to, each one FEEDBK softer, the send's peak behind them;
 *   REVERB   the tail's decay: SIZE sets how long it rings, the wet peak how bright it is drawn;
 *   CHORUS   the three taps as waves, their phase running while the bus carries signal, DEPTH their sweep;
 *   SLICER   the eight gates of PAT, the ones the step is on lit;
 *   PUNCH    the drum bus's transient: PUNCH's amount against the master's own peak;
 *   MASTER   the gain stage and the output peak, the clip lamp latched while the DAC sits at full scale.
 * Animation only runs on real signal: graph_signature folds fx_vu in, so a silent engine leaves the page still. */
#define FXB_N 8u
static const char *const FXB_TABN[FXB_N] = {"TRACK", "BUSES", "DLY", "REV", "CHO", "SLI", "PUNCH", "MASTER"};
static const char *const FXB_ABBR[FXB_N] = {"TRK", "BUS", "DLY", "REV", "CHO", "SLI", "PCH", "MST"};
/* the six blocks, left to right and top to bottom: which bus each reads */
enum { FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST, FXB_NB };
static const char *const FXB_BLOCKN[FXB_NB] = {"DELAY", "REVERB", "CHORUS", "SLICER", "PUNCH", "MASTER"};
/* a level 0..7000 (fx_vu's scale) as a 0..56 px bar height */
static int32_t fxb_px(int32_t v) { return clamp(v, 0, 7000) * 56 / 7000; }

static void graph_fx_buses(const track_t *t)
{
    static const int32_t BX[3] = {6, 84, 162}, BY[2] = {42, 72};
    static const int32_t BW = 72, BH = 28;
    uint32_t i;
    int32_t sel = fxb_sel();
    /* the tabs: TRACK / BUSES / DLY / REV on top, the rest of the chain under them. KNOB 1 lights the link the
     * four cards above belong to, so the page never leaves a doubt about what a turn will move. */
    for (i = 0; i < 4u; i++) {
        int32_t x = 8 + (int32_t)i * 56, on = (int32_t)i == sel;
        uint16_t bg = on ? T_ACCENT : T_SURF;
        cv_rrect(x, 1, 52, 12, 3, bg, T_SURF);
        cv_text_in(x, 2 + CAP_IN(S, 11), 52, &AF_S, FXB_TABN[i], on ? T_INK : T_MID, bg);
    }
    for (i = 4u; i < FXB_N; i++) {
        int32_t x = 8 + (int32_t)(i - 4u) * 56, on = (int32_t)i == sel;
        uint16_t bg = on ? T_ACCENT : T_RAISE;
        cv_rrect(x, 13, 52, 13, 3, bg, T_SURF);
        cv_text_in(x, 14 + CAP_IN(S, 13), 52, &AF_S, FXB_TABN[i], on ? T_INK : T_DIM, bg);
    }
    /* the six blocks, each with its own drawing of what it does (a block that is off draws nothing but its frame) */
    {
        int32_t send_dly = fx_vu[1], wet_dly = fx_vu[4], send_rev = fx_vu[2], wet_rev = fx_vu[5];
        int32_t wet_cho = fx_vu[3], send_cho = fx_vu[0];
        static const int32_t TAB2BLK[FXB_N] = {-1, -1, FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST};
        for (i = 0; i < 6u; i++) {
            int32_t x = BX[i % 3u], y = BY[i / 3u], on;
            uint16_t head;
            cv_rrect(x, y, BW, BH, 3, T_SURF, T_BG);
            cv_text_on(x + 4, y + 1, &AF_S, FXB_BLOCKN[i], T_TEXT, T_SURF);
            switch (i) {
            case FXB_DLY:
                on = (t->p[P_DLY] != 0 || wet_dly > 64) && sys_load != LOAD_ECO;
                break;
            case FXB_REV:
                on = t->p[P_REV] != 0 || wet_rev > 64;
                break;
            case FXB_CHO:
                on = (t->p[P_CHOR] != 0 || wet_cho > 64) && sys_load != LOAD_ECO;
                break;
            case FXB_SLI:
                on = song.g[G_SLMODE] != 0 && sys_load != LOAD_ECO;
                break;
            case FXB_PCH:
                on = song.g[G_PUNCH] > 0 || song.g[G_DUCK] > 0;
                break;
            default:
                on = 1;
                break;
            }
            head = on ? T_THEME : T_DIM;
            cv_rect(x + BW - 8, y + 4, 4, 4, head);      /* lit while the block carries signal */
            switch (i) {
            case FXB_DLY: {                           /* the repeats, TIME apart, each FEEDBK softer */
                int32_t fb = song.g[G_DFDBK], k;
                int32_t amp = clamp(wet_dly + send_dly / 2, 0, 7000) * 12 / 7000;
                int32_t sp = 5 + 20 * (32 - clamp(song.g[G_DTIME], 0, 32)) / 32;
                for (k = 0; k < 3; k++) {
                    int32_t h = amp - amp * fb * k / 300;
                    if (h <= 0)
                        break;
                    cv_rect(x + 5 + k * sp, y + BH - 3 - h, 3, h, on ? ux_mix(T_ACCENT, T_THEME, 40 + k * 18) : T_RAISE);
                    cv_rect(x + 5 + k * sp, y + BH - 3, 3, 1, on ? T_MID : T_RAISE);
                }
                break;
            }
            case FXB_REV: {                           /* the tail: SIZE sets how far it rings, the wet peak how high */
                int32_t size = clamp(song.g[G_RSIZE], 0, 127), k;
                int32_t amp = clamp(wet_rev + send_rev / 2, 0, 7000) * 12 / 7000;
                int32_t len = 12 + size * 52 / 127;
                for (k = 0; k < len; k++) {
                    int32_t h = amp * (len - k) / len;
                    if (h <= 0)
                        break;
                    cv_rect(x + 5 + k, y + BH - 3 - h, 1, h, ux_mix(T_ACCENT, T_THEME, h * 20 / 12));
                }
                cv_rect(x + 4, y + BH - 3, BW - 8, 1, on ? T_LINE : T_RAISE);
                break;
            }
            case FXB_CHO: {                           /* three taps as waves, their phase running with the sound */
                static const int16_t SIN16[16] = {0, 98, 181, 236, 256, 236, 181, 98, 0, -98, -181, -236, -256, -236, -181, -98};
                int32_t k, depth = clamp(song.g[G_CDEPTH], 0, 127), ph = (int32_t)(fm1_ms / 12u);
                int32_t amp = 2 + depth * 4 / 127;
                for (k = 0; k < 3; k++) {
                    int32_t j, yy0 = 0;
                    for (j = 0; j < 60; j++) {
                        int32_t yy = y + BH / 2 + SIN16[(j / 2 + ph + k * 5) & 15] * amp / 256;
                        if (j)
                            cv_line(x + 6 + j - 1, yy0, x + 6 + j, yy,
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
                    int32_t gate = (pat >> (k & 7)) & 1, h = gate ? 4 + dep * 10 / 127 : 2;
                    uint16_t col = !on ? T_RAISE : gate ? (k == step ? T_ACCENT : T_THEME) : T_LINE;
                    cv_rect(x + 5 + k * 8, y + BH - 3 - h, 6, h, col);
                }
                break;
            }
            case FXB_PCH: {                           /* the drum bus's transient, and the dip it ducks */
                int32_t pk = fx_vu[6] > fx_vu[7] ? fx_vu[6] : fx_vu[7];
                int32_t punch = clamp(song.g[G_PUNCH], 0, 127), duck = clamp(song.g[G_DUCK], 0, 127), k;
                int32_t amp = clamp(punch * 8 + fxb_px(pk) * 2, 0, 20);
                for (k = 0; k < 30; k++) {
                    int32_t h = amp * (30 - k) / 30;
                    if (h <= 0)
                        break;
                    cv_rect(x + 5 + k, y + BH - 3 - h, 1, h, on ? ux_mix(T_ACCENT, T_THEME, 120 + h * 6) : T_RAISE);
                }
                {
                    int32_t dh = duck * 10 / 127, dx = x + 52;
                    cv_rect(dx, y + 8, 2, 15, T_LINE);
                    cv_rect(dx, y + 8 + dh, 2, 15 - dh, on ? T_THEME : T_RAISE);
                }
                break;
            }
            default: {                                /* the master: the gain stage, the output peak, the clip lamp */
                int32_t g = clamp(fx_master_g(), 0, 32767) * 38 / 32767, k;
                int32_t pk = audio_out_peak(0) > audio_out_peak(1) ? audio_out_peak(0) : audio_out_peak(1);
                cv_rect(x + 5, y + BH - 3 - 20, 38, 20, T_BG);
                cv_rect(x + 5, y + BH - 3 - 20, g, 20, ux_mix(T_THEME, T_SURF, 40));
                cv_rect(x + 5, y + BH - 3 - fxb_px(pk) * 20 / 56, 38, 1, T_TEXT);
                for (k = 1; k < 4; k++)
                    cv_rect(x + 5 + k * 9, y + BH - 3 - 20, 1, 20, T_BG);
                if (pk >= 32700)
                    cv_rect(x + 50, y + 12, 4, 4, T_ACCENT);
                else
                    cv_rect(x + 50, y + 12, 4, 4, T_LINE);
                break;
            }
            }
            if (TAB2BLK[sel & 7u] == (int32_t)i)
                cv_frame(x, y, BW, BH, T_ACCENT);     /* the block these knobs belong to */
        }
    }
    cv_text_on(10, 104, &AF_S, "KNOB 1 VIEW  2..4 ITS PARAMS", T_DIM, T_SURF);
    cv_text_r(228, 104, &AF_S, FXB_TABN[sel], T_THEME, T_SURF);
}
