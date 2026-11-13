/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.1, 6.4) FX > BUSES: the routing console. One page that shows the chain the audio actually takes -- the track's
 * sends, the three global buses (delay, reverb, chorus), the slicer on its bus, the drum bus's punch and the
 * master -- and lets the four knobs edit whichever link is selected: KNOB 1 walks TRACK / BUSES / DLY / REV /
 * CHO / SLI / PUNCH / MASTER, and KNOB 2..4 are that link's own parameters, the same ids the other FX pages
 * edit (params.c, page_desc). Nothing here is a new DSP stage: every figure is read from the parameter the
 * engine plays and every meter from the peak fx.c measured for that bus (fx_vu), so a block that is off draws
 * no activity and a busy one draws its own signal.
 *
 * (6.4) THE LAYOUT WAS REBUILT. The first cut stacked two rows of tabs and six 72 x 28 blocks whose titles sat on
 * top of their own drawings ("SLICER" over the gates, "MASTER" over the gain bar), and eight tabs in two rows read
 * as a menu of two halves instead of one chain. The chain is now a single row of eight cells, and every block is a
 * panel with its own header strip -- title on the left, the one number that matters for that effect on the right,
 * the activity lamp in the corner -- and a drawing area below it that the title can no longer reach. Each drawing
 * keeps its own kind:
 *   DELAY    the repeats, TIME apart, each one FEEDBK softer, the wet peak as their height;
 *   REVERB   the tail's decay: SIZE sets how long it rings, the wet peak how tall it starts;
 *   CHORUS   three taps as waves, their phase running while the bus carries signal, DEPTH their sweep;
 *   SLICER   the eight gates of PAT, the step the sequencer is on lit;
 *   PUNCH    the drum bus's transient against the master peak, and the dip DUCK takes out of it;
 *   MASTER   the gain stage and the output peak, the clip lamp latched while the DAC sits at full scale.
 * Animation only runs on real signal: graph_signature folds fx_vu in, so a silent engine leaves the page still. */
#define FXB_N 8u
/* (6.4) three letters a cell: eight full names in one row of 28 px cells ran into each other */
static const char *const FXB_TABN[FXB_N] = {"TRK", "BUS", "DLY", "REV", "CHO", "SLI", "PCH", "MST"};
static const char *const FXB_TABSEL[FXB_N] = {"TRACK", "BUSES", "DELAY", "REVERB", "CHORUS", "SLICER", "PUNCH", "MASTER"};
/* the six blocks, left to right and top to bottom: which bus each reads */
enum { FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST, FXB_NB };
static const char *const FXB_BLOCKN[FXB_NB] = {"DELAY", "REVERB", "CHORUS", "SLICER", "PUNCH", "MASTER"};
/* the block's own header: its title, the number that effect is set to, and the unit that number is in */
/* (6.4) the header is a strip with the title and the lamp and nothing else: the number that used to sit at its
 * right edge ("DELAY 192", "MASTER 100") collided with the title in every block narrower than 90 px, and the four
 * cards above already read out the values of the link KNOB 1 has selected. */
static void fxb_head(int32_t x, int32_t y, int32_t w, const char *name, uint16_t ink, uint16_t bg)
{
    (void)bg;
    cv_text_on(x + 4, y + 2, &AF_S, name, ink, T_BG);   /* (6.4) inside the block's own ground, never above it */
    cv_rect(x + w - 7, y + 3, 3, 3, ink);           /* the activity lamp */
}
/* a level 0..7000 (fx_vu's scale) as a 0..56 px bar height */
static int32_t fxb_px(int32_t v) { return clamp(v, 0, 7000) * 56 / 7000; }

