/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT 0.5: the technical graphs of the pages that had none (they showed the scope): what the four knobs do, drawn
 * as an engineer would, live where the sound is (thermal colours: heat_col / heat_y).
 *   DIST       the transfer curve of the drive's TYPE (in -> out), the unity line, the level now as a dot on it; the
 *              tone as a low-pass response at the right
 *   DLY        the echoes as an impulse train: the first at MIX, each FDBK lower, TIME apart; a playhead at tempo
 *   DLY 2      the echoes left (top) and right (bottom): WIDTH sets the right ones later, PITCH steps them up / down,
 *              SPRAY scatters them
 *   REVERB     the impulse response: PRE's gap (CLOUD: a swarm of dots that AIR lifts / sinks), then a dense tail as long as SIZE, cooling as fast as DAMP (SPRING: a
 *              chirp)
 *   REVERB 2   the tail left (up) and right (down), WIDE apart, MOD wobbling it (at RATE, while it sounds), FILT tilting
 *   CHORUS     the taps' delay lines: three sines a third apart, as deep as DEPTH, running at RATE while it sounds,
 *              fanned apart by STER (mono: one line)
 *   MASTER     CLIP's curve with the level now; PUNCH's transient shape; DUCK's gain now as a meter
 *   ENV / LFO DEST  the source now (the envelope's level, the LFO's value) into four bipolar columns under the cards,
 *              each as long as its amount and lit by what it sends now
 *   VOICE      the track's voices: one slot each, as full as its envelope now; the glide as a curve between two notes
 *   GLOBAL     a bar of 16 steps with SWING moving the off-beats, the playhead, the beat */
#define TK_X0 PANEL_X0
#define TK_X1 (PANEL_X0 + PANEL_W)
#define TK_Y0 8
#define TK_Y1 112
/* the current page's knob c as 0..1000 (no knob: 500) */
static int32_t tk(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    return r < 0 ? 500 : r;
}
/* .. as an amount -500..500: a bipolar knob about its middle, a unipolar one from 0 up */
static int32_t tk_amt(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    return d && d->min >= 0 ? tk(c) / 2 : tk(c) - 500;
}
static void tk_axes(int32_t x0, int32_t y0, int32_t x1, int32_t y1)   /* a plot's frame: corner ticks, the axes */
{
    cv_rect(x0, y1, x1 - x0, 1, T_RAISE);
    cv_rect(x0, y0, 1, y1 - y0, T_RAISE);
    cv_rect(x1 - 4, y0, 5, 1, T_RAISE);
    cv_rect(x1, y0, 1, 5, T_RAISE);
}
static void tk_dot(int32_t x, int32_t y) { cv_rrect(x - 3, y - 3, 7, 7, 3, T_TEXT, T_SURF); }

/* DIST: in -> out of dist_shape's maths (the input's full scale +-131072 as the track's mix sees it) */
static int32_t tk_dist_y(uint32_t type, int32_t x, int32_t g, int32_t d)
{
    int32_t v = ((x >> 2) * g) >> 10;
    switch (type) {
    case DT_HEARTH: {
        int32_t h = softclip(v + 4800) - softclip(4800);
        int32_t sp = sine_i((uint32_t)v * 28000u) >> 3;             /* (v1.5.16) as above: the phase wraps */
        return clamp(h + sp, -32767, 32767);
    }
    case DT_FLUX: {
        int32_t jit = (sine_i((uint32_t)x * 9000u) * d) >> 9;      /* (v1.5.16) unsigned: a phase warp, defined */
        int32_t q = (v + jit) & ~(int32_t)((1u << (3u + (uint32_t)d / 20u)) - 1u);
        return softclip(q + (jit >> 1));
    }
    case DT_TAPE: {
        /* (v1.5.16) the TAPE cubic in 64 bits, as fx.c's dist_shape does: v passes 46 000 at full scale
         * and v * v * v overflows int32 there, so the curve drew a fold the audio never had. */
        int32_t x3 = (int32_t)(((int64_t)v * (((int64_t)v * v) >> 15)) >> 15);
        return softclip(v - (x3 / 3));
    }
    case DT_FOLD: {                                     /* (FASE 5) FOLD's own curve, as fx.c folds it: until now the
                                                         * graph drew SOFT's while the engine played this one */
        int32_t T = 8192 >> ((uint32_t)d >> 5), a = v < 0 ? -v : v;
        a &= 2 * T - 1;
        a = a > T ? 2 * T - a : a;
        a <<= 2 + ((uint32_t)d >> 5);
        return clamp(v < 0 ? -a : a, -32767, 32767);
    }
    case DT_CRUSH:                                      /* (FASE 5) CRUSH is a stair, not a curve: the levels the
                                                         * bit depth leaves, drawn as the DSP quantises them */
        return clamp(v & -(1 << (15 - (d >> 4))), -32767, 32767);
    case DT_SOFT:
    default: {
        int32_t b0 = softclip(2400);
        return softclip(v + 2400) - b0;
    }
    }
}
static void tech_dist(const track_t *t)
{
    int32_t d = t->p[P_DIST], g = 4096 + d * d * 2, i, py = 0, x0 = 22, y0 = TK_Y0, s = 100, cx = x0 + s / 2;
    int32_t cy = y0 + s / 2, en = bg_energy(), tone = t->p[P_DTONE];
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_CRUSH ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    tk_axes(x0, y0, x0 + s, y0 + s);
    cv_rect(x0, cy, s, 1, T_LINE);
    cv_rect(cx, y0, 1, s, T_LINE);
    for (i = 0; i <= s; i += 4)                         /* the unity line, dotted */
        cv_rect(x0 + i, y0 + s - i, 1, 1, T_DIM);
    for (i = 0; i <= s; i++) {
        int32_t x = (i - s / 2) * 131072 / (s / 2), y = cy - tk_dist_y(type, x, d ? g : 4096, d) * (s / 2) / 32767;
        y = clamp(y, y0, y0 + s);
        if (i)
            cv_line_t(x0 + i - 1, py, x0 + i, y, heat_y(y < cy ? y : 2 * cy - y, y0, cy, T_THEME), 2);
        py = y;
    }
    if (en > 2) {                                       /* the level now, on the curve */
        int32_t xi = s / 2 + en * (s / 2) / 256;
        tk_dot(x0 + xi, clamp(cy - tk_dist_y(type, en * 512, d ? g : 4096, d) * (s / 2) / 32767, y0, y0 + s));
    }
    {   /* the tone: a low-pass's response, its corner from TONE and DRIVE (fx.c track_dist k) */
        int32_t k = clamp(32000 - d * 95 + tone * 300, 2500, 32767), fx = 140, fw = 82, fc = fx + 8 + (k - 2500) * (fw - 34) / 30267, f;
        tk_axes(fx, y0 + 30, fx + fw, y0 + s);
        py = y0 + 34;
        for (f = 0; f <= fw; f++) {
            int32_t r = fx + f <= fc ? 0 : (fx + f - fc) * (fx + f - fc) / 6, y = clamp(y0 + 34 + r, y0 + 34, y0 + s);
            if (f)
                cv_line_t(fx + f - 1, py, fx + f, y, heat_y(y, y0 + 34, y0 + s, T_THEME), 2);
            py = y;
        }
    }
}

/* (v2.1) THE ECHO TUNNEL. The page drew six bars on an axis: the repeats were there, but a delay is depth, and a
 * row of columns does not read as depth. This is the one picture the effect has always had in every piece of gear
 * that printed it: a dry frame and, inside it, the repeats -- each one smaller, dimmer, shifted by SPREAD, tilted by
 * PITCH, thrown side to side when PING-PONG is on, and coloured along the palette instead of one tone. The number of
 * frames is the number the sound actually has: the feedback decayed to 1 % of the dry hit, counted, not guessed. The
 * repeat that is sounding now is the one drawn at full accent, so the tunnel moves with the train. */
static int32_t dly_echoes(int32_t fb)
{
    int32_t n = 1, amp = 1000;                      /* 1 = the dry hit; the repeats follow while they are audible */
    if (fb < 6)
        return 1;
    while (n < 12 && (amp = amp * fb / 127) > 10)
        n++;
    return n;
}
static void dly_tunnel(int32_t split);
static void tech_dly(void) { dly_tunnel(0); }

