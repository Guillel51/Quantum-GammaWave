/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 ShadowTune / GammaWave // Wraith Custom Firmware
 * Authentic GammaWave-vst & WRAITH hardware visualizers for the 240x240 display:
 *   - THE FORGE (Live Filter Frequency Response Curve + Thermal Spectrum + Dual Scope + Width/Phase Meters)
 *   - THE HEARTH (Solar Furnace Core + Radial Flare Corona + Spark Particles + Orbiting Satellite Moon)
 *   - WRAITH RIBBONS (6-Band Character Ribbon FLU/WHI/VOX/CHR/BEL/SCR & 5-Band Vowel Ribbon OO/OH/AH/EH/EE)
 *   - 6-Track Chamfered Ingot Deck (HOME) & 8-Pad Chamfered Drum Forge (DRUM-X) */

/* neutral-safe thermal color: GREY and MONO stay strictly on the RGB565 gray axis */
static uint16_t bg_col(int32_t v)
{
    uint16_t c = heat_col(clamp(v, 0, 255));
    return settings.palette == UI_GREY_INDEX || settings.palette == UI_BW_INDEX ? ux_gray(ux_luma(c) >> 3) : c;
}
static int32_t bg_sin(uint32_t ang) { return sine_i(ang << 16); }   /* Q15 */

/* the output's level now, 0..256, slewed (fast attack, smooth release) */
static int32_t bg_energy(void)
{
    static int32_t e;
    static uint32_t fr = 0xFFFFFFFFu;
    uint32_t w = scope_w, i;
    int32_t pk = 0, v;
    if (fr == ui.frame)
        return e;
    fr = ui.frame;
    for (i = 1; i <= 256u; i += 2u) {
        v = scope_buf[(w - i) & (SCOPE_N - 1u)];
        if (v < 0) v = -v;
        if (v > pk) pk = v;
    }
    v = pk * 256 / 12000;
    if (v > 256) v = 256;
    e += v > e ? (v - e) * 3 / 4 : (v - e) / 6;
    return e;
}

/* Chamfered Octagonal GammaWave Sub-Module (GroupedComponent::paint from PluginEditorComponents.h) */
static void gw_oct_panel(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t fill, uint16_t brd,
                         int bracket, uint16_t bcol)
{
    int32_t i, j;
    if (r < 1) r = 1;
    cv_rect(x + r, y + 1, w - 2 * r, h - 2, fill);
    cv_rect(x + 1, y + r, r - 1, h - 2 * r, fill);
    cv_rect(x + w - r, y + r, r - 1, h - 2 * r, fill);
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            int32_t s = i + j;
            if (s >= r) {
                cv_pset(x + i, y + j, s == r ? brd : fill);
                cv_pset(x + w - 1 - i, y + j, s == r ? brd : fill);
                cv_pset(x + i, y + h - 1 - j, s == r ? brd : fill);
                cv_pset(x + w - 1 - i, y + h - 1 - j, s == r ? brd : fill);
            }
        }
    cv_rect(x + r, y, w - 2 * r, 1, brd);
    cv_rect(x + r, y + h - 1, w - 2 * r, 1, brd);
    cv_rect(x, y + r, 1, h - 2 * r, brd);
    cv_rect(x + w - 1, y + r, 1, h - 2 * r, brd);
    if (bracket) {
        int32_t bl = w > 60 ? 10 : 6;
        cv_rect(x + r, y, bl, 1, bcol);
        cv_rect(x, y + r, 1, bl - 1, bcol);
        for (i = 0; i < r; i++) {
            cv_pset(x + i, y + r - i, bcol);
            cv_pset(x + i + 1, y + r - i, bcol);
        }
    }
}

/* Smooth filled circular disc (Q0 pixel coordinates) for THE HEARTH solar core & satellite moon */
static void gw_disc(int32_t cx, int32_t cy, int32_t r, uint16_t col)
{
    int32_t dy, dx, r2 = r * r;
    for (dy = -r; dy <= r; dy++) {
        int32_t y = cy + dy, span = 0;
        for (dx = r; dx >= 0; dx--)
            if (dx * dx + dy * dy <= r2) { span = dx; break; }
        cv_rect(cx - span, y, 2 * span + 1, 1, col);
    }
}

/* Smooth circular/elliptical ring (24 segments, optional dotted mode for satellite orbit track) */
static void gw_orbit_ring(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int dotted)
{
    int32_t i, px = cx + rx, py = cy;
    for (i = 1; i <= 24; i++) {
        uint32_t th = (uint32_t)i * 65536u / 24u;
        int32_t x = cx + (rx * bg_sin(th + 0x4000u) >> 15);
        int32_t y = cy + (ry * bg_sin(th) >> 15);
        if (!dotted || (i & 1))
            cv_line(px, py, x, y, col);
        px = x;
        py = y;
    }
}