static void graph_fx_buses(const track_t *t)
{
    static const int32_t BX[3] = {4, 84, 164}, BY[2] = {18, 61};
    static const int32_t BW = 76, BH = 40, IN = 12;  /* IN: the header, so a drawing starts at y + IN */
    uint32_t i;
    int32_t sel = fxb_sel();
    /* the chain, one row: eight cells, the selected link lit, KNOB 1 walking them */
    for (i = 0; i < FXB_N; i++) {
        int32_t x = 4 + (int32_t)i * 29, on = (int32_t)i == sel;
        uint16_t bg = on ? T_ACCENT : T_SURF;
        cv_rrect(x, 1, 28, 13, 3, bg, T_SURF);
        cv_text_in(x, 2 + CAP_IN(S, 12), 28, &AF_S, FXB_TABN[i], on ? T_INK : (i < 2 ? T_MID : T_DIM), bg);
    }
    /* the six blocks, each a panel with its header and its own drawing of what it does */
    {
        int32_t send_dly = fx_vu[1], wet_dly = fx_vu[4], send_rev = fx_vu[2], wet_rev = fx_vu[5];
        int32_t wet_cho = fx_vu[3], send_cho = fx_vu[0];
        static const int32_t TAB2BLK[FXB_N] = {-1, -1, FXB_DLY, FXB_REV, FXB_CHO, FXB_SLI, FXB_PCH, FXB_MST};
        for (i = 0; i < 6u; i++) {
            int32_t x = BX[i % 3u], y = BY[i / 3u], on, dx = x + 4, dy = y + IN, dw = BW - 8, dh = BH - IN - 4;
            uint16_t head;
            cv_rrect(x, y, BW, BH, 3, T_SURF, T_BG);
            cv_rrect(x + 2, y + 2, BW - 4, BH - 4, 2, T_BG, T_SURF);   /* the block's ground: title and drawing */
            switch (i) {
            case FXB_DLY: on = (t->p[P_DLY] != 0 || wet_dly > 64) && sys_load != LOAD_ECO; break;
            case FXB_REV: on = t->p[P_REV] != 0 || wet_rev > 64; break;
            case FXB_CHO: on = (t->p[P_CHOR] != 0 || wet_cho > 64) && sys_load != LOAD_ECO; break;
            case FXB_SLI: on = song.g[G_SLMODE] != 0 && sys_load != LOAD_ECO; break;
            case FXB_PCH: on = song.g[G_PUNCH] > 0 || song.g[G_DUCK] > 0; break;
            default: on = 1; break;
            }
            head = on ? T_THEME : T_DIM;
            fxb_head(x, y, BW, FXB_BLOCKN[i], head, T_BG);
            switch (i) {
            case FXB_DLY: {                           /* the repeats, TIME apart, each FEEDBK softer */
                int32_t fb = clamp(song.g[G_DFDBK], 0, 127), k;
                int32_t amp = (clamp(wet_dly + send_dly / 2, 0, 7000) * dh) / 7000;
                int32_t sp = 6 + 16 * (32 - clamp(song.g[G_DTIME], 0, 32)) / 32;
                for (k = 0; k < 3; k++) {
                    int32_t h = amp - amp * fb * k / 260;
                    if (h <= 0)
                        break;
                    cv_rect(dx + 3 + k * sp, dy + dh - h, 4, h,
                            on ? ux_mix(T_ACCENT, T_THEME, 40 + k * 18) : T_RAISE);
                    if (k < 2)
                        cv_rect(dx + 8 + k * sp, dy + dh - 1, sp - 6, 1, T_LINE);   /* the tap's own time line */
                }
                break;
            }
            case FXB_REV: {                           /* the tail: SIZE sets how far it rings, the wet peak how high */
                int32_t size = clamp(song.g[G_RSIZE], 0, 127), k;
                int32_t amp = (clamp(wet_rev + send_rev / 2, 0, 7000) * dh) / 7000;
                int32_t len = 8 + size * (dw - 8) / 127;
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
                    cv_rect(dx + 2 + k * 8, dy + dh - h, 6, h, col);
                }
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
                int32_t g = clamp(fx_master_g(), 0, 32767) * (dw - 2) / 32767, k;
                int32_t pk = audio_out_peak(0) > audio_out_peak(1) ? audio_out_peak(0) : audio_out_peak(1);
                cv_rect(dx + 1, dy + 1, dw - 2, dh - 1, T_BG);
                cv_rect(dx + 1, dy + dh - 2 - g, g, 2, ux_mix(T_THEME, T_SURF, 40));
                cv_rect(dx + 1, dy + dh - 2 - fxb_px(pk) * (dh - 2) / 56, dw - 2, 1, T_TEXT);
                for (k = 1; k < 4; k++)
                    cv_rect(dx + 1 + k * (dw - 2) / 4, dy + 1, 1, dh - 2, T_RAISE);
                if (pk >= 32700)
                    cv_rect(dx + dw - 9, dy + 2, 4, 4, T_ACCENT);
                else
                    cv_rect(dx + dw - 9, dy + 2, 4, 4, T_LINE);
                break;
            }
            }
            if (TAB2BLK[sel & 7u] == (int32_t)i)
                cv_frame(x, y, BW, BH, T_ACCENT);     /* the block these knobs belong to */
            else if (i == FXB_MST && sel >= 1 && sel <= 7)
                cv_frame(x, y, BW, BH, T_LINE);       /* the master is every bus's way out: always in the chain */
        }
    }
    cv_text_on(6, 104, &AF_S, "1 CHAIN  2-4 PARAMS", T_DIM, T_SURF);
    cv_text_r(232, 104, &AF_S, FXB_TABSEL[sel & 7u], T_THEME, T_SURF);
}