/* (v2.1) DLY 2: THE SAME TUNNEL, ONE PER SIDE. The pitch grains were six bars over a centre line; the tunnel is
 * the same picture the main DLY page draws, split in two, with PITCH tilting each side the way the grains detune
 * them (left up, right down) and WIDTH deciding how far the two sides are thrown apart. One function draws both,
 * so the pair can never say different things about the same effect. */
static void dly_tunnel(int32_t split)
{
    int32_t x0 = TK_X0 + 6, w = PANEL_W - 12, top = TK_Y0 + 2, base = TK_Y1 - 2;
    int32_t gen = (int32_t)fx_dly_gen, on = song.g[G_DMIX] > 0;
    int32_t fb = clamp(song.g[G_DFDBK], 0, 127), tms = clamp(song.g[G_DTIME], 0, 57);
    int32_t spr = clamp(song.g[G_DSPRY], -64, 63), pit = clamp(song.g[G_DPIT], -24, 24);
    int32_t ping = song.g[G_DPING] > 0, i, side;
    int32_t nf = dly_echoes(fb);
    
    for (side = 0; side < (split ? 2 : 1); side++) {
        int32_t cx = split ? x0 + w / 4 + side * (w / 2) : x0 + w / 2;
        int32_t cy = split ? (top + base) / 2 : (top + base) / 2;
        int32_t mx = (split ? w / 4 : w / 2) - 3, my = (base - top) / 2 - 2;
        int32_t sgn = side ? -1 : 1;
        for (i = nf - 1; i >= 0; i--) {
            int32_t ix = mx * (nf - i) / nf + i, iy = my * (nf - i) / nf + i / 2;
            int32_t dx = 0, dy = 0, live = on && gen == i;
            uint16_t ink;
            if (ix < 3 || iy < 2)
                continue;
            if (ping && i)
                dx = ((i & 1) ? 1 : -1) * ix / 3;
            dx += spr * i * (split ? 1 : 2) / 90;
            dy += sgn * pit * i * (split ? 2 : 3) / 24;
            ink = !on ? T_RAISE
                : live ? T_TEXT
                       : ux_mix(T_BG, i == 0 ? T_THEME : heat_col(clamp(120 + i * 26, 40, 236)),
                                90 + (nf - i) * 60 / nf);
            cv_frame(cx - ix + dx, cy - iy + dy, ix * 2, iy * 2, ink);
            if (live) {
                cv_rect(cx - ix + dx, cy - iy + dy, ix * 2, 1, T_ACCENT);
                cv_rect(cx - ix + dx, cy + iy + dy - 1, ix * 2, 1, T_ACCENT);
            }
        }
    }
    if (split && on)
        cv_rect(x0 + w / 2 - 1, top + 2, 1, base - top - 4, T_LINE);   /* the split between left and right */
}
static void tech_dly2(void) { dly_tunnel(1); }

/* (3.1) THE REVERB ANSWERS THE NOTES. fx_vu[5] is the peak the reverb bus actually returned this
 * frame (fx.c publishes it), so the picture follows the effect, not a timer: a note starts a tail,
 * the envelope rises at once and falls slowly, and the expansion phase only advances while there is
 * something to show. One compare and one shift a frame; no per-sample work, no allocation, and no
 * lock between the DSP and the UI (the UI reads the published peak, as every other meter does). */
static uint8_t rev_act;
static int32_t rev_ph;
static uint32_t rev_fr = 0xFFFFFFFFu;
static void rev_tick(void)
{
    int32_t lvl;
    if (rev_fr == ui.frame)
        return;
    rev_fr = ui.frame;
    lvl = clamp((int32_t)fx_vu[5], 0, 32767) * 255 / 32767;
    if (lvl >= (int32_t)rev_act)
        rev_act = (uint8_t)lvl;                         /* (3.1) the note: lit at once */
    else                                                /* the tail: fall towards the level it has now (7/8 a
                                                         * drawn frame): no oscillation under a steady return, one
                                                         * multiply and a shift, and it reaches 0 -- the graph rests */
        rev_act = (uint8_t)(lvl + (((int32_t)rev_act - lvl) * 7 >> 3));
    if (rev_act > 4)
        rev_ph = (rev_ph + 4) & 255;
    else
        rev_ph = 0;
}