/* Segmented vertical plasma meter bar */
static void gw_plasma_bar(int32_t x, int32_t y0, int32_t w, int32_t h, int32_t segs, int32_t val256, int32_t heat0)
{
    int32_t k, lit = (val256 * segs + 128) >> 8, step = h / segs, sh = step > 2 ? step - 1 : 1;
    for (k = 0; k < segs; k++) {
        int32_t sy = y0 + h - (k + 1) * step;
        uint16_t c = k < lit ? bg_col(heat0 + k * (244 - heat0) / (segs > 1 ? segs - 1 : 1)) : T_RAISE;
        cv_rect(x, sy, w, sh, c);
    }
}

/* Segmented horizontal thermal bar */
static void gw_hbar(int32_t x0, int32_t y, int32_t w, int32_t h, int32_t segs, int32_t val256, int32_t heat0)
{
    int32_t k, lit = (val256 * segs + 128) >> 8, step = w / segs, sw = step > 2 ? step - 1 : 1;
    for (k = 0; k < segs; k++) {
        int32_t sx = x0 + k * step;
        uint16_t c = k < lit ? bg_col(heat0 + k * (244 - heat0) / (segs > 1 ? segs - 1 : 1)) : T_RAISE;
        cv_rect(sx, y, sw, h, c);
    }
}

/* the page's knob c as 0..256 (none: -1) and its label */
static int32_t bg_knob(uint32_t c, const char **lab)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    *lab = d && d->label ? d->label : "";
    return r < 0 ? -1 : r * 256 / 1000;
}

/* Live triggered waveform trace across a horizontal span */
static void gw_live_trace(int32_t x0, int32_t w, int32_t cy, int32_t amp, int32_t heat_base)
{
    uint32_t sw = scope_w, i, trig = 0;
    int32_t x, py = cy, pk = 2000;
    for (i = 0; i < 256u; i++) {
        int32_t v = scope_buf[(sw - 256u + i) & (SCOPE_N - 1u)];
        if (v < 0) v = -v;
        if (v > pk) pk = v;
    }
    for (i = 2; i < 128u; i++) {
        int32_t a = scope_buf[(sw - 256u + i - 2u) & (SCOPE_N - 1u)] + scope_buf[(sw - 256u + i - 1u) & (SCOPE_N - 1u)];
        int32_t b = scope_buf[(sw - 256u + i) & (SCOPE_N - 1u)] + scope_buf[(sw - 256u + i + 1u) & (SCOPE_N - 1u)];
        if (a < 0 && b >= 0) { trig = i; break; }
    }
    for (x = 0; x < w; x++) {
        uint32_t idx = (sw - 256u + trig + (uint32_t)x * 128u / (uint32_t)(w ? w : 1)) & (SCOPE_N - 1u);
        int32_t s = (scope_buf[idx] + 2 * scope_buf[(idx + 1u) & (SCOPE_N - 1u)] + scope_buf[(idx + 2u) & (SCOPE_N - 1u)]) / 4;
        int32_t y = cy - s * amp / pk;
        int32_t dy = y > cy ? y - cy : cy - y;
        y = clamp(y, cy - amp, cy + amp);
        if (x)
            cv_line(x0 + x - 1, py, x0 + x, y, bg_col(heat_base + dy * 140 / (amp ? amp : 1)));
        py = y;
    }
}

/* ============================================================================
 * 1. THE HEARTH (Ported from HearthVisualizer in PluginEditorComponents.h)
 *    Solar Furnace Core + Radial Flare Corona + Spark Particles + Orbiting Moon
 * ============================================================================ */
