/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (6.2) EDIT 1's graph: one picture per engine, drawn from that engine's own parameters, so the page that edits the
 * engine also says what the engine is doing. It is not one plot recoloured nine times: each engine gets the shape its
 * sound has -- two oscillators and the filter for ANALOG and its relatives, four operators for FM6, the kit's lanes
 * for DRUM, the noise floor for NOISE, the formants for VOICE, the drawbars for WHEEL, the table's morph for WTABLE,
 * the grains for GRAIN, the slices for SLICE, the string for PHYS and the crushed steps for LOFI. */
static void graph_engine(const track_t *t, uint16_t c)
{
    const uint32_t e = t->eng_req % NENGINES;
    int32_t x0 = 12, x1 = 228, y0 = 12, y1 = 84, i, k;
    char b[16];
    (void)c;
    cv_text_on(12, 2, &AF_S, ENGINES[e]->name, T_THEME, T_SURF);
    switch (e) {
    case ENGI_ANALOG: {                              /* the two oscillators, and the filter's cutoff over them */
        int32_t cut = clamp(t->p[P_FCUT], 0, 127), res = clamp(t->p[P_FRES], 0, 127);
        for (k = 0; k < 2; k++) {
            int32_t yc = y0 + 18 + k * 22, amp = 12 - k * 3;
            for (i = 0; i < x1 - x0; i++) {
                int32_t ph = (i * (2 + k * 3)) & 63;
                int32_t yv = ph < 32 ? amp - ph * amp / 16 : -(amp - (ph - 32) * amp / 16);
                cv_pset(x0 + i, yc + yv, k ? T_MID : T_ACCENT);
            }
        }
        cv_rect(x0, y1 - 6, x1 - x0, 1, T_RAISE);
        cv_rect(x0 + cut * (x1 - x0 - 4) / 127, y1 - 16, 2, 11 + 0 * res, T_THEME);
        fmt_int(b, cut);
        cv_text_r(x1, y1 - 4, &AF_S, b, T_MID, T_SURF);
        break;
    }
    case ENGI_DIGITAL:                               /* the four operators' levels */
    case ENGI_TRIO: {
        static const uint8_t FML[4] = {P_FM1_LEVEL, P_FM2_LEVEL, P_FM3_LEVEL, P_FM4_LEVEL};
        for (k = 0; k < 4; k++) {
            int32_t lv = clamp(t->p[FML[k]], 0, 127);
            int32_t x = x0 + k * ((x1 - x0) / 4) + 4;
            cv_rrect(x, y1 - 6 - lv * 46 / 127, (x1 - x0) / 4 - 12, lv * 46 / 127 + 6, 3, k ? T_THEME : T_ACCENT, T_SURF);
            fmt_int(b, (int32_t)k + 1);
            cv_text_in(x, y1 - 2, (x1 - x0) / 4 - 12, &AF_S, b, T_MID, T_SURF);
        }
        break;
    }
    case ENGI_DRUM: {                                /* the kit: one bar a lane, its level */
        for (k = 0; k < 8; k++) {
            int32_t lv = clamp(t->p[P_LN0 + k], 0, 127);
            int32_t x = x0 + k * ((x1 - x0) / 8) + 3;
            cv_rect(x, y1 - 4 - lv * 48 / 127, (x1 - x0) / 8 - 6, lv * 48 / 127 + 4,
                    lv > 100 ? T_ACCENT : T_THEME);
        }
        break;
    }
    case ENGI_GRAIN: {                               /* the grains: density against their drift */
        int32_t dens = clamp(t->p[P_E2], 0, 127);
        uint32_t seed = 0x1234u;
        for (i = 0; i < x1 - x0; i++) {
            seed = seed * 1664525u + 1013904223u;
            if ((int32_t)((seed >> 16) & 127) < 24 + dens)   /* DENSITY: how often a grain lands */
                cv_rect(x0 + i, y0 + (int32_t)((seed >> 8) & 63), 1, 2, i & 1 ? T_THEME : T_ACCENT);
        }
        break;
    }
    case ENGI_SLICE: {                               /* the slices: eight of them, the player's own length */
        static const int32_t FX3[3] = {26, 62, 108};
        for (k = 0; k < 3; k++) {
            int32_t lv = clamp(t->p[P_E0 + k], 0, 127);
            int32_t x = x0 + FX3[k];
            cv_rrect(x, y1 - 6 - lv * 46 / 127, 22, lv * 46 / 127 + 6, 4, k ? T_THEME : T_ACCENT, T_SURF);
        }
        break;
    }
    case ENGI_SAMPLE: {                              /* the sample: its wave, compressed by the range */
        for (k = 0; k < 8; k++) {
            int32_t lv = clamp(t->p[P_E0 + (k & 7)], 0, 127);
            int32_t x = x0 + k * ((x1 - x0) / 8) + 4;
            cv_rect(x, y1 - 4 - lv * 48 / 127, 8, lv * 48 / 127 + 4, k == 0 ? T_ACCENT : T_THEME);
        }
        break;
    }
    case ENGI_WAVETABLE: {                           /* the table: the morph between two shapes */
        int32_t morph = clamp(t->p[P_MORPH], 0, 127);
        for (i = 0; i < x1 - x0; i++) {
            int32_t ph = (i * 5) & 63, yv = ph < 32 ? 16 - ph : ph - 48;
            int32_t yv2 = (((i * 7) & 31) - 16);
            int32_t yy = (yv * (127 - morph) + yv2 * morph) / 127;
            cv_pset(x0 + i, (y0 + y1) / 2 + yy, T_THEME);
        }
        break;
    }
    case ENGI_PHYS: {                                /* the string: its two modes, offset by the engine detune */
        int32_t det = clamp(t->p[P_E1], -63, 63);
        for (k = 0; k < 2; k++)
            for (i = 0; i < x1 - x0; i++) {
                int32_t ph = (i * 4 + k * (32 + det)) & 63;
                cv_pset(x0 + i, (y0 + y1) / 2 + (ph < 32 ? 18 - ph : -(18 - (ph - 32))), k ? T_MID : T_ACCENT);
            }
        break;
    }
    default: {                                       /* LOFI: the same wave, quantized as the engine quantizes it */
        int32_t bits = clamp(t->p[P_E0], 0, 127);
        int32_t step = 1 + bits / 16;
        for (i = 0; i < x1 - x0; i++) {
            int32_t ph = (i * 4) & 63;
            int32_t yv = ph < 32 ? 18 - ph : ph - 46;
            yv = (yv / step) * step;
            cv_rect(x0 + i, (y0 + y1) / 2 + yv, 1, 2, T_THEME);
        }
        break;
    }
    }
    cv_rect(x0, y1, x1 - x0, 1, T_LINE);
    cv_text_on(12, 94, &AF_S, "E0..E7 ON THE CARDS ABOVE", T_DIM, T_SURF);
}