/* REVERB: the impulse response */
static void tech_rev(void)
{
    int32_t size = song.g[G_RSIZE], damp = song.g[G_RDAMP], pre = song.g[G_RPRE], spring = song.g[G_RTYPE] == 1;
    int32_t x0 = TK_X0 + 6, w = PANEL_W - 12, base = TK_Y1 - 4, h = 92, len = 40 + size * (w - 50) / 127;
    int32_t gap = song.g[G_RTYPE] <= 1 ? clamp(pre, 0, 100) * 40 / 100 : 0;   /* (PRE: ROOM, SPRING; else AIR) */
    int32_t i, ty = song.g[G_RTYPE], mod = song.g[G_RMOD];
    rev_tick();
    tk_axes(x0, TK_Y0, x0 + w, base);
    if (ty == 3) {                                      /* RESO: the room's four strings resolved into combs, each
                                                         * ringing as long as SIZE lets it and as bright as DAMP leaves
                                                         * it, with the energy travelling along it */
        /* (3.2) RESO and CLOUD were the same picture twice (the owner: "los que tienen la nueva animacion
         * tienen todos la misma"). RESO is now a RESOLVER: fixed strings, a comb whose teeth get finer as
         * SIZE rises, and DOTS RUNNING ALONG each string (the resolution's energy), brighter with what the
         * bus returns. CLOUD below is a swarm that breathes and puffs. Different geometry, different motion,
         * both from tables and ui.frame: no per-pixel trigonometry, no allocation. */
        int32_t nd = 3 + song.g[G_RRATE] / 32, fr = (int32_t)((ui.frame * (1u + (uint32_t)rev_act / 32u)) & 63u);
        for (i = 0; i < 4; i++) {
            int32_t x = x0 + 30 + i * 44 + (mod > 64 && i >= 2 ? 10 : 0), k, hh = 30 + size * 55 / 127;
            int32_t len = clamp(hh * 3 / 2, 20, w - (x - x0) - 8);
            uint16_t sc = heat_col(clamp(200 - i * 22 - damp / 2 + (int32_t)rev_act / 6, 20, 236));
            for (k = 0; k < 6; k++) {                       /* the comb: six teeth, finer as SIZE rises */
                int32_t th = hh - k * hh / (3 + (127 - size) / 20 + 1), yy;
                if (th <= 0)
                    break;
                yy = base - th;
                cv_rect(x + k * (6 + i), yy, 2, th, heat_col(clamp(220 - k * 30 - damp, 20, 236)));
            }
            cv_rect(x - 2, base - hh - 6, 1, hh + 6, T_MID);
            cv_rect(x - 2, base - hh - 6, len, 1, ux_mix(T_BG, sc, 55));    /* the string itself */
            for (k = 0; k < nd; k++) {                      /* the energy running out along it: the resolution */
                int32_t ph = (fr * (2 + i) + k * (len / nd) + i * 7) % len;
                int32_t y = base - hh - 6 + (ph * (hh + 6) / len);
                uint16_t dc = ux_mix(T_BG, bg_col(clamp(150 + (int32_t)rev_act / 2 + ph * 90 / len, 40, 250)),
                                     70 + (int32_t)rev_act / 4);
                cv_rect(x - 2 + ph, y, 2, 1, dc);
                if ((k & 1) == 0)
                    cv_pset(x - 2 + ph - 1, y, ux_mix(T_BG, dc, 45));      /* a soft head on every other one */
            }
        }
        if (rev_act > 10) {                             /* a note: each string answers with a travelling pulse;
                                                         * the old orbit ring wrapped outside the panel and read as a glitch */
            int32_t i2, pass;
            for (pass = 0; pass < 2; pass++)
            for (i2 = 0; i2 < 4; i2++) {
                int32_t xx = x0 + 30 + i2 * 44 + (mod > 64 && i2 >= 2 ? 10 : 0);
                int32_t hh2 = 30 + size * 55 / 127;
                int32_t len2 = clamp(hh2 * 3 / 2, 20, w - (xx - x0) - 8);
                int32_t pulse = ((rev_ph + pass * 80) * (3 + i2 + pass)) % (len2 > 0 ? len2 : 1);
                int32_t yy = base - hh2 - 6 + pulse * (hh2 + 6) / (len2 > 0 ? len2 : 1);
                if (yy > TK_Y0 && yy < base - 2 && xx - 2 + pulse > x0 && xx - 2 + pulse < x0 + w)
                    cv_rrect(xx - 3 + pulse, yy - 2, pass == 0 ? 5 : 3, pass == 0 ? 4 : 3, 2,
                             ux_mix(T_BG, bg_col(220 + (int32_t)rev_act / 6 + pass * 15),
                                    (pass == 0 ? 70 : 45) + (int32_t)rev_act / 4), T_SURF);
            }
        }
        if (rev_act > 24) {                             /* a strong note: a bright resonance flash on each string */
            int32_t i3;
            for (i3 = 0; i3 < 4; i3++) {
                int32_t xx = x0 + 30 + i3 * 44 + (mod > 64 && i3 >= 2 ? 10 : 0);
                int32_t hh2 = 30 + size * 55 / 127;
                if (xx > x0 && xx < x0 + w)
                    cv_rect(xx - 1, base - hh2 - 8, 3, 2, ux_mix(T_BG, bg_col(245), 60 + (int32_t)rev_act / 5));
            }
        }
        return;
    }
    if (ty == 4) {                                      /* CLOUD (0.6.12): a swarm of dots, as many as RATE, drifting back
                                                         * over SIZE while it sounds; AIR lifts or sinks the whole cloud
                                                         * (and warms / cools it), MOD splits it into octave layers;
                                                         * the grains playing now bigger; FREEZE: a line over them */
        int32_t air = clamp(pre, -24, 24), cy = (TK_Y0 + base) / 2 - air * 30 / 24, nd = 24 + song.g[G_RRATE] / 3;
        int32_t breath = 1000 + (bg_sin((uint32_t)ui.frame * 900u) >> 7) * 40 / 256,   /* the cloud fills and empties */
                grow = 1000 + (int32_t)rev_act * 8 / 5;                                 /* .. and a note pushes it out */
        int32_t spread = (40 + size * (w - 40) / 127) * clamp(breath * grow / 1000, 700, 1600) / 1000;
        int32_t live = bg_energy() > 2 || rev_act > 6, k;
        uint32_t fr = live ? ui.frame : 0u;
        /* (3.2) CLOUD: a swarm, not strings. Three depths of grain (far small and dim, near large and
         * bright), the whole cloud breathing, MOD splitting it into octave layers, and on a note a puff
         * that leaves the middle -- a shape RESO does not have. Same table field as before. */
        for (i = 0; i < nd; i++) {
            uint32_t hsh = (uint32_t)(i + 1) * 2654435761u, sp = 1u + (hsh >> 28);
            int32_t depth = (int32_t)((hsh >> 26) % 3u), sz = depth == 0 ? 1 : depth == 1 ? 2 : 3;
            int32_t lane = (int32_t)((hsh >> 20) % 100u) < mod * 3 / 4 ? (int32_t)((hsh >> 24) % 3u) + 1 : 0;
            int32_t dx = (int32_t)(((hsh >> 8) + fr * sp) % (uint32_t)spread);
            int32_t gx = x0 + w - 6 - dx, wob = (bg_sin((hsh >> 6) + fr * 900u) * (3 + depth * 2)) >> 15;
            int32_t gy = cy - (lane == 1 ? 24 : lane == 2 ? -24 : lane == 3 ? 12 : 0) + (int32_t)((hsh >> 4) % 13u) - 6 + wob;
            int32_t fade = 1000 - dx * 1000 / (spread ? spread : 1);    /* (older, further back: cooler) */
            uint16_t c = heat_col(clamp(fade * (140 + depth * 30) / 1000 + air * 2 - damp / 3 + 20, 8, 240));
            if (gy > TK_Y0 + 1 && gy < base - 2 && gx > x0 && gx < x0 + w - 2)
                cv_rect(gx, gy, sz, sz, c);
        }
        if (rev_act > 8) {                              /* the puff: the note's front leaving the cloud */
            int32_t pr = 4 + (rev_ph & 63), ph2 = 26 + (rev_ph & 31);
            gw_orbit_ring(x0 + w / 2, cy, pr + ph2, (pr + ph2) * 2 / 3,
                          ux_mix(T_BG, bg_col(200), clamp(120 + (int32_t)rev_act / 2 - pr * 2, 20, 220)), 1);
        }
        for (k = 0; k < (int32_t)CL_N; k++) {            /* the grains now: their place in their life, their pitch */
            int32_t gx, gy, up;
            if (!cl_g[k].len || !live) continue;
            gx = x0 + w - 6 - (int32_t)cl_g[k].age * spread / (int32_t)cl_g[k].len;
            up = cl_g[k].inc > 50000u ? 24 : cl_g[k].inc < 24000u ? -24 : 0;
            gy = cy - up + (int32_t)(k % 5u) * 3 - 6;
            if (gy > TK_Y0 + 2 && gy < base - 3)
                cv_rrect(gx - 2, gy - 2, 5, 5, 2, heat_col(clamp(200 + air * 2, 60, 250)), T_SURF);
        }
        cv_rect(x0 + 2, cy, 3, 1, T_MID);                /* (where AIR has it) */
        if (size >= 127)
            cv_rect(x0 + 4, TK_Y0 + 4, w - 8, 1, T_ACCENT);
        return;
    }
    if (rev_act > 10) {                                 /* the tail's answer: three wavefronts leaving the hit,
                                                         * drawn before the data so they never hide it */
        int32_t cxr = x0 + 6 + gap + len / 2, cyr = base - 40, k, rad = 5 + (rev_ph & 63);
        for (k = 0; k < 3; k++) {
            int32_t rr = rad + k * 13;
            if (rr > 54)
                continue;
            gw_orbit_ring(cxr, cyr, rr, rr * 2 / 3,
                          ux_mix(T_BG, bg_col(150 + rev_act / 3), 40 + k * 22), 1);
        }
    }
    cv_rect(x0, base - h, 3, h, T_TEXT);                /* the dry hit */
    /* (v2.1) THE TAIL AS A STAIRCASE. The decay was a cloud of 1 px columns: a texture, not a shape, and at 240 px
     * the eye cannot follow it. The same envelope is now a row of 3 px bars with a 1 px gap -- the bars of the
     * reference -- so the slope of the decay, its length with SIZE and its brightness with DAMP read straight off
     * the picture. Nothing about the maths changed: same envelope, same breathing with the bus, same SPRING ripple. */
    for (i = 0; i < len && x0 + 6 + gap + i < x0 + w - 3; i += 4) {
        int32_t env = 1000 - i * 1000 / len, r = (int32_t)(((uint32_t)(i + 7) * 2654435761u) >> 22) % 1000;
        int32_t bh = env * (400 + r * 6 / 10) / 1000 * h / 1000;
        int32_t heat = clamp(env * 230 / 1000 - i * damp / (len + 1) + (int32_t)rev_act / 8, 6, 246);
        if (rev_act > 4)                                /* the tail breathes with what the bus returns */
            bh += bh * (int32_t)rev_act / 900;
        if (spring)
            bh = bh * (600 + (bg_sin((uint32_t)(i * i * 9)) >> 6) * 400 / 512) / 1000;
        if (bh > 0) {
            cv_rect(x0 + 6 + gap + i, base - bh, 3, bh, heat_col(heat));
            cv_rect(x0 + 6 + gap + i, base - bh, 3, 1, ux_mix(heat_col(heat), T_TEXT, 55));   /* the bar's head */
        }
        if (ty == 2 && mod && i % 4 == 0 && bh > 4) {   /* SHIMMER: sparks an octave (+ AIR) over the tail, denser
                                                         * and animated; a second layer lifts them further on a note */

            int32_t air = clamp(pre, -24, 24), sy = base - bh - 6 - (i * 7 % 23) * mod / 127 - air * 18 / 24
                       + (int32_t)((bg_sin((uint32_t)(ui.frame * 1200u + i * 31u)) >> 12) * (2 + (int32_t)rev_act / 50));
            if (sy > TK_Y0 && sy < base - 2) {
                cv_rect(x0 + 6 + gap + i, sy, 2, 2, heat_col(clamp(heat + 40 + air * 3, 0, 250)));
                if (rev_act > 10)
                    cv_rect(x0 + 6 + gap + i + 1, sy - 3 - (int32_t)rev_act / 40, 1, 1,
                            heat_col(clamp(heat + 70 + air * 3, 0, 250)));
            }
        }
    }
    if (ty == 0) {                                     /* ROOM: a small hall's comb teeth, breathing with the
                                                         * return: the front four, their ghosts behind, and a second
                                                         * row that wobbles as the tail does -- a room, not a string */
        int32_t k;
        for (k = 1; k <= 4; k++) {
            int32_t cx = x0 + 6 + gap + k * len / 5;
            int32_t wob = (int32_t)((bg_sin((uint32_t)(ui.frame * 150u + k * 90u)) >> 11) * (2 + (int32_t)rev_act / 40));
            int32_t ch = 4 + k * 3 + (int32_t)rev_act / 24 + wob;
            if (cx > x0 + w - 2) break;
            cv_rect(cx - 1, base - ch / 2, 3, ch / 2, ux_mix(T_BG, bg_col(95 + k * 14), 14 + (int32_t)rev_act / 8));   /* ghost */
            cv_rect(cx, base - ch, 1, ch, ux_mix(T_BG, bg_col(120 + k * 20), 35 + (int32_t)rev_act / 5));
            cv_rect(cx + 2, base - (2 + k) - wob / 2, 1, 2 + k, ux_mix(T_BG, bg_col(150 + k * 12), 28 + (int32_t)rev_act / 6));
        }
    }
    if (ty == 1) {                                               /* SPRING: a coil that wobbles while it sounds, and a
                                                         * chirp that travels the tail -- distinct from ROOM's teeth */
        int32_t i;
        int32_t amp = 2 + song.g[G_RMOD] / 40 + (int32_t)rev_act / 45;
        for (i = 0; i + 5 < len; i += 6) {
            int32_t xx = x0 + 6 + gap + i;
            int32_t yy = base - 3 + (((i / 6) & 1) ? -amp : amp)
                       + (int32_t)((bg_sin((uint32_t)(ui.frame * 900u + i * 17u)) >> 13) * amp / 4);
            if (xx > x0 + w - 3) break;
            cv_rect(xx, yy, 2, 2, ux_mix(T_BG, bg_col(150 + i * 2), 30 + (int32_t)rev_act / 6));
            cv_rect(xx + 2, yy - (((i / 6) & 1) ? 2 : -2), 2, 2, ux_mix(T_BG, bg_col(175 + i * 2), 22 + (int32_t)rev_act / 8));
        }
    }
    if (ty == 1 && rev_act > 3) {                      /* SPRING: one or two chirp markers travelling the tail */
        int32_t cp = x0 + 6 + gap + ((int32_t)rev_ph * len / 256) % (len > 1 ? len : 1);
        if (cp > x0 + 5 && cp < x0 + w - 2)
            cv_rrect(cp - 2, base - 9, 5, 9, 2, ux_mix(T_BG, bg_col(210), 60 + (int32_t)rev_act / 4), T_SURF);
        if (rev_act > 18) {
            int32_t cp2 = x0 + 6 + gap + (((int32_t)rev_ph * 3 / 2) * len / 256) % (len > 1 ? len : 1);
            if (cp2 > x0 + 5 && cp2 < x0 + w - 2)
                cv_rrect(cp2 - 1, base - 5, 3, 5, 1, ux_mix(T_BG, bg_col(235), 45 + (int32_t)rev_act / 5), T_SURF);
        }
    }
}