static void gw_draw_hearth(int32_t cx, int32_t cy, int32_t r_core, int32_t en, int32_t heat, int32_t sparks, uint32_t tm)
{
    int32_t i, r_outer = r_core + 4 + (en * 4 >> 8), rx_orb = r_core + 20, ry_orb = r_core + 12;
    uint32_t moon_ang = tm / 3u + 0x2800u;
    int32_t mx = cx + (rx_orb * bg_sin(moon_ang + 0x4000u) >> 15);
    int32_t my = cy + (ry_orb * bg_sin(moon_ang) >> 15);

    /* 1. Elliptical Orbital Path (dotted) */
    gw_orbit_ring(cx, cy, rx_orb, ry_orb, T_LINE, 1);
    gw_orbit_ring(cx, cy, r_outer + 5, r_outer + 5, ux_mix(T_SURF, bg_col(64 + heat / 3), 45), 0);

    /* 2. Solar Flare Corona Rays (20 smooth radial flares modulated by heat & audio) */
    for (i = 0; i < 20; i++) {
        uint32_t a = (uint32_t)i * 65536u / 20u + (tm >> 2);
        int32_t wave = (bg_sin(a * 3u - tm) + 32768) >> 12;   /* 0..16 */
        int32_t len = 3 + ((heat + en) * (3 + (wave >> 1)) >> 8);
        int32_t x0 = cx + ((r_outer - 1) * bg_sin(a + 0x4000u) >> 15);
        int32_t y0 = cy + ((r_outer - 1) * bg_sin(a) >> 15);
        int32_t x1 = cx + ((r_outer + len) * bg_sin(a + 0x4000u) >> 15);
        int32_t y1 = cy + ((r_outer + len) * bg_sin(a) >> 15);
        cv_line(x0, y0, x1, y1, bg_col(110 + (wave * 7)));
    }

    /* 3. Orbiting Star / Spark Particles (HearthVisualizer::stars) */
    for (i = 0; i < 14; i++) {
        uint32_t hsh = (uint32_t)(i + 1) * 2654435761u;
        uint32_t pa = (hsh >> 16) + tm * (1u + (i & 1u)) / 4u;
        int32_t pr = r_outer + 3 + (int32_t)(((hsh >> 8) + (tm >> 9)) % (uint32_t)(10 + (sparks >> 4)));
        int32_t sx = cx + (pr * bg_sin(pa + 0x4000u) >> 15);
        int32_t sy = cy + (pr * bg_sin(pa) >> 15) * 4 / 5;
        uint16_t sc = (i & 3) == 0 ? T_TEXT : bg_col(160 + (i * 6));
        cv_pset(sx, sy, sc);
        if ((i & 3) == 0) cv_pset(sx + 1, sy, sc);
    }

    /* 4. Concentric Solar Furnace Disc (Mantle -> Golden Core -> White-Hot Singularity) */
    gw_disc(cx, cy, r_outer, bg_col(88 + heat / 3));
    gw_disc(cx, cy, r_core, bg_col(156 + en / 4));
    gw_disc(cx, cy, r_core * 65 / 100, bg_col(212 + en / 8));
    gw_disc(cx, cy, r_core * 32 / 100, bg_col(250));

    /* 5. Orbiting Satellite Moon (HearthVisualizer lines 1554-1583) */
    gw_disc(mx, my, 4, T_RAISE);
    gw_disc(mx, my, 3, T_MID);
    /* Sunlit crescent facing the Hearth core */
    cv_pset(mx + (cx > mx ? 1 : -1), my + (cy > my ? 1 : -1), bg_col(220));
    cv_pset(mx + (cx > mx ? 2 : -2), my, T_TEXT);
}

/* ============================================================================
 * 2. WRAITH 6-BAND CHARACTER RIBBON & 5-BAND VOWEL RIBBON
 *    (Ported from CharacterRibbon & VowelSelector in PluginEditorWraith.cpp)
 * ============================================================================ */
static void gw_wraith_glyph(int32_t idx, int32_t cx, int32_t cy, uint16_t col)
{
    int32_t i;
    switch (idx) {
    case 0: /* FLUTE: gentle sine wave + air dots */
        for (i = -7; i <= 7; i++)
            cv_pset(cx + i, cy + (bg_sin((uint32_t)(i + 7) * 4680u) * 3 >> 15), col);
        cv_pset(cx - 4, cy - 4, col); cv_pset(cx + 1, cy - 4, col); cv_pset(cx + 5, cy - 3, col);
        break;
    case 1: /* WHISTLE: tight fast sine wave */
        for (i = -7; i <= 7; i++)
            cv_pset(cx + i, cy + (bg_sin((uint32_t)(i + 7) * 9360u) * 3 >> 15), col);
        break;
    case 2: /* VOICE: vocal mouth ellipse */
        gw_orbit_ring(cx, cy, 6, 3, col, 0);
        cv_rect(cx - 3, cy, 7, 1, col);
        break;
    case 3: /* CHOIR: 3 stacked harmonic waves */
        for (i = -6; i <= 6; i += 2) {
            int32_t w = bg_sin((uint32_t)(i + 6) * 5460u) * 2 >> 15;
            cv_pset(cx + i, cy - 3 + w, col);
            cv_pset(cx + i, cy + w, col);
            cv_pset(cx + i, cy + 3 + w, col);
        }
        break;
    case 4: /* BELL: diamond resonator */
        cv_line(cx, cy - 4, cx + 5, cy, col);
        cv_line(cx + 5, cy, cx, cy + 4, col);
        cv_line(cx, cy + 4, cx - 5, cy, col);
        cv_line(cx - 5, cy, cx, cy - 4, col);
        break;
    default: /* SCREAM: jagged high-gain spikes */
        cv_line(cx - 7, cy + 2, cx - 4, cy - 4, col);
        cv_line(cx - 4, cy - 4, cx - 1, cy + 3, col);
        cv_line(cx - 1, cy + 3, cx + 2, cy - 4, col);
        cv_line(cx + 2, cy - 4, cx + 5, cy + 3, col);
        cv_line(cx + 5, cy + 3, cx + 7, cy - 2, col);
        break;
    }
}