/* (v2.1) REVERB 2: THE SAME TAIL, ONE PER SIDE. The bar row of the REVERB page, split over the centre line: the
 * left channel's decay up, the right channel's down, WIDE deciding how far apart the two rows start and FILT how
 * warm they are. The wobble is one offset a bar, not a sine a column, so the page costs less than the columns did. */
static void tech_rev2(void)
{
    int32_t mod = tk(0), wide = tk(3) * 8 / 1000, filt = song.g[G_RFILT];
    int32_t x0 = TK_X0 + 6, w = PANEL_W - 12, mid = (TK_Y0 + TK_Y1) / 2, i;
    int32_t wob = (int32_t)((bg_sin((uint32_t)(ui.frame * 900u)) * mod) >> 12) * 4 / 1000;
    rev_tick();
    cv_rect(x0, mid, w, 1, T_RAISE);
    for (i = 0; i + 3 < w; i += 4) {
        int32_t env = 1000 - i * 1000 / w;
        int32_t bh = env * (44 + (int32_t)rev_act * 26 / 255) / 1000;
        int32_t heat = clamp(env * 200 / 1000 + filt + (int32_t)rev_act / 8, 6, 246);
        if (bh <= 0)
            continue;
        cv_rect(x0 + i, mid - 2 - wide - bh - wob / 4, 3, bh, heat_col(heat));
        cv_rect(x0 + i, mid + 2 + wide + wob / 4, 3, bh, heat_col(heat - 12));
    }
}



/* (v2.1) THE CHORUS'S TWO CHANNELS, AS TWO FIGURES. Three parallel sines say "it modulates"; they do not say which
 * side is ahead, how far apart the voices sit, or how wide the bus is. A chorus' data is two delay lanes sweeping in
 * quadrature, and a figure-eight is exactly that pair plotted against each other: X is the lane, Y is the sweep. Left
 * is drawn on the left of the panel and right on the right, their phase offset is STEREO (0: one line, exactly as the
 * DSP's taps line up at 0), RATE turns the sweep, DEPTH its height. Both are read from the same parameters the effect
 * runs, and both stop moving when nothing is sounding -- the same rule as every other graph here. */
static void tech_chorus(void)
{
    int32_t rate = tk(0), depth = tk(1), ster = clamp(song.g[G_CSTER], -64, 63);
    int32_t x0 = TK_X0 + 6, w = PANEL_W - 12, top = TK_Y0 + 2, base = TK_Y1 - 2, k;
    int32_t cy = (top + base) / 2;
    int32_t amp = ((base - top) / 2 - 16) * (300 + depth * 700 / 1000) / 1000;   /* tk() is per-mille */
    uint32_t ph = bg_energy() > 2 ? ui.frame * (uint32_t)(40 + rate) * 8u : 0u;
    cv_rect(x0, cy, w, 1, T_RAISE);                     /* the zero line, quiet under both figures */
    /* (v2.3.9) THE DEPTH IS THE SAME STACK THE CURVES USE. The lane used to be drawn three times with a per-copy
     * phase nudge and a fixed lift: three bright lines of the same weight a few pixels apart, which at this height
     * read as one thick smear -- the figure said "it moves" and nothing about where it sits. It is now the stack
     * MORPH, TILT and FILTER wear: four passes of the SAME sweep, each one flatter (sy = 1000 - k*80) and lifted
     * (dy = -k*5), inked through qz_back_ink so the far ones sink towards the page's own ground. The front pass,
     * the one that is the effect's data, is the only one at full ink and two pixels thick. */
    for (k = 0; k < 2; k++) {                           /* k 0 = left, 1 = right */
        int32_t lane = cy + (k ? 1 : -1) * (6 + ster * 22 / 63);   /* STEREO: how far apart the two lanes sit */
        int32_t po = (int32_t)(ph / 40) + (k ? ster * 9 / 16 : 0); /* and how far ahead the right one is */
        uint16_t ink = k ? T_ACCENT : T_THEME;
        int32_t g;
        cv_text_on(x0 + 2 + k * 14, top + 12, &AF_S, k ? "R" : "L", ux_mix(T_SURF, ink, 70), T_SURF);
        for (g = 3; g >= 0; g--) {                      /* back to front, so the near sweep is never covered */
            int32_t sy = 1000 - g * 80, dy = -g * 5;
            int32_t a_k = amp * sy / 1000, lane_k = lane + dy;
            uint16_t gi = g ? qz_back_ink(ink, sy) : ink;
            int32_t px = 0, py = lane_k, i2;
            for (i2 = 0; i2 <= w; i2++) {
                uint32_t a = (uint32_t)i2 * (3u * 65536u) / (uint32_t)w + (uint32_t)po * 1024u;
                int32_t y = lane_k - ((bg_sin(a) * a_k) >> 15) * (k ? 3 : 4) / 4;
                if (i2)
                    cv_line_t(x0 + px, clamp(py, top, base), x0 + i2, clamp(y, top, base), gi, g ? 1 : 2);
                px = i2;
                py = y;
            }
        }
    }
    cv_rect(x0 + w / 2 - 1, top + 2, 1, base - top - 4, T_LINE);   /* the split between the two channels */
}