static int32_t gw_trk_cut(const track_t *t)
{
    return clamp(t->p[(t->eng_req % NENGINES) == ENGI_ANALOG ? P_E4 : P_FCUT], 0, 127);
}
static int32_t gw_trk_res(const track_t *t)
{
    return clamp(t->p[(t->eng_req % NENGINES) == ENGI_ANALOG ? P_E5 : P_FRES], 0, 127);
}

static void gw_draw_wraith_ribbon(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    static const char *const CHAR_N[6] = {"FLU", "WHI", "VOX", "CHR", "BEL", "SCR"};
    static const char *const VOW_N[5]  = {"OO", "OH", "AH", "EH", "EE"};
    static const uint8_t CHAR_HEAT[6]  = {36, 68, 128, 96, 176, 232};
    uint32_t eng = t->eng_req % NENGINES;
    int is_voice = str_eq(ENGINES[eng]->name, "VOICE");
    int is_vowel = is_voice || (t->p[P_FTYPE] >= FT_VOWEL);
    int32_t nb = is_vowel ? 5 : 6, bw = w / nb, i;
    const char *lb;
    int32_t k0 = bg_knob(0, &lb), khot = ui.hot_t ? bg_knob(ui.hot_col & 3u, &lb) : -1;
    int32_t cut = gw_trk_cut(t);
    int32_t lfo_mod = (t->p[P_LD_FLT] * mulq15(t->lfo_val, t->lfo_fade)) >> 14;
    int32_t morph = cut;
    if (khot >= 0) {
        morph = khot >> 1;
    } else if (is_voice) {
        int32_t v0 = t->p[P_E0], talk = (t->p[P_E2] > 0 && bg_energy() > 8) ? ((bg_sin(ui.frame * 900u) + 32768) >> 9) : 0;
        morph = v0 + (((t->p[P_E1] - v0) * talk) >> 7);
    } else if (!is_vowel && cur_page()->id[0] == P_E0 && k0 >= 0) {
        morph = (k0 >> 1) + (t->p[P_MORPH] >> 2);
    } else if (!is_vowel && cur_page()->id[0] == P_MORPH) {
        morph = t->p[P_MORPH];
    }
    morph = clamp(morph + lfo_mod, 0, 127);
    int32_t active_b = (morph * nb) / 128;
    int32_t nx = x0 + 3 + morph * (w - 6) / 127;

    gw_oct_panel(x0, y0, w, h, 3, T_BG, T_LINE, 0, T_THEME);
    for (i = 0; i < nb; i++) {
        int32_t bx = x0 + i * bw, cw = (i == nb - 1) ? (x0 + w - bx) : bw;
        int sel = (i == active_b);
        uint16_t hc = bg_col(is_vowel ? (140 + i * 24) : CHAR_HEAT[i]);
        uint16_t bg = sel ? ux_mix(T_BG, hc, 35) : T_BG;
        if (sel)
            cv_rect(bx + 1, y0 + 1, cw - 2, h - 2, bg);
        if (i > 0)
            cv_rect(bx, y0 + 1, 1, h - 2, T_LINE);
        if (!is_vowel) {
            gw_wraith_glyph(i, bx + cw / 2, y0 + 7, sel ? hc : T_DIM);
            cv_text_in(bx, y0 + h - 13, cw, &AF_S, CHAR_N[i], sel ? T_TEXT : T_MID, bg);
        } else {
            cv_text_in(bx, y0 + CAP_IN(S, h), cw, &AF_S, VOW_N[i], sel ? T_TEXT : T_MID, bg);
        }
    }
    /* Continuous WRAITH Ribbon Playhead Needle */
    cv_rect(nx, y0 + 1, 2, h - 2, T_TEXT);
    cv_rect(nx - 2, y0, 5, 2, T_ACCENT);
    cv_rect(nx - 2, y0 + h - 2, 5, 2, T_ACCENT);
}

/* ============================================================================
 * 3. THE FORGE (Ported from SpectrumVisualizer & drawFilterCurve in GammaWave-vst)
 *    Logarithmic Filter Frequency Response Curve + 24-Band Thermal Spectrum + Scope
 * ============================================================================ */