/* MASTER EQ: the response of the same Q24 biquad the audio path runs, on a logarithmic
 * 20 Hz .. 20 kHz X axis. (1.8.6) The old graph plotted the parameter index as if it were
 * a frequency, put 125 / 2k / 20k at the wrong columns and let -12 overlap the bottom
 * labels; it also clamped a bad Y in a way that could fold the line. This maps X once,
 * maps dB symmetrically around 0 dB, and clamps both ends before drawing. */
static const uint16_t EQ_AXIS_HZ[33] = {
    20, 25, 31, 38, 47, 59, 73, 91, 112, 140, 173, 215, 267, 331, 411, 510,
    632, 785, 974, 1209, 1500, 1861, 2310, 2866, 3557, 4413, 5477, 6796, 8434, 10466,
    12988, 16117, 20000
};
static int32_t eq_hz_at(int32_t i)                   /* x 0..212 -> Hz, log spaced */
{
    int32_t q = i * 32 / 212, r = i * 32 % 212;
    if (q >= 32)
        return EQ_AXIS_HZ[32];
    return EQ_AXIS_HZ[q] + (EQ_AXIS_HZ[q + 1] - EQ_AXIS_HZ[q]) * r / 212;
}
static int32_t eq_hz_x(int32_t hz)                  /* Hz -> x 14..226 */
{
    int32_t i = 0;
    while (i < 31 && hz > EQ_AXIS_HZ[i + 1])
        i++;
    /* The table's intervals are 212 / 32 px each; divide once, not once per term,
     * so 20 kHz lands on the graph's right edge (x 226), not short of it. */
    return 14 + (i * 212 + (hz - EQ_AXIS_HZ[i]) * 212 / (EQ_AXIS_HZ[i + 1] - EQ_AXIS_HZ[i])) / 32;
}
static int32_t eq_param_hz(void)                    /* the cutoff the audio filter uses */
{
    return (int32_t)EQ_HZ[clamp(song.g[G_EQFREQ], 0, 127)];
}
/* "30" / "1.2k": the frequency the control point sits on, as the page reads it */
static void eq_hz_txt(char *b, int32_t hz, int32_t n)
{
    char t[8];
    if (hz < 1000) {
        fmt_int(b, hz);
        return;
    }
    fmt_int(t, hz / 1000);
    str_cpy(b, t, (uint32_t)n);
    str_cpy(b + str_len(b), ".", (uint32_t)n - str_len(b));
    fmt_int(t, (hz % 1000) / 100);
    str_cpy(b + str_len(b), t, (uint32_t)n - str_len(b));
    str_cpy(b + str_len(b), "k", (uint32_t)n - str_len(b));
}
/* Q24.8 dB -> "+9.6 dB" */
static void eq_db_txt(char *b, int32_t q8, int32_t n)
{
    char t[8];
    int32_t a = q8 < 0 ? -q8 : q8;
    b[0] = 0;
    str_cpy(b, q8 < 0 ? "-" : "+", (uint32_t)n);
    fmt_int(t, a / 256);
    str_cpy(b + str_len(b), t, (uint32_t)n - str_len(b));
    str_cpy(b + str_len(b), ".", (uint32_t)n - str_len(b));
    fmt_int(t, (a % 256) * 10 / 256);
    str_cpy(b + str_len(b), t, (uint32_t)n - str_len(b));
    str_cpy(b + str_len(b), " dB", (uint32_t)n - str_len(b));
}
static void tech_master_eq(void)
{
    int32_t mode = clamp(song.g[G_EQMODE], 0, 4);
    int32_t y0 = TK_Y0, gx0 = 32, gw = 192, gtop = y0 + 20, gbot = y0 + 84, i;
    int32_t mid_y = y0 + 53, px = gx0, py = mid_y;
    static const char *const MODES[] = {"BYPASS", "PEAK/BELL", "NOTCH", "LOW SHELF", "HIGH SHELF"};

    gw_oct_panel(8, y0 - 2, 224, 104, 4, T_BG, T_LINE, mode > 0, T_ACCENT);
    cv_rect(12, y0 + 2, 216, 14, T_RAISE);
    cv_text_on(16, y0 + 4, &AF_S, "MASTER BIQUAD EQ", T_THEME, T_RAISE);
    cv_text_on(140, y0 + 4, &AF_S, MODES[mode], mode ? T_ACCENT : T_DIM, T_RAISE);

    /* (3.4) EDIT > FILTER's bay is the reference and this is its language: a named logarithmic grid, a dB scale in
     * a gutter the curve never crosses, the band's own energy behind it as a backdrop, the area between the curve
     * and 0 dB filled so a boost and a cut read at a glance, the shelf's plateau and the bell's width drawn from the
     * parameters, and the control point with its frequency and gain written on it. Every number comes from
     * fx_master_eq_hz_db(), the Q24 biquad the audio path runs: a drawing of the filter, not of the knobs. */
    cv_rect(gx0, mid_y, gw, 1, T_LINE);
    cv_rect(gx0, mid_y - 22, gw, 1, T_RAISE);
    cv_rect(gx0, mid_y + 22, gw, 1, T_RAISE);
    {
        int32_t mark_hz = eq_param_hz(), mark_x, mark_y, mark_db;
        int32_t gain_db = clamp(song.g[G_EQGAIN], -24, 24), q = clamp(song.g[G_EQQ], 0, 127);
        char b[32];
        /* Hz -> x on the plot: the axis table spans 212 px and the plot 192, in its own frame */
        #define EQ_X(hz) (gx0 + (clamp(eq_hz_x(hz), 14, 226) - 14) * gw / 212)
        for (i = 0; i < 3; i++) {
            static const int32_t GL[3] = {100, 1000, 10000};
            static const char *const GN[3] = {"100", "1k", "10k"};
            int32_t gx = EQ_X(GL[i]);
            cv_rect(gx, gtop, 1, gbot - gtop, T_GRID);
            if (i < 2)                                 /* (10k's line has no room for a name before 20k's) */
                cv_text_on(gx - 7, y0 + 87, &AF_S, GN[i], T_DIM, T_BG);
        }
        cv_rect(gx0, gtop, 1, gbot - gtop, T_GRID);
        cv_text_on(10, y0 + 87, &AF_S, "20", T_LINE, T_BG);
        cv_text_r(230, y0 + 87, &AF_S, "20k", T_LINE, T_BG);
        /* the gain scale, in the gutter: the plot runs +-24 dB, so +12 and -12 sit halfway to each end */
        cv_text_on(10, mid_y - 30, &AF_S, "+12", T_DIM, T_BG);
        cv_text_on(14, mid_y - 6, &AF_S, "0", T_DIM, T_BG);
        cv_text_on(8, mid_y + 18, &AF_S, "-12", T_DIM, T_BG);
        /* (7.3) THE FLOOR UNDER THE CURVE: the console's four-band ramp (ui_fxb.c fxb_grad) between the response and
         * the plot's floor, so a boost has a body and a cut reads as a depth. Drawn before the energy bars and the
         * curve, so both keep their contrast; two columns a step, mixed once per band. */
        {   /* (1.2.6) THE MAIN SCREEN'S GRADIENT, UNDER THE CURVE. The fill used to be six staggered slabs mixed
             * from T_BG towards the theme and the accent: a local blend that restarted under every column, so the
             * body read as texture instead of as light -- the same defect the owner reported on FX's first page.
             * It is the ground the console's graphs are drawn on now (HOME's views, hv_grad; FX > BUSES' blocks,
             * fxb_grad: ux_ground_rows, solid, panel 22..62 % and accent 10..45 % from the top of the plot to its
             * floor), resolved by ABSOLUTE row through the clip, so one height is one colour in every column and
             * the fill is the page's own gradient showing through. Solid rows: one fill per colour the palette has,
             * not one per pixel. */
            for (i = 0; i <= gw; i += 2) {
                int32_t db2 = fx_master_eq_hz_db(eq_hz_at(i * 212 / gw));
                int32_t y2 = clamp(mid_y - db2 * 44 / (24 * 256), gtop, gbot);
                if (y2 + 1 >= gbot)
                    continue;
                ux_ground_rows(gx0 + i, gtop, 2, gbot - gtop, 22, 62, 10, 45, y2 + 1, gbot, 100);
            }
        }
        /* the band's energy, under everything: what the EQ is shaping, not a measurement of the EQ */
        for (i = 0; i < 28; i++) {
            int32_t bx = gx0 + 2 + i * (gw - 4) / 28, bw = (gw - 4) / 28 - 1;
            uint32_t idx = (scope_w - 128u + (uint32_t)i * 4u) & (SCOPE_N - 1u);
            int32_t s = scope_buf[idx] - scope_buf[(idx + 2u) & (SCOPE_N - 1u)];
            int32_t bh;
            if (s < 0)
                s = -s;
            bh = clamp(s * 14 / 9000 + 2, 2, 14);
            cv_rect(bx, gbot - bh, bw < 1 ? 1 : bw, bh, ux_mix(T_BG, bg_col(68), 42));
        }
        mark_x = EQ_X(mark_hz);
        mark_db = fx_master_eq_hz_db(mark_hz);
        mark_y = clamp(mid_y - mark_db * 44 / (24 * 256), gtop, gbot);
        for (i = 0; i <= gw; i++) {
            int32_t db = fx_master_eq_hz_db(eq_hz_at(i * 212 / gw));
            int32_t y = clamp(mid_y - db * 44 / (24 * 256), gtop, gbot);
            /* (1.2.6) THE LIFT AND THE CUT ARE A GRADIENT, not one flat 40 % slab: each column is filled in three
             * steps whose strength follows the row's own place in the plot, so the body brightens towards the
             * 0 dB line the way the console's grounds brighten towards their floor (HOME's views, hv_grad). The
             * colours stay the ones that say what the shape does -- theme for a lift, accent for a cut. */
            if (db > 0 && y < mid_y) {
                int32_t k3, span3 = mid_y - y;
                for (k3 = 0; k3 < 3; k3++) {
                    int32_t a3 = y + span3 * k3 / 3, b3 = y + span3 * (k3 + 1) / 3;
                    int32_t fr = (b3 - gtop) * 100 / (gbot - gtop);
                    if (b3 > a3)
                        cv_rect(gx0 + i, a3, 1, b3 - a3, ux_mix(T_BG, T_THEME, 18 + fr * 27 / 100));
                }
            } else if (db < 0 && y > mid_y) {
                int32_t k3, span3 = y - mid_y;
                for (k3 = 0; k3 < 3; k3++) {
                    int32_t a3 = mid_y + span3 * k3 / 3, b3 = mid_y + span3 * (k3 + 1) / 3;
                    int32_t fr = (b3 - gtop) * 100 / (gbot - gtop);
                    if (b3 > a3)
                        cv_rect(gx0 + i, a3, 1, b3 - a3, ux_mix(T_BG, T_ACCENT, 18 + fr * 27 / 100));
                }
            }
            if (i) {
                int32_t pct = clamp(45 + (mid_y - y) * 4, 0, 100);
                cv_line_t(px, py, gx0 + i, y, ux_mix(T_THEME, T_ACCENT, pct), 2);
            }
            px = gx0 + i;
            py = y;
        }
        /* (3.2) What the shape means, which the knob's own number cannot say: a shelf settles on a plateau (dashed,
         * on its side of the corner), a bell and a notch have a width the Q gives them (a bracket at the band's
         * level, end ticks included). A corner at the axis edge would leave no plateau to dash, so the line is held
         * to a readable length on its own side. */
        if (mode == 3 || mode == 4) {
            int32_t py2 = clamp(mid_y - gain_db * 44 / 24, gtop, gbot);
            int32_t xa = mode == 3 ? gx0 : mark_x + 1, xb = mode == 3 ? mark_x : gx0 + gw;
            if (mode == 3 && xb < gx0 + 30)
                xb = gx0 + 30;
            if (mode == 4 && xa > gx0 + gw - 30)
                xa = gx0 + gw - 30;
            for (i = xa; i + 2 < xb; i += 6)
                cv_rect(i, py2, 3, 1, T_MID);
        } else if (mode == 1 || mode == 2) {
            int32_t qx = 8 + q;                            /* Q ~ qx / 16: 0,5 .. 8,4 */
            int32_t half = mark_hz * 8 / qx;               /* half the band's width, Hz */
            int32_t xa = EQ_X(mark_hz - half < 20 ? 20 : mark_hz - half);
            int32_t xb = EQ_X(mark_hz + half > 20000 ? 20000 : mark_hz + half);
            if (xb - xa > 2) {
                cv_rect(xa, mark_y, xb - xa, 1, T_DIM);
                cv_rect(xa, mark_y - 2, 1, 5, T_DIM);
                cv_rect(xb - 1, mark_y - 2, 1, 5, T_DIM);
            }
        }
        if (mode > 0) {
            cv_rect(mark_x, gtop, 1, gbot - gtop, T_LINE);          /* the point's frequency */
            cv_rect(mark_x - 3, mark_y - 3, 7, 7, T_ACCENT);        /* and where the curve is on it */
            cv_rect(mark_x - 1, mark_y - 1, 3, 3, T_TEXT);
        }
        /* its reading, in a chip at the plot's top right: "30 Hz +9.6 dB" */
        b[0] = 0;
        eq_hz_txt(b, mark_hz, (int32_t)sizeof b);
        str_cpy(b + str_len(b), " Hz ", (uint32_t)sizeof b - str_len(b));
        eq_db_txt(b + str_len(b), mark_db, (int32_t)sizeof b - str_len(b));
        {
            int32_t tw = (int32_t)str_len(b) * 7 + 6;
            /* the chip goes to the side of the plot the band is not on, so it never sits on the curve or on the
             * shelf's plateau: a lift reads at the bottom, a cut at the top */
            int32_t chip_y = mark_y < mid_y ? gbot - 11 : gtop - 1;
            cv_rect(gx0 + gw - tw, chip_y, tw, 11, T_RAISE);
            cv_text_on(gx0 + gw - tw + 3, chip_y + 1, &AF_S, b, mode > 0 ? T_TEXT : T_DIM, T_RAISE);
        }
        #undef EQ_X
    }
}

/* MASTER: Modular 4-Slot Master FX Rack + Globals Meters (QUANTUM GAMMAWAVE // QG-1) */
static void tech_master(void)
{
    int32_t clip = song.g[G_CLIP], g = 1024 + clip * 31, i, py = 0, x0 = 22, y0 = TK_Y0, s = 100, cy = y0 + s / 2;
    int32_t en = bg_energy(), punch = song.g[G_PUNCH], duck = song.g[G_DUCK];
    if (ux.chamfer && !ux.style) {
        static const char *const RACK_N[4] = {"CRUSH", "DELAY", "REVERB", "CHORUS"};
        int32_t rv[4][3];
        int32_t r, k;
        rv[0][0] = clamp(TSEL->p[P_DIST] * 2, 0, 256);
        rv[0][1] = clamp(TSEL->p[P_DTONE] * 2, 0, 256);
        rv[0][2] = clamp(clip * 2 + 64, 0, 256);
        rv[1][0] = clamp(song.g[G_DTIME] * 2, 0, 256);
        rv[1][1] = clamp(song.g[G_DFDBK] * 2, 0, 256);
        rv[1][2] = clamp(song.g[G_DMIX] * 2, 0, 256);
        rv[2][0] = clamp(song.g[G_RSIZE] * 2, 0, 256);
        rv[2][1] = clamp(song.g[G_RDAMP] * 2, 0, 256);
        rv[2][2] = clamp(song.g[G_RMOD] * 2, 0, 256);
        rv[3][0] = clamp(song.g[G_CRATE] * 2, 0, 256);
        rv[3][1] = clamp(song.g[G_RWIDE] * 2, 0, 256);
        rv[3][2] = clamp((song.g[G_CSTER] + 64) * 2, 0, 256);

        /* Left Bay (x = 8..176): 4 Stacked Modular FX Rack Slots */
        for (r = 0; r < 4; r++) {
            int32_t ry = y0 - 2 + r * 26, rh = 24;
            int active = (rv[r][2] > 8 || r == 0);
            gw_oct_panel(8, ry, 168, rh, 3, T_BG, active ? T_LINE : T_RAISE, active, (r & 1) ? T_ACCENT : T_THEME);
            /* Active Slot Orange + Green LEDs */
            cv_rect(14, ry + 7, 4, 4, active ? bg_col(184) : T_DIM);
            cv_rect(14, ry + 13, 4, 4, active ? T_ACCENT : T_RAISE);
            cv_text_on(22, ry + 5, &AF_S, RACK_N[r], active ? T_TEXT : T_DIM, T_BG);
            /* 3 Mini Bakelite Rotary Knobs per FX Rack Slot */
            for (k = 0; k < 3; k++) {
                int32_t kx = 104 + k * 28;
                gw_mini_bakelite_knob(kx, ry + 12, 7, rv[r][k], (k == 2) ? T_THEME : T_ACCENT);
            }
        }

        /* Right Bay (x = 180..232): GLOBALS Master Knob + L / R / PUMP meters. The first two are the
         * master's real peaks (fx_vu[6..7], the same envelope HOME reads), not a shape drawn from the
         * envelope generator: a meter that moves with the knobs but not with the audio is a lie, and the
         * owner has been told one of those before. PUMP is the duck gain -- zero when DUCK is off, because
         * then nothing is pumping. */
        gw_oct_panel(180, y0 - 2, 52, 102, 4, T_BG, T_LINE, 1, T_ACCENT);
        gw_mini_bakelite_knob(206, y0 + 16, 9, clamp(180 + clip, 0, 256), T_THEME);
        cv_rect(185, y0 + 32, 42, 1, T_LINE);
        gw_plasma_bar(188, y0 + 37, 8, 58, 9, clamp(fx_vu[6] * 256 / 32767, 0, 256), 64);
        gw_plasma_bar(201, y0 + 37, 8, 58, 9, clamp(fx_vu[7] * 256 / 32767, 0, 256), 96);
        gw_plasma_bar(214, y0 + 37, 10, 58, 9, duck ? clamp(duck_g1 * 256 / 32767, 0, 256) : 0, 140);
        return;
    }
    tk_axes(x0, y0, x0 + s, y0 + s);
    cv_rect(x0, cy, s, 1, T_LINE);
    for (i = 0; i <= s; i++) {
        int32_t x = (i - s / 2) * 32767 / (s / 2), y = cy - softclip(x * g >> 10) * (s / 2) / 32767;
        if (i)
            cv_line_t(x0 + i - 1, py, x0 + i, y, heat_y(y < cy ? y : 2 * cy - y, y0, cy, T_THEME), 2);
        py = y;
    }
    if (en > 2)
        tk_dot(x0 + s / 2 + en * (s / 2) / 256, cy - softclip(en * 128 * g >> 10) * (s / 2) / 32767);
    {   /* PUNCH: a drum hit's envelope (~190 ms) times punch_gain (eng_drum.c): the push, the fuller body */
        int32_t px0 = 136, pw = 60, b = y0 + s, py2 = b - 45;
        tk_axes(px0, y0 + 20, px0 + pw, b);
        for (i = 0; i <= pw; i++) {
            int32_t e = 45 * (pw - i) / pw, y = b - (e * punch_gain((uint32_t)i * 255u / (uint32_t)pw, punch) >> 15);
            if (i)
                cv_line_t(px0 + i - 1, py2, px0 + i, y, heat_y(y, y0 + 20, b, T_THEME), 2);
            py2 = y;
        }
    }
    {   /* DUCK: the gain the other parts get now (full: no duck) */
        int32_t mx = 210, mh = s, gh = duck_g1 * mh / 32767;
        cv_rect(mx, y0, 10, mh, T_RAISE);
        cv_rect(mx, y0 + mh - gh, 10, gh, duck ? heat_col(60 + gh * 180 / mh) : T_DIM);
    }
}