static int32_t gw_filter_resp_y(int32_t ftype, int32_t x, int32_t w, int32_t cut_x, int32_t res, int32_t y_zero, int32_t h_max)
{
    int32_t d = x - cut_x, ad = d < 0 ? -d : d;
    int32_t res_pk = (res * h_max / 180) * (24 - (ad > 24 ? 24 : ad)) / 24;
    int32_t att = 0;
    switch (ftype) {
    case 1: /* HP: high-pass roll-off below cutoff */
        if (d < 0) att = (-d * -d) * h_max / (w * 18 + 1);
        break;
    case 2: /* BP: band-pass bell around cutoff */
        att = (d * d) * h_max / (w * 14 + 1);
        break;
    case 3: /* FT_COMB: comb notches */
        att = ((x * 5) % 18 < 5) ? h_max / 3 : 0;
        break;
    case 4: /* FT_VOWEL: 3 formant peaks (F1, F2, F3) morphing with cutoff */
    {
        int32_t f1 = w / 6 + cut_x / 4, f2 = w / 2 + cut_x / 3, f3 = w * 4 / 5;
        int32_t d1 = x - f1, d2 = x - f2, d3 = x - f3;
        int32_t p1 = (22 - (d1 < 0 ? -d1 : d1)) * h_max / 36;
        int32_t p2 = (18 - (d2 < 0 ? -d2 : d2)) * h_max / 40;
        int32_t p3 = (14 - (d3 < 0 ? -d3 : d3)) * h_max / 48;
        int32_t pk = (p1 > 0 ? p1 : 0) + (p2 > 0 ? p2 : 0) + (p3 > 0 ? p3 : 0);
        return clamp(y_zero + h_max / 3 - pk - res_pk / 2, y_zero - h_max / 2, y_zero + h_max / 2);
    }
    case 5: /* FT_SCREAM: sharp resonant peak + comb notch */
        if (d > 0) att = (d * d) * h_max / (w * 12 + 1);
        if (d > -14 && d < -4) att += h_max / 3;
        res_pk = res_pk * 3 / 2;
        break;
    default: /* LP: low-pass roll-off above cutoff */
        if (d > 0) att = (d * d) * h_max / (w * 18 + 1);
        break;
    }
    return clamp(y_zero - res_pk + att, y_zero - h_max / 2, y_zero + h_max / 2);
}

static void gw_mini_bakelite_knob(int32_t cx, int32_t cy, int32_t r, int32_t val256, uint16_t arc_col)
{
    uint32_t th = (uint32_t)((-384 + clamp(val256, 0, 256) * 768 / 256) * 64);
    int32_t px1 = cx + ((r - 1) * bg_sin(th) >> 15), py1 = cy - ((r - 1) * bg_sin(th + 0x4000u) >> 15);
    uint16_t bake = (settings.palette == UI_GREY_INDEX || settings.palette == UI_BW_INDEX)
                        ? T_RAISE
                        : ux_mix(T_BG, bg_col(128), 42);
    gw_disc(cx, cy, r, T_LINE);
    gw_disc(cx, cy, r - 1, bake);
    gw_orbit_ring(cx, cy, r + 1, r + 1, arc_col, 1);
    cv_line(cx, cy, px1, py1, T_TEXT);
    cv_pset(cx, cy, T_ACCENT);
}