/* ENV DEST / LFO DEST: the source now into four bipolar columns, one under each card */
static void tech_dest(const track_t *t, int lfo)
{
    int32_t src = 0, c, mid = (TK_Y0 + TK_Y1) / 2, half = 44;
    uint32_t i;
    if (lfo)
        src = mulq15(t->lfo_val, t->lfo_fade);
    else
        for (i = 0; i < NVOICE; i++)
            if (V(t, i).stage && V(t, i).stage < 4u && (V(t, i).env >> 9) > src)
                src = V(t, i).env >> 9;                 /* Q15 */
    for (c = 0; c < 4; c++) {
        int32_t x = CARD_X(c) + 22, amt = tk_amt((uint32_t)c), bh = amt * half / 500, live = bh * src / 32767;
        cv_rect(x - 6, mid, 25, 1, T_RAISE);
        cv_rect(x + 6, mid - half, 1, 2 * half, T_LINE);
        if (bh > 0) cv_rect(x, mid - bh, 13, bh, ux_mix(T_SURF, T_THEME, 35));
        else if (bh < 0) cv_rect(x, mid, 13, -bh, ux_mix(T_SURF, T_THEME, 35));
        if (live > 0) cv_rect(x, mid - live, 13, live, heat_col(80 + live * 170 / half));
        else if (live < 0) cv_rect(x, mid, 13, -live, heat_col(80 - live * 170 / half));
    }
    {   /* the source: its level now as a bar at the left edge */
        int32_t sh = (src < 0 ? -src : src) * half / 32767;
        cv_rect(TK_X0, src >= 0 ? mid - sh : mid, 3, sh ? sh : 1, T_TEXT);
    }
}

/* VOICE: the track's voices now; the glide between two notes */
static void tech_voice(const track_t *t)
{
    uint32_t i, nv = trk_nvoice(t) ? trk_nvoice(t) : NVOICE;
    int32_t sw = PANEL_W / (int32_t)NVOICE, glide = t->p[P_GLIDE], py = 0, x;
    for (i = 0; i < NVOICE; i++) {
        int32_t x0 = TK_X0 + (int32_t)i * sw + 2, h = 40, lv = 0;
        if (V(t, i).stage && V(t, i).stage < 4u)
            lv = (int32_t)(((int64_t)V(t, i).env * h) >> 24);
        cv_rrect(x0, TK_Y0, sw - 4, h, 2, i < nv ? T_RAISE : T_LINE, T_SURF);
        if (lv > 0)
            cv_rect(x0 + 1, TK_Y0 + h - lv, sw - 6, lv, heat_col(60 + lv * 190 / h));
    }
    for (x = 0; x <= PANEL_W - 20; x++) {               /* the glide: low note to high, as slow as GLIDE */
        int32_t tm = glide ? x * 127 * 6 / (glide * 4 + 1) : 1000, k = tm > 255 ? 255 : tm;
        int32_t y = TK_Y1 - 4 - 40 * (x < 30 ? 0 : (k * (255 - (255 - k) * (255 - k) / 255)) / 255) / 255;
        if (x < 30) y = TK_Y1 - 4;
        if (x)
            cv_line_t(TK_X0 + 10 + x - 1, py, TK_X0 + 10 + x, y, heat_y(y, TK_Y1 - 44, TK_Y1 - 4, T_THEME), 2);
        py = y;
    }
}

/* GLOBAL (JIANT 0.5): the bar as 16 columns, one a sixteenth, in four beats; each as hot as the tempo (40 BPM cold ..
 * 240 white), the downbeats taller, the off-beats late by SWING, the one playing now white; under them CLK's three
 * sources (the one followed lit, its name over it) and TUNE as a bar from the middle (flat left, sharp right) */