static void gw_draw_forge_bay(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t en)
{
    int32_t gx0 = x0 + 4, gy0 = y0 + 4, gw = w - 28, gh = h - 8;
    int32_t y_zero = gy0 + gh * 2 / 5, cut = gw_trk_cut(t), res = gw_trk_res(t);
    int32_t ftype = clamp(t->p[P_FTYPE], 0, 5);
    int32_t lfo_mod = (t->p[P_LD_FLT] * mulq15(t->lfo_val, t->lfo_fade)) >> 14;
    int32_t cut_x = 10 + clamp(cut + lfo_mod, 0, 127) * (gw - 20) / 127;
    int32_t mod_span = (t->p[P_LD_FLT] < 0 ? -t->p[P_LD_FLT] : t->p[P_LD_FLT]) * (gw - 20) / 256;
    int32_t i, x, py = y_zero, mpy = y_zero;

    gw_oct_panel(x0, y0, w, h, 4, T_BG, T_LINE, 1, T_THEME);

    /* 1. Logarithmic Frequency Grid Lines (100Hz, 1kHz, 10kHz) & 0dB Horizon */
    for (i = 1; i <= 3; i++)
        cv_rect(gx0 + i * gw / 4, gy0, 1, gh, T_GRID);
    cv_rect(gx0, y_zero, gw, 1, T_GRID);
    cv_rect(gx0, gy0 + gh * 3 / 4, gw, 1, T_GRID);

    /* 2. 28-Band Yellow -> Orange -> Purple FFT Spectrogram Waterfall (matching latest GammaWave Filter bay) */
    for (i = 0; i < 28; i++) {
        int32_t bx = gx0 + 2 + i * (gw - 4) / 28, bw = (gw - 4) / 28 - 1;
        uint32_t idx = (scope_w - 128u + (uint32_t)i * 4u) & (SCOPE_N - 1u);
        int32_t s = scope_buf[idx] - scope_buf[(idx + 2u) & (SCOPE_N - 1u)];
        if (s < 0) s = -s;
        int32_t env_y = gw_filter_resp_y(ftype, i * gw / 28, gw, cut_x, res, y_zero, gh);
        int32_t max_h = clamp(gy0 + gh - env_y, 3, gh - 3);
        int32_t bh = clamp((s * max_h / 8500) + (en * max_h / 290), 3, max_h);
        int32_t top_y = gy0 + gh - bh;
        int32_t h_top = bh / 3 + 1, h_mid = bh / 3 + 1, h_bot = bh - h_top - h_mid;
        if (bw < 1) bw = 1;
        /* Base: Deep Purple-Crimson thermal band */
        if (h_bot > 0)
            cv_rect(bx, top_y + h_top + h_mid, bw, h_bot, ux_mix(T_BG, bg_col(68), 65));
        /* Mid: Vibrant #f27121 GammaWave Orange */
        if (h_mid > 0)
            cv_rect(bx, top_y + h_top, bw, h_mid, ux_mix(T_BG, bg_col(156), 80));
        /* Crest: Blazing Yellow-White Waterfall Peak */
        cv_rect(bx, top_y, bw, h_top, bg_col(232));
        cv_rect(bx, top_y, bw, 1, bg_col(252));
    }

    /* 3. Cyan Modulation Envelope Curve (drawFilterCurve pre-FX/mod overlay) */
    for (x = 0; x < gw; x += 2) {
        int32_t mod_cut = clamp(cut_x + (mod_span > 4 ? mod_span : 14), 8, gw - 8);
        int32_t my = clamp(gw_filter_resp_y(ftype, x, gw, mod_cut, res / 2, y_zero + 3, gh), gy0 + 2, gy0 + gh - 2);
        if (x)
            cv_line(gx0 + x - 2, mpy, gx0 + x, my, T_ACCENT);
        mpy = my;
    }

    /* 4. Live Triggered Pre-FX Waveform Trace */
    gw_live_trace(gx0 + 2, gw - 4, gy0 + gh / 2, gh / 3, 64);

    /* 5. Glowing #f27121 Orange Filter Frequency Response Curve (2px thick) */
    for (x = 0; x < gw; x++) {
        int32_t y = clamp(gw_filter_resp_y(ftype, x, gw, cut_x, res, y_zero, gh), gy0 + 2, gy0 + gh - 2);
        if (x)
            cv_line_t(gx0 + x - 1, py, gx0 + x, y, bg_col(176 + (gy0 + gh - y) * 76 / (gh ? gh : 1)), 2);
        py = y;
    }
    /* Cutoff Resonance Node Handle (Glowing Orange + White Core) */
    {
        int32_t cy_node = clamp(gw_filter_resp_y(ftype, cut_x, gw, cut_x, res, y_zero, gh), gy0 + 3, gy0 + gh - 4);
        cv_rrect(gx0 + cut_x - 3, cy_node - 3, 7, 7, 3, T_THEME, T_BG);
        cv_rect(gx0 + cut_x - 1, cy_node - 1, 3, 3, T_TEXT);
    }

    /* 6. Right-Side Stereo Width (W) & Phase Correlation (P) Meters (ScopeViewComponent) */
    {
        int32_t mx = x0 + w - 20, wid = clamp(64 + song.g[G_WIDTH] + song.g[G_RWIDE] / 2, 24, 240);
        int32_t corr = clamp(210 - (song.g[G_CSTER] > 0 ? song.g[G_CSTER] : -song.g[G_CSTER]), 80, 248);
        cv_rect(mx - 3, gy0, 1, gh, T_LINE);
        gw_plasma_bar(mx + 1, gy0 + 2, 6, gh - 4, 8, (en ? (wid * (128 + en / 2) >> 8) : wid / 2), 60);
        gw_plasma_bar(mx + 10, gy0 + 2, 6, gh - 4, 8, corr, 110);
    }
}

/* ============================================================================
 * 4. SCREEN 2 (WAVES & WRAITH): WAVE 1 + WAVE 2 + MIXER + WRAITH RIBBON
 *    (Matches planned_2_waves.png & latest GammaWave WAVE 1 / WAVE 2 / MIXER)
 * ============================================================================ */
static void graph_being(const track_t *t)
{
    static uint32_t tm, f0;
    int32_t en = bg_energy();
    uint32_t c;
    int32_t k[4];
    for (c = 0; c < 4u; c++) {
        const char *lb;
        int32_t v = bg_knob(c, &lb);
        k[c] = v < 0 ? 128 : v;
    }
    if (!(ui_prefs & PREF_ANIM_OFF))
        tm += (ui.frame - f0) * (uint32_t)(180 + en * 4);
    f0 = ui.frame;

    /* 1. Upper-Left Module (x=8..118, y=2..82): WAVE 1 (Oscillator Scope + 4 Mini Bakelite Knobs) */
    gw_oct_panel(8, 2, 110, 81, 4, T_BG, T_LINE, 1, T_THEME);
    cv_text_on(14, 4, &AF_S, "WAVE 1", T_THEME, T_BG);
    cv_text_r(113, 4, &AF_S, "UNI", T_ACCENT, T_BG);
    cv_rect(12, 18, 102, 1, T_LINE);
    cv_rect(12, 34, 102, 1, T_GRID);
    gw_live_trace(14, 98, 34, 13, 148);
    cv_rect(12, 51, 102, 1, T_LINE);
    /* 4 Mini Bakelite Rotary Knobs for Wave 1 (Pitch, Fine, PWM, Drift) */
    for (c = 0; c < 4u; c++) {
        int32_t kx = 24 + (int32_t)c * 26;
        gw_mini_bakelite_knob(kx, 66, 8, k[c], (c & 1u) ? T_ACCENT : T_THEME);
    }

    /* 2. Upper-Right Module (x=122..232, y=2..82): WAVE 2 + MIXER (Wave 1/2/Sub/Noise/FM/Ring/Grain) */
    gw_oct_panel(122, 2, 110, 81, 4, T_BG, T_LINE, 1, T_ACCENT);
    cv_text_on(128, 4, &AF_S, "WAVE 2", T_ACCENT, T_BG);
    cv_text_r(227, 4, &AF_S, "MIX", T_MID, T_BG);
    cv_rect(126, 18, 102, 1, T_LINE);
    gw_live_trace(128, 98, 29, 9, 88);
    cv_rect(126, 41, 102, 1, T_LINE);
    for (c = 0; c < 4u; c++) {
        int32_t by = 45 + (int32_t)c * 9;
        gw_hbar(128, by, 98, 6, 11, clamp(k[c], 0, 256), 72 + (int32_t)c * 36);
    }

    /* 3. Lower Bay: WRAITH 6-Band Character Ribbon / 5-Band Vowel Ribbon */
    gw_draw_wraith_ribbon(t, 8, 86, 224, 32);
}

/* ============================================================================
 * 5. SCREEN 1 (HOME): SPECTROGRAM FILTER BAY + BLAZING SUN/MOON + HEARTH & 6 TRACKS
 *    (Matches planned_1_home.png & latest GammaWave screenshot)
 * ============================================================================ */
static void graph_ecosys(void)
{
    static int32_t en[NTRK];
    static uint32_t f0, tm;
    int32_t tot_en = bg_energy();
    uint32_t c, dt = ui.frame - f0;
    if (dt > 8u) dt = 8u;
    if (ui_prefs & PREF_ANIM_OFF) dt = 0;
    f0 = ui.frame;
    tm += dt * (uint32_t)(200 + tot_en * 3);

    /* 1. Upper Bay (y = 2..63): FILTER + Yellow-Orange-Purple Spectrogram Waterfall + Cyan Mod Curve */
    gw_draw_forge_bay(TSEL, 8, 2, 224, 61, tot_en);

    /* 2. Lower-Left Bay (x = 8..96, y = 66..118): Blazing Yellow-White Sun + Orbiting Moon */
    gw_oct_panel(8, 66, 88, 52, 4, T_BG, T_LINE, 1, T_THEME);
    gw_draw_hearth(52, 92, 10 + (tot_en >> 6), tot_en, 210, 180, tm);

    /* 3. Lower-Right Bay (x = 100..232, y = 66..118): HEARTH Physics Knobs (Heat/Sparks/Weight/Force) + 6 Track Pads */
    gw_oct_panel(100, 66, 132, 52, 4, T_BG, T_LINE, 1, T_ACCENT);
    for (c = 0; c < 4u; c++) {
        const char *lb;
        int32_t kv = bg_knob(c, &lb);
        int32_t kx = 118 + (int32_t)c * 31;
        gw_mini_bakelite_knob(kx, 79, 8, kv < 0 ? (110 + (int32_t)c * 28) : kv, (c & 1u) ? T_ACCENT : T_THEME);
    }
    cv_rect(104, 92, 124, 1, T_LINE);

    /* 6 Compact Track Ingot Indicators (T1..T6) at bottom of Hearth Bay */
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        int32_t x = 104 + (int32_t)c * 21, y = 95, w = 19, h = 20;
        int32_t v = t->peak * 256 / 7000;
        int sel = (c == song.sel), muted = (t->p[P_MUTE] || c >= trk_live);
        char num[2] = {(char)('1' + c), 0};
        if (v > 256) v = 256;
        if (dt) {
            t->peak = 0;
            en[c] = v > en[c] ? v : en[c] + (v - en[c]) / 4;
        }
        cv_rrect(x, y, w, h, 2, sel ? T_RAISE : T_BG, T_BG);
        cv_text_in(x, y + 1, w, &AF_S, num, sel ? T_ACCENT : muted ? T_DIM : T_TEXT, sel ? T_RAISE : T_BG);
        gw_hbar(x + 2, y + 14, w - 4, 4, 3, muted ? 0 : (en[c] > 32 ? en[c] : (sel ? 96 : 24)), 72);
    }
}