static void tech_global(void)
{
    int32_t sw = song.g[G_SWING], bpm = song.g[G_BPM], tune = song.g[G_TUNE], i, x0 = TK_X0 + 6, w = PANEL_W - 12;
    int32_t step = w / 16, base = 60, hot = 40 + (bpm - 40) * 190 / 200, now = -1, cx, k;
    static const char *const CK[3] = {"INT", "USB", "TRS"};
    if (song.playing) {
        uint32_t bar = div_samples(2) * 16u;
        now = bar ? (int32_t)((ms_clock % bar) / div_samples(2)) : -1;
    }
    cv_rect(x0, base + 1, w, 1, T_RAISE);
    for (i = 0; i < 16; i++) {
        int32_t x = x0 + i * step + ((i & 1) ? sw * step / 200 : 0), hh = i % 4 == 0 ? 46 : i % 2 == 0 ? 30 : 20;
        uint16_t c = i == now ? T_TEXT : heat_col(hot - (i % 4 ? 30 : 0) + ((i & 1) ? sw / 2 : 0));
        cv_rect(x + 2, base - hh, step - 5, hh, c);
    }
    for (k = 0; k < 3; k++) {                           /* CLK: the sources */
        int32_t x = x0 + k * 40, on = song.g[G_CLOCK] == k;
        cv_rrect(x, 74, 34, 6, 3, on ? heat_col(200) : T_RAISE, T_SURF);
        cv_text_in(x, 82, 34, &AF_S, CK[k], on ? T_TEXT : T_DIM, T_SURF);
    }
    cx = x0 + 120 + (w - 120) / 2;                      /* TUNE: from the middle */
    cv_rect(x0 + 124, 76, w - 124, 2, T_RAISE);
    cv_rect(cx, 70, 1, 14, T_MID);
    if (tune)
        cv_rect(tune > 0 ? cx + 1 : cx + tune * ((w - 124) / 2) / 50, 74, (tune > 0 ? tune : -tune) * ((w - 124) / 2) / 50, 6,
                heat_col(60 + (tune > 0 ? tune : -tune) * 180 / 50));
}

/* (JIANT 0.6.9) AUTOPAN: the stereo field across (L left, R right, the track's PAN the centre line), the swing down
 * the panel as time (two cycles, the newest at the top), a dot where the pan is now (fx.c ap_ph) */
static int32_t tk_apwave(uint32_t w, uint32_t ph, uint32_t cyc)
{
    switch (w) {
    case 1: return ph < 0x80000000u ? (int32_t)(ph >> 15) - 32768 : 98303 - (int32_t)(ph >> 15);
    case 2: return (int32_t)(ph >> 16) - 32768;
    case 3: return ph < 0x80000000u ? 32767 : -32767;
    case 4: return (int32_t)((cyc * 2654435761u) >> 16) - 32768;   /* (S&H: a place a cycle) */
    default: return bg_sin(ph >> 16);                   /* (0.6.12: bg_sin takes 16 bits of the turn) */
    }
}
/* (0.6.12) as LFO's chart: two cycles of the pan's wave across the panel, L up, R down, centred on PAN, DEPTH its
 * swing; the playhead a quarter in (the past to its left, dimmed), a dot on where the pan is now */
static void tech_autopan(const track_t *t)
{
    int32_t x0 = TK_X0, w = PANEL_W, dep = clamp(t->p[P_APDEP], 0, 127), xp = w / 4, x, py = 0;
    int32_t a = (TK_Y1 - TK_Y0) / 2 - 6, cy = (TK_Y0 + TK_Y1) / 2 + clamp(t->p[P_PAN], -64, 63) * a / 128;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, wv = (uint32_t)clamp(t->p[P_APWAVE], 0, 4);
    uint64_t now = ap_ph[ti] + 0x400000000ull, span = 0x200000000ull / (uint64_t)w;   /* (two cycles a panel) */
    cv_rect(x0, (TK_Y0 + TK_Y1) / 2, w, 1, T_RAISE);     /* the centre */
    cv_text(x0 + 4, TK_Y0, &AF_S, "L", T_DIM);
    cv_text(x0 + 4, TK_Y1 - 14, &AF_S, "R", T_DIM);
    for (x = 0; x < w; x++) {
        uint64_t q = now + (uint64_t)(int64_t)(x - xp) * span;
        int32_t y = clamp(cy - (tk_apwave(wv, (uint32_t)q, (uint32_t)(q >> 32)) * dep / 127) * a / 32768, TK_Y0, TK_Y1);
        if (x)
            cv_line_t(x0 + x - 1, py, x0 + x, y,
                      x < xp ? ux_mix(T_SURF, heat_y(y, cy - a, cy + a, T_THEME), 45) : heat_y(y, cy - a, cy + a, T_THEME), 2);
        py = y;
    }
    cv_rect(x0 + xp, TK_Y0, 1, TK_Y1 - TK_Y0, T_MID);    /* the playhead */
    {
        int32_t y = clamp(cy - (tk_apwave(wv, (uint32_t)now, (uint32_t)(now >> 32)) * dep / 127) * a / 32768, TK_Y0, TK_Y1);
        cv_rrect(x0 + xp - 3, y - 3, 7, 7, 3, T_TEXT, T_SURF);
    }
}

/* RETRIG (LOFI BYTE / FLOAT): T.CLK, the formula's clock, over four notes (their ticks under it).
 * NOTE: the formula restarts at every note, so each note is a ramp from 0 and the sound repeats.
 * FREE: the track's clock carries the formula on and the notes join it where it stands -- the ramp never goes
 * back, and each tick sits on the point of it that note starts from. That is what the DSP does (eng_lofi.c
 * lofi_note_on: t = fm1_ms * 8 in FREE, 0 in NOTE), so the drawing is a reading of it, not an illustration. */
static void tech_retrig(const track_t *t)
{
    int32_t x0 = TK_X0 + 10, w = PANEL_W - 20, base = TK_Y1 - 12, top = TK_Y0 + 10;
    int32_t freerun = t->p[P_LRTRG] != 0, i, py = base, seg = w / 4;

    for (i = 0; i < 4; i++)                              /* the notes */
        cv_rect(x0 + i * seg, base + 3, 3, 6, T_ACCENT);
    for (i = 0; i <= w; i++) {
        int32_t ph = freerun ? i * 1000 / w : (i % seg) * 1000 / seg;
        int32_t y = base - ph * (base - top) / 1000;
        if (i && !(!freerun && i % seg == 0))
            cv_line_t(x0 + i - 1, py, x0 + i, y, heat_y(y, base, top, T_THEME), 2);
        else if (i)
            cv_rect(x0 + i, y, 1, py - y + 1, T_RAISE);  /* (back to the top) */
        py = y;
    }
    for (i = 1; i < 4; i++) {                            /* where the 2nd..4th notes start on that ramp */
        int32_t x = x0 + i * seg;
        int32_t ph = freerun ? i * 1000 / 4 : 0;
        int32_t y = base - ph * (base - top) / 1000;
        cv_rrect(x - 2, (freerun ? y : base) - 2, 5, 5, 2, freerun ? T_TEXT : T_DIM, T_BG);
    }
    cv_text(x0, TK_Y0, &AF_S, freerun ? "RUNS ON: THE NOTE JOINS IT" : "RESTARTS AT EVERY NOTE", T_DIM);
}

/* the page's technical graph, if it has one (1) */
static int graph_tech(const track_t *t, const page_t *pg)
{
    if (pg->scope != SC_GLOBAL)
        switch (pg->id[0]) {
        case P_FTYPE:
            if (ux.chamfer && !ux.style) {
                gw_draw_forge_bay(t, 8, 2, 224, 82, bg_energy());
                gw_draw_wraith_ribbon(t, 8, 86, 224, 32);
                return 1;
            }
            return 0;
        case P_DIST: tech_dist(t); return 1;
        case P_APDEP: tech_autopan(t); return 1;
        case P_LRTRG: if (pg->id[1] != 0xFFu) return 0; tech_retrig(t); return 1;   /* (not DIGITAL's OP1 ENV) */
        case P_ED_FLT: tech_dest(t, 0); return 1;
        case P_LD_PIT: tech_dest(t, 1); return 1;
        case P_VOICE: tech_voice(t); return 1;
        case P_MSDIV: cv_oy = GOY; graph_mseq(t, T_THEME); return 1;   /* (MSEQ's, on the 100 px scale) */
        default: return 0;
        }
    switch (pg->id[0]) {
    case G_DTIME: tech_dly(); return 1;
    case G_WIDTH: tech_dly2(); return 1;
    case G_RTYPE: tech_rev(); return 1;
    case G_RMOD: tech_rev2(); return 1;
    case G_CRATE: tech_chorus(); return 1;
    case G_CLIP: tech_master(); return 1;
    case G_EQMODE: tech_master_eq(); return 1;
    case G_BPM: tech_global(); return 1;
    default: return 0;
    }
}