/* ============================================================================
 * 6. DRUM-X: 8 CHAMFERED OCTAGONAL DRUM FORGE PADS + TRANSIENT WAVEFORMS
 * ============================================================================ */
static void gw_drum_wave(uint32_t lane, int32_t x0, int32_t cy, int32_t w, uint16_t col)
{
    int32_t i, py = cy;
    for (i = 0; i < w; i++) {
        int32_t env = (w - i) * 10 / w, y = cy;
        switch (lane & 7u) {
        case 0: /* BD: Pitch-drop sine sub */
            y = cy - (bg_sin((uint32_t)(i * (24 - i / 2) * 520)) * env >> 15);
            break;
        case 1: /* SD: Snappy transient + noise tail */
            y = cy - ((((int32_t)((uint32_t)(i * 2654435761u) >> 24) - 128) * env) >> 7);
            break;
        case 2: /* CP: Multi-burst flam spikes */
            env = (i < 4 || (i >= 6 && i < 10) || i >= 12) ? (w - i / 2) * 9 / w : 2;
            y = cy - (((i & 1) ? env : -env));
            break;
        case 3: /* CH: Crisp short metallic comb */
            y = cy - ((i < w / 2) ? ((i & 1) ? env : -env) : 0);
            break;
        case 4: /* OH: Open sustained metallic shimmer */
            y = cy - ((i & 1) ? env * 3 / 4 : -env * 3 / 4);
            break;
        case 5: /* TM: Resonant tom sine */
            y = cy - (bg_sin((uint32_t)(i * 6800)) * env >> 15);
            break;
        case 6: /* RS: Sharp rimshot impulse */
            y = cy - (i < 5 ? ((i & 1) ? 9 : -9) : 0);
            break;
        default: /* CB: Dual-square cowbell */
            y = cy - (((i % 5 < 2) ? 1 : -1) * env * 2 / 3);
            break;
        }
        if (i) cv_line(x0 + i - 1, py, x0 + i, y, col);
        py = y;
    }
}

static void graph_colony(const track_t *t, int32_t sel)
{
    const drum_lane_t *K = drum_kit_of(t);
    int32_t morph = clamp(t->p[P_E0] + dx_mot_v, 0, 127), mx = 30 + morph * 180 / 127;
    uint32_t l;

    /* 1. Top X-MOD Morph Ribbon (A <-> B) */
    gw_oct_panel(8, 2, 224, 16, 2, T_BG, T_LINE, 0, T_THEME);
    cv_text(14, 4, &AF_S, "A", morph < 64 ? T_THEME : T_DIM);
    cv_text_r(226, 4, &AF_S, "B", morph >= 64 ? T_THEME : T_DIM, T_BG);
    gw_hbar(30, 7, 180, 6, 18, morph * 2, 48);
    cv_rect(mx - 1, 4, 3, 12, T_ACCENT);

    /* 2. 8 Chamfered Octagonal Drum Forge Pads (4x2 Grid) */
    for (l = 0; l < 8u; l++) {
        int32_t px = 8 + (int32_t)(l % 4u) * 57, py = 22 + (int32_t)(l / 4u) * 48, pw = 53, ph = 44;
        int32_t e = K && K[l].x.live ? K[l].x.ea >> 22 : 0, muted = dx_lane_muted(l);
        int is_sel = (sel == (int32_t)l);
        uint16_t brd = is_sel ? T_ACCENT : e > 12 ? bg_col(140 + e / 2) : T_LINE;
        uint16_t fill = is_sel ? T_RAISE : T_BG;

        gw_oct_panel(px, py, pw, ph, 4, fill, brd, is_sel || e > 16, is_sel ? T_ACCENT : T_THEME);
        cv_text_on(px + 5, py + 2, &AF_S, drum_lane_abbr(t, l),
                   is_sel ? T_ACCENT : muted ? T_DIM : T_TEXT, fill);
        if (!muted && e > 8)
            cv_rect(px + pw - 9, py + 5, 4, 4, bg_col(180 + (e >> 2)));
        gw_drum_wave(l, px + 6, py + 23, pw - 12, muted ? T_DIM : e > 8 ? bg_col(160 + e / 3) : T_THEME);
        gw_hbar(px + 5, py + ph - 8, pw - 10, 5, 6, muted ? 0 : (e > 256 ? 256 : e), 72);
    }
}
