/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Guillel51 / Quantum GammaWave
 * (the file was created in this repository: "ShadowTune / GammaWave // Wraith Custom Firmware" named a holder
 *  that no commit, no LICENSES/ text and no contributor records; the Wraith ribbon and Wraith bar are this
 *  firmware's own names, see engines.c and the manual.)
 * The GammaWave / Wraith visualizers the 240x240 display carries:
 *   - THE FORGE (Live Filter Frequency Response Curve + Thermal Spectrum + Dual Scope + Width/Phase Meters)
 *   - THE HEARTH (Solar Furnace Core + Radial Flare Corona + Spark Particles + Orbiting Satellite Moon)
 *   - WRAITH RIBBONS (6-Band Character Ribbon FLU/WHI/VOX/CHR/BEL/SCR & 5-Band Vowel Ribbon OO/OH/AH/EH/EE)
 *   - 6-Track Chamfered Ingot Deck (HOME) & 8-Pad Chamfered Drum Forge (DRUM-X) */

/* neutral-safe thermal color: GREY and MONO stay strictly on the RGB565 gray axis */
static uint16_t bg_col(int32_t v)
{
    uint16_t c = heat_col(clamp(v, 0, 255));
    /* (3.0) the grey axis is decided by the palette being DRAWN (ux.mono), not only by the stored
     * settings.palette: a screen rendered as GREY while the setting still named another palette drew
     * tinted pixels into a gray page (54 of them, one at HOME's star). MONO keeps its black/white. */
    return ux.mono || ux.bw ? ux_gray(ux_luma(c) >> 3) : c;
}
static int32_t bg_sin(uint32_t ang) { return sine_i(ang << 16); }   /* Q15 */
/* (3.5) a tone one step deeper, in its own palette's terms: it can only move a channel down, so MENU > PALETTE
 * decides the colour and the visualizer only decides how far the field recedes. Neutral-safe (a gray stays a
 * gray, MONO's black stays black), no table, three multiplies. */
static uint16_t bg_darken(uint16_t c, int32_t pct)
{
    uint16_t r = (uint16_t)((c >> 11) & 31u), g = (uint16_t)((c >> 5) & 63u), b = (uint16_t)(c & 31u);
    if (pct < 0) pct = 0;
    if (pct > 90) pct = 90;
    if (r == b && g == (uint16_t)(2u * r))          /* on the gray axis: stay on it to the pixel */
        return ux_gray((uint32_t)(r * (100 - pct) / 100));
    r = (uint16_t)(r * (100 - pct) / 100);
    g = (uint16_t)(g * (100 - pct) / 100);
    b = (uint16_t)(b * (100 - pct) / 100);
    return (uint16_t)(r << 11 | g << 5 | b);
}
/* (3.5) the same for a blend of two tones: a GREY or MONO page may not gain a tinted pixel from a background
 * body, so a blend of two gray-axis tones is snapped back onto the axis */
static uint16_t bg_tone_mix(uint16_t a, uint16_t b, int32_t pct)
{
    uint16_t c = ux_mix(a, b, pct);
    if ((a >> 11) == (a & 31u) && (b >> 11) == (b & 31u))
        return ux_gray(c >> 11);
    return c;
}

/* (1.8.6) Note-reactive sparks for the visualizers. They are driven by the voices the
 * audio path is actually playing (gate, note, velocity), not by a random animation:
 * a note-on emits a small fan, a new note on a reused voice releases the old one, and
 * note-off emits a short outward burst. Bounded storage, no allocation, UI-side only.
 * The host's deterministic pixel tests set UI_NO_DRIFT, which also freezes these. */
/* (2.2) the pool's owner, outside UI_NO_DRIFT: the constant and the setter exist in both builds */
enum { VZ_OWN_NONE = 0, VZ_OWN_HOME, VZ_OWN_VIZ };
/* (3.5) the dust field's own ceiling: whatever a screen asks for, gw_embers never draws more than this (32
grains of 1..2 px, so the whole field is at most 64 pixels of a frame and no page can be saturated by it) */
#define GW_EMBER_MAX 32u
static uint8_t vz_np_owner;
static uint32_t vz_np_drawn = 0xFFFFFFFFu;   /* the frame the pool last advanced in (two passes: once) */
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 0
#endif
#if !UI_NO_DRIFT
#define VZ_NP_MAX 28
/* (2.2) WHO a spark belongs to. The pool is one fixed array that every screen drawing sparks shares.
 * Without an owner, a spark born on HOME was painted by whichever screen next drew the pool -- which is
 * how they turned up over the EDIT ribbon's bands (CHR among them) and over the six cells under it. A
 * spark is now drawn, and only advanced, by the screen that owns it, inside the rect that screen hands
 * in; a spark that leaves that rect is dropped instead of being carried to another screen. */
typedef struct {
    int16_t x, y, vx, vy;
    uint8_t life, max, size, own;
    uint16_t col;
} vz_np_t;
typedef struct {
    uint8_t seen, note, vel;
    int16_t x, y;
} vz_nv_t;
static vz_np_t vz_np[VZ_NP_MAX];
static uint8_t vz_np_next;
static vz_nv_t vz_nv[NTRK][NVOICE];
static uint32_t vz_nv_frame = 0xFFFFFFFFu;

static void vz_np_add(int32_t x, int32_t y, int32_t vx, int32_t vy, uint16_t col, uint8_t life, uint8_t size)
{
    vz_np_t *p = &vz_np[vz_np_next++ % VZ_NP_MAX];
    p->x = (int16_t)clamp(x, -32, 271);
    p->y = (int16_t)clamp(y, -32, 271);
    p->vx = (int16_t)clamp(vx, -400, 400);
    p->vy = (int16_t)clamp(vy, -400, 400);
    p->life = p->max = life;
    p->size = size;
    p->col = col;
    p->own = vz_np_owner;                            /* (2.2) the screen that gave birth to it */
}
static void vz_np_layer(uint8_t l) { vz_np_owner = l; }
static void vz_np_scan(int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    uint32_t t, i;
    if (vz_nv_frame == ui.frame)
        return;
    vz_nv_frame = ui.frame;
    if (ui_prefs & PREF_ANIM_OFF)
        return;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &V(&trk[t], i);
            vz_nv_t *n = &vz_nv[t][i];
            int on = v->active && v->gate && v->note < 128u;
            int32_t nx = x0 + 6 + (int32_t)v->note * (w - 12) / 127;
            int32_t ny = y0 + h - 9 - (int32_t)((t * 7u + i * 5u) % 18u);
            uint16_t col = bg_col(96 + (int32_t)(v->note % 12u) * 11 + (int32_t)t * 17);
            if (on) {
                if (!n->seen || n->note != v->note) {
                    int k;
                    if (n->seen) {                   /* a reused voice: release the note it held */
                        for (k = 0; k < 3; k++)
                            vz_np_add(n->x, n->y, (k - 1) * 2, -1 - (k & 1), col, 11, 1);
                    }
                    for (k = 0; k < 3; k++)
                        vz_np_add(nx + (k - 1) * 2, ny, (k - 1) * 2, -1 - (k & 1), col,
                                  8 + (uint8_t)((v->vel ? v->vel : 100u) / 32u), k == 1 ? 2 : 1);
                }
                n->seen = 1;
                n->note = v->note;
                n->vel = v->vel;
                n->x = (int16_t)nx;
                n->y = (int16_t)ny;
            } else if (n->seen) {                    /* note-off: a short burst where it was */
                int k;                    for (k = 0; k < 4; k++)
                        vz_np_add(n->x + (k & 1) * 3 - 1, n->y + ((k >> 1) & 1) * 3 - 1,
                                  (k & 1 ? 2 : -2), (k >> 1 ? 2 : -2), col, 8, 1);
                n->seen = 0;
            }
        }
    }
}
static __attribute__((noinline)) void gw_note_particles(int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    uint32_t i;
    int32_t l, top = y0, bot = y0 + h - 1, rgt = x0 + w - 1;
    int adv = ui.frame != vz_np_drawn;               /* (2.2) the full-screen visualizer draws two canvas
                                                      * bands a frame: the pool advances once, both bands draw */
    uint8_t own = vz_np_owner;
    if (adv)
        vz_np_drawn = ui.frame;
    vz_np_scan(x0, y0, w, h);
    for (i = 0; i < VZ_NP_MAX; i++) {
        vz_np_t *p = &vz_np[i];
        int32_t ox, oy, ex, ey;
        if (!p->life || p->own != own)               /* (2.2) another screen's spark: not ours to move or draw */
            continue;
        ox = p->x;
        oy = p->y;
        if (adv) {
            p->x = (int16_t)(p->x + p->vx / 2);
            p->y = (int16_t)(p->y + p->vy / 2);
            p->vy = (int16_t)clamp(p->vy + 1, -6, 6);
            p->vx = (int16_t)clamp(p->vx * 15 / 16, -6, 6);
        }
        /* (3.1) a spark is the bay's, strictly: it is dropped the frame it touches the edge, and neither its
         * trail nor its 2 px body is allowed to paint one pixel outside the rect it was handed. The old test
         * allowed 2 px of overshoot and drew the ghost line to a point that could already be outside. */
        if (p->x < x0 || p->x > rgt || p->y < top || p->y > bot) {
            p->life = 0;
            continue;
        }
        l = 42 + (int32_t)p->life * 100 / p->max;
        ex = p->x + p->size > rgt + 1 ? rgt + 1 : p->x + p->size;
        ey = p->y + p->size > bot + 1 ? bot + 1 : p->y + p->size;
        if (ox >= x0 && ox <= rgt && oy >= top && oy <= bot && (ox != p->x || oy != p->y))
            cv_line(ox, oy, p->x, p->y, ux_mix(T_BG, p->col, l / 2));
        if (ex > p->x && ey > p->y)
            cv_rect(p->x, p->y, ex - p->x, ey - p->y, ux_mix(T_BG, p->col, l));
        if (adv)
            p->life--;
    }
}
#else
/* the host's deterministic pixel builds (UI_NO_DRIFT) freeze the sparks: the pool is not there, and
 * nothing that would add to it must fail to compile because of that */
static void vz_np_add(int32_t x, int32_t y, int32_t vx, int32_t vy, uint16_t col, uint8_t life, uint8_t size)
{ (void)x; (void)y; (void)vx; (void)vy; (void)col; (void)life; (void)size; }
static void vz_np_layer(uint8_t l) { (void)l; }
static __attribute__((noinline)) void gw_note_particles(int32_t x0, int32_t y0, int32_t w, int32_t h)
{ (void)x0; (void)y0; (void)w; (void)h; }
#endif

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
static __attribute__((noinline)) void gw_oct_panel(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t fill, uint16_t brd,
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

/* Smooth anti-aliased filled circular disc for THE HEARTH solar core, knobs & satellite moons */
static __attribute__((noinline)) void gw_disc(int32_t cx, int32_t cy, int32_t r, uint16_t col)
{
    int32_t dy, dx, r2 = r * r, r_inner2 = (r > 1) ? (r - 1) * (r - 1) : 0;
    for (dy = -r; dy <= r; dy++) {
        int32_t y = cy + dy, span = 0, inner_span = 0;
        for (dx = r; dx >= 0; dx--)
            if (dx * dx + dy * dy <= r2) { span = dx; break; }
        for (dx = span; dx >= 0; dx--)
            if (dx * dx + dy * dy <= r_inner2) { inner_span = dx; break; }

        if (r <= 2 || inner_span == span) {
            cv_rect(cx - span, y, 2 * span + 1, 1, col);
        } else {
            if (inner_span >= 0)
                cv_rect(cx - inner_span, y, 2 * inner_span + 1, 1, col);

            /* Soft antialiased edge pixels on perimeter */
            uint16_t edge_col = ux.mono ? col : ux_mix(T_SURF, col, 55);
            for (dx = (inner_span >= 0 ? inner_span + 1 : 0); dx <= span; dx++) {
                if (dx == 0) {
                    cv_pset(cx, y, edge_col);
                } else {
                    cv_pset(cx - dx, y, edge_col);
                    cv_pset(cx + dx, y, edge_col);
                }
            }
        }
    }
}

/* Smooth circular/elliptical ring (24 segments, optional dotted mode for satellite orbit track) */
static __attribute__((noinline)) void gw_orbit_ring(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int dotted)
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
static __attribute__((noinline)) void gw_plasma_bar(int32_t x, int32_t y0, int32_t w, int32_t h, int32_t segs, int32_t val256, int32_t heat0)
{
    int32_t k, lit = (val256 * segs + 128) >> 8, step = h / segs, sh = step > 2 ? step - 1 : 1;
    for (k = 0; k < segs; k++) {
        int32_t sy = y0 + h - (k + 1) * step;
        uint16_t c = k < lit ? bg_col(heat0 + k * (244 - heat0) / (segs > 1 ? segs - 1 : 1)) : T_RAISE;
        cv_rect(x, sy, w, sh, c);
    }
}

/* Segmented horizontal thermal bar */
static __attribute__((noinline)) void gw_hbar(int32_t x0, int32_t y, int32_t w, int32_t h, int32_t segs, int32_t val256, int32_t heat0)
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
    /* HOME's four mini-knobs are the four HOME encoders, not the last page's columns. Using page_desc()
     * here made the indicators follow a stale page and desynchronise from KNOB 1..4. */
    const param_desc_t *d = ui.home ? home_param(c, &vp) : page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    if (lab)
        *lab = d && d->label ? d->label : "";
    return r < 0 ? -1 : r * 256 / 1000;
}

/* Live triggered waveform trace across a horizontal span */
static __attribute__((noinline)) void gw_live_trace(int32_t x0, int32_t w, int32_t cy, int32_t amp, int32_t heat_base)
{
    uint32_t sw = scope_w, i, trig = 0;
    int32_t x, py, pk = 2000;
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
    /* (FASE 3) one 2 px pass, after the floor and the bands, in the palette's own ink when its ramp is neutral
     * (GREY, MONO) and on the heat ramp otherwise: the trace is never painted over and never the same grey as
     * the bars it crosses. */
    py = cy;
    for (x = 0; x < w; x++) {
        uint32_t idx = (sw - 256u + trig + (uint32_t)x * 128u / (uint32_t)(w ? w : 1)) & (SCOPE_N - 1u);
        int32_t s = (scope_buf[idx] + 2 * scope_buf[(idx + 1u) & (SCOPE_N - 1u)] +
                     scope_buf[(idx + 2u) & (SCOPE_N - 1u)]) / 4;
        int32_t y = cy - s * amp / pk;
        int32_t dy = y > cy ? y - cy : cy - y;
        y = clamp(y, cy - amp, cy + amp);
        if (x)                                          /* (3.5) two pixels: a trace thinner than the curve reads
                                                         * as a rendering seam, not as a layer */
            /* (1.3.5) the oscilloscope is now a see-through line: 84 % of its own ink over what it crosses, so
             * the bands behind it keep their shape under the trace and the signal still reads as the top layer */
            cv_line_a(x0 + x - 1, py, x0 + x, y,
                      ux.mono ? T_TEXT : bg_col(heat_base + dy * 140 / (amp ? amp : 1)), CV_A_SIGNAL, 2);
        py = y;
    }
}

/* (QG-1 v1.5) MENU > VISUALIZER QUALITY (ui.c ui_viz_q): what the visualizer spends, never the audio. Nothing here
 * is on the render path's budget -- it only changes how much of the screen is drawn and how often the motion advances.
 * HIGH (0, default): every spark and flare, the curves at their own step, the motion every frame (as before).
 * BALANCED (1): half the sparks, a third fewer flares, the curves every 2nd column, the motion every other frame.
 * LOW (2): no sparks, eight flares, the curves every 3rd column, the motion every third frame. */
static uint32_t vz_step(void) { return ui_viz_q >= 2u ? 3u : ui_viz_q == 1u ? 2u : 1u; }
static uint32_t vz_flares(void) { return ui_viz_q >= 2u ? 8u : ui_viz_q == 1u ? 12u : 20u; }
/* the motion's frame snapped to vz_step(): it advances in the same size steps, once per step's frames, so a lower
 * quality updates the animation less often at the same speed (the screen still repaints every frame, so nothing
 * is left blank) */
static uint32_t vz_qframe(void) { uint32_t s = vz_step(); return ui.frame / s * s; }

/* ============================================================================
 * 1. HOME'S SYSTEM: a star, three orbits, three planets (3.0)
 *    Arranged as a system seen slightly from above: planets on their own ellipses, their own
 *    speeds and their own sizes, the ones behind the star dimmed and drawn before it, the ones
 *    in front drawn after it.
 *
 *    (3.0) What changed, and why the old one read as broken:
 *    - MOTION. The old angle was an accumulator advanced by `frames * (200 + energy * 3)`, so the
 *      orbit sped up to ~5x with the mix and depended on how often the panel happened to redraw
 *      (a drop in frames, a LOAD LITE skip, a chord: a visible jump). The angle is now a function
 *      of the millisecond clock (fm1_ms * the orbit's own rate), so it is the same curve at any
 *      frame rate, never restarts, and never jumps; the music no longer bends time.
 *    - SIZE. Planets were 2 / 3 / 4 px discs in an 88 x 52 px bay (91 x 55 since 3.6), with the star at most 6 px.
 *      They are 3 / 4 / 5 px spheres now (2.25x the area of the old ones), the star 5..7 px, on
 *      orbits of 15 / 25 / 34 px instead of 11 / 20 / 29, which still fit the bay with 2 px of
 *      margin at the widest point.
 *    - DETAIL. gw_sphere() shades a body row by row: a dark limb and a lit crescent facing the
 *      star (so the phase turns as the planet goes round), a 1 px terminator between them, an
 *      atmosphere ring, a specular pixel and two surface bands. gw_disc() (the knobs, the star,
 *      the page's other discs) is untouched.
 *    - MUSIC. Each planet answers the notes of its own pair of tracks (1/4, 2/5, 3/6): a note-on
 *      adds to that planet's excitation (velocity-weighted, additive, so a chord or a fast run
 *      stacks instead of restarting) and it decays smoothly, driving the sphere's radius, its
 *      heat, an afterglow ring, a bright arc trailing on its orbit and two sparks in the pool the
 *      visualizers already share. Muted or sleeping tracks are skipped, so the picture follows
 *      what is really sounding.
 *
 *    No allocation, no new primitive beyond gw_sphere, and the per-frame cost stays of the same
 *    order as before: 3 rings + 3 spheres + 5 star discs + 4 sines. The three orbit rings are
 *    drawn from a 24-entry unit circle (gw_ring_tab) instead of 48 sine_i calls a ring.
 * ============================================================================ */
#define NPL 5                             /* (3.2) four planets: the third is the ringed giant, the fourth a
                                           * small rocky one inside, so the system has a moon as well as a sun;
                                           * (2.1) a fifth, a frozen world on the outermost path */
static int16_t pl_ex[NPL];                /* each planet's excitation, 0..256: a note-on adds, it decays */
static int16_t pl_flash[NPL];             /* (3.3) the note's own light, 0..256: set by a note-on, decays
                                           * faster than the excitation, so the strike reads as a strike and
                                           * the afterglow as an afterglow */
#if !UI_NO_DRIFT
static uint8_t pl_seen[NTRK][NVOICE];     /* the note each voice held on the last scan (+1; 0: nothing) */
#endif

/* The 24 unit-circle points an orbit ring is drawn through (16-bit turn, 24 segments): cos/sin of
 * (k + 1) * 65536 / 24, Q15, ending on the start point. It replaces two sine_i() calls per segment
 * per ring -- 144 interpolated lookups a frame -- with 48 table loads and the same geometry. */
static const int16_t GW_RING[24][2] = {
    {31650, 8481}, {28377, 16384}, {23170, 23170}, {16384, 28377}, {8481, 31650}, {0, 32767},
    {-8481, 31650}, {-16384, 28377}, {-23170, 23170}, {-28377, 16384}, {-31650, 8481}, {-32767, 0},
    {-31650, -8481}, {-28377, -16384}, {-23170, -23170}, {-16384, -28377}, {-8481, -31650}, {0, -32767},
    {8481, -31650}, {16384, -28377}, {23170, -23170}, {28377, -16384}, {31650, -8481}, {32767, 0}
};
static void sys_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);   /* the bay's clip (below) */
typedef void (*ring_line_t)(int32_t, int32_t, int32_t, int32_t, uint16_t);
/* (1.9.4) ONE RING, ONE LOOP. This was two functions -- gw_ring_tab on the panel (cv_line) and gw_ring_clip in
 * HOME's bay (sys_line) -- with the same 24-segment body twice, and at -Oz the toolchain inlined each copy whole:
 * 1 344 B of identical code for a different name on the line call. The loop is one now, noinline so it is not
 * pasted back into both callers, and the line primitive is its only difference: the pixels are the same ones the
 * two copies drew (the golden renders and the pixel sweep compare them). */
static __attribute__((noinline)) void gw_ring(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col,
                                              int dotted, ring_line_t line)
{
    uint32_t k;
    int32_t px = cx + rx, py = cy;
    for (k = 0; k < 24u; k++) {
        int32_t x = cx + (rx * GW_RING[k][0] >> 15), y = cy + (ry * GW_RING[k][1] >> 15);
        /* two chords a table step (48 segments): a 15-degree chord of a 34 px ellipse bowed by half a
         * pixel, so the ring reads as a curve, not as a polygon of dashes. The midpoint is the chord's,
         * not the arc's -- half a pixel off at most, and no second table. */
        int32_t mx = (px + x) / 2, my = (py + y) / 2;
        if (!dotted || (k & 1u))
            line(px, py, mx, my, col);
        if (!dotted || !(k & 1u))
            line(mx, my, x, y, col);
        px = x;
        py = y;
    }
}
/* the ring gw_orbit_ring paints (24 segments from cx + rx, clockwise, every other one when dotted), from the
 * table: identical geometry, no interpolation */
static __attribute__((noinline)) void gw_ring_tab(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int dotted)
{
    gw_ring(cx, cy, rx, ry, col, dotted, cv_line);
}
/* the same ring, clipped by HOME's bay: the note's answer and the touch's rings are drawn with it, so an echo
 * that leaves the 91 x 55 px square is dropped rather than painted over the panel's border */
static __attribute__((noinline)) void gw_ring_clip(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int dotted)
{
    gw_ring(cx, cy, rx, ry, col, dotted, sys_line);
}

/* A shaded planet: the dark side, the lit crescent turned toward the star, a terminator pixel
 * row between them, an atmosphere ring, a specular and two surface bands. Row spans come from a
 * monotone walk of the circle's half width (no sqrt, no table): the widest row is r px.
 * lx/ly: the unit-ish direction of the star from the planet, -1 / 0 / 1 each. */
static void sys_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);   /* the bay's clip (below) */
static void sys_pset(int32_t x, int32_t y, uint16_t c);
static void sys_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c);
/* (4.0) where the star was drawn and how big it was: the hit test's, and 0 until the bay has drawn one */
static int16_t sys_star_x = 53, sys_star_y = 92;
static uint8_t sys_star_r = 4;
/* (3.5) the ringed giant's own plane, opened by its touch (PK_SLING): 0 at rest, up to 2 px while pushed */
static int32_t sys_ring_lift;
/* (3.5) ONE REACTION PER WORLD. The five gestures are different mechanisms, not one animation in five colours:
 *   PK_SPIN   the rocky world surges along its own orbit and leaves a comet trail (a bump that starts and ends
 *             on the path, so the motion never jumps);
 *   PK_SWELL  the banded giant inhales two pixels and lets them go;
 *   PK_RIPPLE the ocean world sends two rings out of its surface, out of step with each other;
 *   PK_SLING  the ringed giant is pushed out along its radius and drawn back, its ring plane opening;
 *   PK_FROST  the frozen world rings with frost and throws a fan of flakes.
 * A reaction is a pure function of the frame it was touched, never an accumulator: a second touch moves that
 * frame, so any reaction can be interrupted and restarted at once, and two bodies can answer together. */
enum { PK_SPIN, PK_SWELL, PK_RIPPLE, PK_SLING, PK_FROST };
static const uint8_t PL_POKE[NPL] = {PK_SPIN, PK_SWELL, PK_RIPPLE, PK_SLING, PK_FROST};
static const uint8_t POKE_L[NPL] = {34, 30, 34, 30, 34};     /* ~0.5 s at the panel's frame rate */
static uint32_t pl_poke_t[NPL];                              /* ui.frame of the last touch */
static uint8_t pl_poke_on[NPL], pl_sr[NPL], pl_order[NPL];
static uint32_t gal_t;                                       /* (3.1.5) the frame the band was touched */   /* touched? the radius drawn, the depth order */
static int16_t pl_sx[NPL], pl_sy[NPL];                       /* where each body was drawn: the hit test's */
static int32_t gw_span(int32_t r, int32_t dy)                /* the row's half width, no sqrt, no libm */
{
    int32_t s;
    for (s = r; s > 0 && s * s + dy * dy > r * r; s--)
        ;
    return s;
}
/* kind: 0 rocky (craters), 1 banded (a gas giant), 2 ocean (a bright specular and a thick
 * atmosphere), 3 ringed (the giant with its ring). (3.2) they were three identical discs with
 * different radii; each now has its own surface, and a note makes it grow up to two pixels. */
/* (7.3) A MOON. The gas giant and the frozen world were the only two bodies with nothing else of their own on the
 * path; each now carries a companion: one pixel lit on the half facing the star, one in shadow on the other, on an
 * ellipse just outside its own limb, its angle a pure function of the clock (no state, no allocation). Two psets a
 * moon a frame, and the pair turn at different rates, so the bay keeps moving where it already did. */
static __attribute__((noinline)) void gw_moon(int32_t px, int32_t py, int32_t pr, uint32_t tm, uint32_t seed, int32_t heat)
{
    uint32_t ph = tm * (4u + (seed & 3u)) + seed * 7919u;
    int32_t rx = pr + 4, ry = pr + 2;
    int32_t mx = px + (bg_sin(ph + 0x4000u) * rx >> 15);
    int32_t my = py + (bg_sin(ph) * ry >> 15);
    int32_t sp = bg_sin(ph + 0x4000u) >> 15;
    sys_pset(mx, my, ux_mix(T_BG, bg_col(heat + 50), 150));      /* its lit face */
    sys_pset(mx + (sp > 0 ? 1 : -1), my, ux_mix(T_BG, bg_col(heat), 70));   /* and the one turning away */
}
/* (7.4) EACH WORLD ITS OWN WEATHER. The bodies had a surface and a rotation; what they lacked was something that
 * happens ON them. Two effects a body now, each as far from the next world's as its surface is: the rocky one
 * throws a dust plume off its lit limb and walks a glint along it, the gas giant flickers lightning between its
 * belts, the ocean world rolls a crest and flashes its glint, the ringed giant runs a sparkle round its ring, and
 * the frozen one wears an aurora over its pole and a drifting frost mote. Every phase is a function of the clock,
 * so nothing accumulates: three or four psets a body a frame, and none of it is a table or an allocation. */
static __attribute__((noinline)) void gw_body_fx(int kind, int32_t x, int32_t y, int32_t r, int32_t heat, int32_t lx, uint32_t tm)
{
    uint32_t ph = tm * 7u + (uint32_t)kind * 2600u;
    uint16_t hi = ux_mix(T_BG, bg_col(heat + 90), 62);
    uint16_t lo = ux_mix(T_BG, bg_col(heat + 30), 44);
    if (r < 3)
        return;
    switch (kind) {
    case 0: {                                       /* ROCKY: a plume off the limb, and a glint that walks it */
        uint32_t a = ph % 1500u;
        int32_t s = (int32_t)(a / 18u);
        if (a < 90u) {
            sys_pset(x + lx * (r - 1) - s, y - r + s, ux_mix(T_BG, bg_col(heat + 60), (90 - s * 14) / 2));
            sys_pset(x + lx * (r - 1) - s - 1, y - r + s + 1, ux_mix(T_BG, bg_col(heat + 20), (60 - s * 8) / 2));
        }
        break;
    }
    case 1: {                                       /* GAS: lightning between the belts */
        uint32_t a = ph % 900u;
        if (a < 30u) {
            int32_t sx = x - r + 1 + (int32_t)((ph / 900u) % (uint32_t)(2 * r - 1));
            sys_pset(sx, y - 1, ux_mix(T_BG, T_TEXT, 140));
            sys_pset(sx + lx, y, ux_mix(T_BG, T_TEXT, 90));
        }
        break;
    }
    case 2: {                                       /* OCEAN: a crest rolling up, a glint flashing after it */
        uint32_t a = ph % 1200u;
        if (a < 200u)
            sys_pset(x - lx * (r - 2), y + r / 2 - (int32_t)(a / 90u), lo);
        if (a > 820u)
            sys_pset(x + lx * (r - 2), y + r / 3, hi);
        break;
    }
    case 3: {                                       /* RINGED: a sparkle riding the ring, its shadow under it */
        int32_t a = (int32_t)(ph % 32768u), rr = r + 3;
        int32_t sx = x + (bg_sin((uint32_t)a + 0x4000u) * rr >> 15);
        int32_t sy = y + (bg_sin((uint32_t)a) * (rr * 2 / 3) >> 15);
        sys_pset(sx, sy, hi);
        sys_pset(sx - lx, sy + 1, lo);
        break;
    }
    default: {                                      /* FROZEN: an aurora over the pole, a frost mote below it */
        uint32_t a = (uint32_t)((bg_sin(ph * 2u) + 32768) >> 13);
        int32_t k;
        for (k = -1; k <= 1; k++)
            sys_pset(x + k * 2, y - r - 1 - (k == 0), ux_mix(T_BG, bg_col(heat + 120), 30 + (int32_t)a / 6 - (k ? 9 : 0)));
        if ((ph % 1100u) < 300u)
            sys_pset(x - lx * r, y - 1, ux_mix(T_BG, T_TEXT, 110));
        break;
    }
    }
}
static __attribute__((noinline)) void gw_sphere(int32_t x, int32_t y, int32_t r, int32_t heat, int32_t lx, int32_t ly, int kind,
                      uint32_t rot, int32_t flash)
{
    int32_t dy, span, atm, lo, hi, t, tz, lift, bi;
    uint16_t edge, lit_top, lit_low, tone[3][2], rim, atm2;
    /* (3.0) a phase, not a crescent patch: the light comes from the star, so the terminator is the line
     * where the surface normal turns away from it, and each row is lit from that line to the lit limb.
     * (3.1) every row goes through sys_rect/sys_pset: the body, its atmosphere and its ring are clipped by
     * the bay itself, and the radii are small (2..5 px + effects) so the shading reads at 240 x 240. */
    uint16_t base = bg_col(heat - 70);                         /* the planet's own tone on the thermal ramp */
    uint16_t litc = ux_mix(base, T_TEXT, 46);                 /* the sunlit surface */
    uint16_t mid = ux_mix(T_BG, base, 66);                    /* the surface away from it: dark, not a hole */
    uint16_t dark = ux_mix(T_BG, base, 26);                   /* its own shadow: the ring's, one pixel of it */
    uint16_t term = ux_mix(litc, mid, 55), atmc = ux_mix(T_BG, bg_col(heat + 30), 38);
    /* (3.3) a note is a strike of light, not only a bigger body: the lit face and its limb brighten for the
     * first frames of pl_flash and fall back as it decays. Both tones are computed once, not per pixel. */
    lift = flash > 0 ? (flash >> 4) : 0;
    if (lift > 70) lift = 70;
    edge = ux_mix(litc, T_TEXT, 55 + lift);
    lit_top = ux_mix(litc, T_TEXT, 18 + lift);
    lit_low = ux_mix(litc, mid, 30 - (lift >> 1));
    /* (3.5) VOLUME. A body was lit by latitude alone -- a vertical gradient with a terminator -- which on a
     * 4..7 px disc reads as a flat coin. Its lit face is a 3 x 2 grid now: three latitude bands down the body
     * (the pole, the equator, the far side) and two halves across it, split where the light falls off. Six
     * tones computed once per body, never once per row: the extra volume costs six ux_mix and not a pixel. */
    for (bi = 0; bi < 3; bi++) {
        uint16_t lat = bi == 0 ? lit_top : bi == 2 ? lit_low : litc;
        tone[bi][0] = ux_mix(lat, T_TEXT, 8 + (lift >> 2));    /* the half the light lands on */
        tone[bi][1] = ux_mix(lat, mid, 34);                    /* the half falling away from it */
    }
    /* the limb that turns away keeps one pixel of its own: a silhouette, not a hole in the bay */
    rim = ux_mix(mid, T_ACCENT, 30);
    atm2 = ux_mix(T_BG, bg_col(heat + 46), 22);
    if (r < 2) r = 2;
    if (r > 9) r = 9;                                           /* a touch can swell a body two pixels */
    /* (4.0) ONE PIXEL OF SPACE. Two paths cross, and two bodies that touch merged into a single
     * silhouette until the eye could not tell a small world in front from a large one behind. A ring of
     * the page's own background is laid at r + 2 before the body: the world in front keeps an edge, the
     * one behind loses the overlap, and nothing else changes. It is one pixel a row, and it follows the
     * body when a note or a touch swells it. */
    {
        int32_t d2;
        for (d2 = -(r + 2); d2 <= r + 2; d2++) {
            int32_t s2 = gw_span(r + 1, d2);
            if (s2) {
                sys_pset(x - s2 - 1, y + d2, T_BG);
                sys_pset(x + s2 + 1, y + d2, T_BG);
            }
        }
        for (d2 = -r - 2; d2 <= -r - 1; d2++) {
            int32_t s2 = gw_span(r, d2);
            sys_rect(x - s2, y + d2, 2 * s2 + 1, 1, T_BG);
        }
        for (d2 = r + 1; d2 <= r + 2; d2++) {
            int32_t s2 = gw_span(r, d2);
            sys_rect(x - s2, y + d2, 2 * s2 + 1, 1, T_BG);
        }
    }
    if (kind == 3) {                                               /* the ring first: the body hides its middle, so it
                                                                   * reads as one ring passing behind the planet. (3.5) it is
                                                                   * two ringlets at different radii, and the touch opens the
                                                                   * plane from 2 to 4 px (sys_ring_lift) */
        int32_t rt = 2 + sys_ring_lift;
        gw_ring_clip(x, y, r + 4, rt, ux_mix(T_BG, bg_col(heat + 40), 64 + (lift >> 1)), 0);
        gw_ring_clip(x, y, r + 2, rt > 1 ? rt - 1 : 1, ux_mix(T_BG, bg_col(heat + 20), 88 + (lift >> 2)), 0);
    }
    for (dy = -(r + 1); dy <= r + 1; dy++) {                   /* the atmosphere: the 1 px ring the body leaves */
        atm = gw_span(r + 1, dy);
        span = gw_span(r, dy);
        if (atm > span) {
            sys_pset(x - atm, y + dy, atmc);
            sys_pset(x + atm, y + dy, atmc);
            if (kind == 2 || kind == 3) {                      /* (3.5) OCEAN and the RINGED giant carry a second,
                                                                  * fainter shell: their limbs do not end on a line */
                sys_pset(x - atm - 1, y + dy, atm2);
                sys_pset(x + atm + 1, y + dy, atm2);
            }
        }
    }
    for (dy = -r; dy <= r; dy++) {
        span = gw_span(r, dy);
        if (!span && dy)
            continue;
        sys_rect(x - span, y + dy, 2 * span + 1, 1, mid);
        if (span)                                              /* the far limb, one pixel of rim light */
            sys_pset(x + (lx > 0 ? -span : span), y + dy, rim);
        /* the phase: dx * lx + dy * ly >= 0 is lit; lx is +-1, so the terminator crosses every row */
        t = -dy * ly;
        lo = -span;
        hi = span;
        if (lx > 0) {
            if (t > lo) lo = t;
        } else if (t < hi)
            hi = t;
        if (hi < lo)
            continue;
        if (lo > -span)
            sys_pset(x + lo - 1, y + dy, term);
        else if (hi < span)
            sys_pset(x + hi + 1, y + dy, term);
        /* (3.5) the lit face: two halves across the body, split 2/5 of the way from the terminator, so the light
         * falls off across the disc as well as down it. The latitude band picks the tone; the split is one
         * compare and at most two row fills -- no more work than the single fill it replaces. */
        tz = dy * 3 / (2 * r + 1);
        bi = tz < 0 ? 0 : tz > 0 ? 2 : 1;
        if (hi > lo) {
            int32_t q = lo + (hi - lo) * 2 / 5;
            if (lx > 0) {
                if (q > lo)
                    sys_rect(x + lo, y + dy, q - lo, 1, tone[bi][1]);
                sys_rect(x + q, y + dy, hi - q + 1, 1, tone[bi][0]);
            } else {
                sys_rect(x + lo, y + dy, q - lo + 1, 1, tone[bi][0]);
                if (hi > q)
                    sys_rect(x + q + 1, y + dy, hi - q, 1, tone[bi][1]);
            }
            sys_pset(x + (lx > 0 ? hi : lo), y + dy, edge);
        }
    }
    /* (3.3) THE SURFACE TURNS. Until 3.2 a body was a shaded disc on an orbit: the system rotated, but
     * nothing on any planet moved, so each one read as a still picture on a carousel. Two features now sit
     * on their own longitudes of the body and sweep across the lit face: x = r * sin(phase) packs them
     * toward the limb as they go round, cos(phase) hides them behind it, and the tone falls off as they
     * leave the light. One sine pair and at most two pixels per feature per planet per frame -- the cost of
     * the specular it joins, and no table and no allocation. */
    if (r >= 3) {
        int32_t k;
        for (k = 0; k < 2; k++) {
            uint32_t ph = rot + (k ? 0x6C00u : 0x2400u);
            int32_t sp2 = bg_sin(ph) >> 15, cp2 = bg_sin(ph + 0x4000u) >> 15;
            int32_t fy = k ? (r + 1) / 2 : -((r + 1) / 2), span2, fx;
            if (cp2 < 2)                                       /* round the back: nothing to see */
                continue;
            span2 = gw_span(r, fy);
            if (span2 < 1)
                continue;
            fx = (r * sp2) >> 15;
            if (fx > span2) fx = span2;
            if (fx < -span2) fx = -span2;
            if (fx * lx + fy * ly < 0)                          /* on the dark side: hidden */
                continue;
            sys_pset(x + fx, y + fy, ux_mix(litc, mid, 74 - (cp2 >> 12)));
            if (fx + 1 <= span2)
                sys_pset(x + fx + 1, y + fy, ux_mix(litc, T_TEXT, 24 + (cp2 >> 12)));
        }
    }
    if (kind == 0 && r >= 3) {                                 /* ROCKY: a mare, a lit ridge and two craters */
        int32_t rw = gw_span(r, 1);
        sys_rect(x + (lx > 0 ? -rw + 1 : 1), y + 1, (2 * rw - 1) * 2 / 3, 1, ux_mix(mid, T_BG, 45));
        sys_pset(x + lx * (r - 3), y - r + 2, ux_mix(litc, T_TEXT, 62));
        sys_pset(x + lx, y - 1, ux_mix(mid, T_BG, 55));
        sys_pset(x + lx + 1, y - 1, ux_mix(mid, T_BG, 30));
        if (r >= 4) {
            sys_pset(x + (lx > 0 ? -1 : 2), y + 2, ux_mix(mid, T_BG, 55));
            sys_pset(x + (lx > 0 ? -1 : 2), y + 3, ux_mix(mid, T_BG, 30));
        }
    } else if (kind == 1 && r >= 3) {                           /* GAS: three belts and one storm eye */
        int32_t bw = gw_span(r, 0);
        sys_rect(x - bw + 1, y - 2, 2 * bw - 1, 1, ux_mix(mid, litc, 52));
        sys_rect(x - gw_span(r, 1) + 1, y, 2 * gw_span(r, 1) - 1, 1, ux_mix(mid, T_BG, 62));
        sys_rect(x - bw + 1, y + 2, 2 * bw - 1, 1, ux_mix(mid, litc, 30));
        if (r >= 4)
            sys_pset(x + lx * (r - 3), y - 1, ux_mix(litc, T_TEXT, 58));
    } else if (kind == 2 && r >= 3) {                           /* OCEAN: a horizon, a glint and a shore */
        int32_t ow = gw_span(r, r / 2);
        sys_rect(x - ow + 1, y + r / 2, 2 * ow - 1, 1, ux_mix(litc, T_TEXT, 30));
        sys_pset(x - lx * (r / 2), y + r / 3, ux_mix(litc, T_TEXT, 74));
        sys_pset(x + lx * (r - 2), y + 1, ux_mix(mid, litc, 40));
    } else if (kind == 3 && r >= 3) {                           /* RINGED: the ring's shadow, both sides */
        int32_t sw2 = gw_span(r, 0);
        sys_rect(x - sw2 + 1, y - 2 - sys_ring_lift, 2 * sw2 - 1, 1, ux_mix(mid, dark, 50));
        sys_rect(x - sw2 + 1, y + 2 + sys_ring_lift, 2 * sw2 - 1, 1, ux_mix(mid, dark, 38));
    } else if (kind == 4 && r >= 3) {                           /* (2.1) FROZEN: two caps, a crevasse and a speck of
                                                                * frost at the terminator -- a body that reads at once
                                                                * as ice, so the fifth world is not another grey rock */
        int32_t fw = gw_span(r, 1);
        sys_rect(x - fw + 1, y - r + 1, 2 * fw - 1, 1, ux_mix(litc, T_TEXT, 55));
        sys_rect(x - fw + 1, y + r - 1, 2 * fw - 1, 1, ux_mix(litc, T_TEXT, 40));
        sys_rect(x - gw_span(r, 0) + 2, y, 2 * gw_span(r, 0) - 3, 1, ux_mix(mid, T_BG, 55));
        sys_pset(x - lx, y + r - 2, ux_mix(litc, T_TEXT, 80));
    }
    if (r >= 3) {                                              /* the specular, where the light lands */
        int32_t sx = x + lx * (r - 2) * 2 / 3, sy = y + ly * (r - 2) * 2 / 3;
        sys_pset(sx, sy, ux_mix(bg_col(240), T_TEXT, 75));
        sys_pset(sx - lx, sy - ly, ux_mix(litc, T_TEXT, 45));
        /* (3.5) one pixel of bounce under the highlight: the body has a near side, not only a lit side */
        sys_pset(sx - lx, sy + 1, ux_mix(litc, bg_col(200), 34));
    }
}

/* Which planet a track feeds, and how hard: 1/4 -> 0, 2/5 -> 1, 3/6 -> 2. A note-on is a voice
 * whose (note, gate) differs from the last scan's. Additive and clamped, so a chord or a fast run
 * raises the excitation instead of restarting it; the release is a fixed fraction a frame. */
#if !UI_NO_DRIFT
static __attribute__((noinline)) void gw_planet_excite(int on)
{
    uint8_t hit[NPL] = {0}, vel[NPL] = {0};
    uint32_t t, i, g;
    for (t = 0; t < NTRK; t++) {
        int sounding = on && t < trk_live && !trk[t].p[P_MUTE];
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &V(&trk[t], i);
            int live = sounding && v->active && v->gate && v->note < 128u;
            uint8_t tag = live ? (uint8_t)(v->note + 1u) : 0u;
            if (live && pl_seen[t][i] != tag) {
                g = t % NPL;
                hit[g] = 1;
                if (v->vel > vel[g]) vel[g] = v->vel;
            }
            pl_seen[t][i] = tag;
        }
    }
    for (g = 0; g < NPL; g++) {
        int32_t e = pl_ex[g], f = pl_flash[g];
        if (hit[g]) {
            e += 120 + (int32_t)vel[g] * 3 / 2;               /* a boom, and the velocity on top */
            f = 256;                                          /* (3.3) and the light of the strike */
        }
        e -= (e >> 3) + (e ? 1 : 0);                          /* ~8 frames to 1/e: a note's afterglow */
        f -= (f >> 2) + (f ? 1 : 0);                          /* ~4 frames: the strike is shorter */
        pl_ex[g] = (int16_t)clamp(e, 0, 256);
        pl_flash[g] = (int16_t)clamp(f, 0, 256);
    }
}
#else
/* the host's deterministic pixel builds (UI_NO_DRIFT) freeze the response: no scan, no excitation */
static __attribute__((noinline)) void gw_planet_excite(int on)
{
    (void)on;
    { uint32_t k; for (k = 0; k < NPL; k++) pl_ex[k] = 0; }
    pl_flash[0] = pl_flash[1] = pl_flash[2] = pl_flash[3] = 0;
}
#endif

/* (3.1) HOME's bay is a real clip, not a suggestion. Every primitive the system draws goes through these
 * row/pixel helpers, so a body, a ring, a corona, a trail or a gesture spark can never paint outside the
 * 91 x 55 px square the panel gives it. The check is four compares per call -- no clip stack, no state to
 * save and restore in gfx.c's hot path. */
static int16_t sys_x0 = 9, sys_y0 = 66, sys_x1 = 97, sys_y1 = 118;
static void sys_box(int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    sys_x0 = (int16_t)(x0 + 1);
    sys_y0 = (int16_t)(y0 + 1);
    sys_x1 = (int16_t)(x0 + w - 2);
    sys_y1 = (int16_t)(y0 + h - 2);
}
static void sys_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h;
    if (x < sys_x0) x = sys_x0;
    if (y < sys_y0) y = sys_y0;
    if (x1 > sys_x1 + 1) x1 = sys_x1 + 1;
    if (y1 > sys_y1 + 1) y1 = sys_y1 + 1;
    if (x1 > x && y1 > y)
        cv_rect(x, y, x1 - x, y1 - y, c);
}
static void sys_pset(int32_t x, int32_t y, uint16_t c)
{
    if (x >= sys_x0 && x <= sys_x1 && y >= sys_y0 && y <= sys_y1)
        cv_pset(x, y, c);
}
static void sys_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    /* the segments here are short and the endpoints are already inside by construction: clamp both so a
     * trail cannot leave the square, and skip a segment whose point is outside (a clamped line would join
     * the wrong place) */
    if (x0 < sys_x0 || x0 > sys_x1 || y0 < sys_y0 || y0 > sys_y1 ||
        x1 < sys_x0 || x1 > sys_x1 || y1 < sys_y0 || y1 > sys_y1)
        return;
    cv_line(x0, y0, x1, y1, c);
}
/* half an orbit: the far half (behind the star) or the near one, from the same 24-entry table */
static __attribute__((noinline)) void gw_ring_semi(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int back)
{
    uint32_t k;
    int32_t px = cx + rx, py = cy;
    for (k = 0; k < 24u; k++) {
        int32_t x = cx + (rx * GW_RING[k][0] >> 15), y = cy + (ry * GW_RING[k][1] >> 15);
        int32_t mx = (px + x) / 2, my = (py + y) / 2;
        if (((my < cy) ? 1 : 0) == back) {
            sys_line(px, py, mx, my, col);
            sys_line(mx, my, x, y, col);
        }
        px = x;
        py = y;
    }
}

/* the bay's sky: a fixed star field with its own twinkle phases, two faint nebula bands, and the system over
 * them. Every position is precomputed: the per-frame cost is one sine per star (slowed to a quarter of the
 * frame rate), no per-pixel trigonometry, and no allocation. */
/* (3.5) how far into its own reaction a body is: the length itself means "at rest", so every effect below is
 * one compare. No accumulator: the frame of the touch is the whole state, which is what makes a reaction
 * interruptible at any moment (a second touch moves the frame, the next frame starts the gesture again). */
static uint32_t pl_age(int32_t i, uint32_t len)
{
#if UI_NO_DRIFT
    (void)i;
    (void)len;
    return 64u;                                     /* the deterministic builds freeze the gesture */
#else
    uint32_t a;
    if (!pl_poke_on[i] || (ui_prefs & PREF_ANIM_OFF))
        return 64u;
    a = ui.frame - pl_poke_t[i];
    return a < len ? a : 64u;
#endif
}

/* what HOME's bay adds to the page's signature while a body is answering, so the bay is redrawn every frame of
 * a reaction instead of every other one (0 when nothing is happening: the page keeps its old rhythm) */
static uint32_t gw_planet_sig(void)
{
    uint32_t i, s = 0;
    for (i = 0; i < NPL; i++) {
        uint32_t a = pl_age((int32_t)i, POKE_L[i]);
        if (a < 64u)
            s = s * 2654435761u + (uint32_t)i * 7u + a + 1u;
    }
    return s;
}

/* which body is under (x, y): the near ones first, in the depth order of the last frame, and only where a body
 * was actually drawn. A tap on the star, the dust or the panel is nothing at all. */
static int32_t gw_planet_at(int32_t x, int32_t y)
{
    int32_t k;
    /* the panel is drawn on its own strip and blitted at graph_y(): a tap arrives in screen rows */
    y -= (int32_t)graph_y();
    /* (4.0) THE STAR FIRST. A body crossing the centre used to take the tap that landed on the star, and the
     * gesture that should reach the whole system hit one world instead. The star is the bay's centre and its
     * largest target, so it is answered before the bodies: its disc and one pixel of corona, NPL its index. */
    {
        int32_t dx = x - sys_star_x, dy = y - sys_star_y, r = (int32_t)sys_star_r + 2;
        if (dx * dx + dy * dy <= r * r)
            return (int32_t)NPL;
    }
    /* (3.1.2) THE NEAREST BODY, NOT THE FRONT ONE. Five bodies share a 44 px bay, so the discs a tap can
     * claim overlap: taking the front-most of them gave every shared pixel to whichever was drawn last, and
     * the worlds behind it answered only where they poked out. The tap now goes to the body whose centre is
     * closest, which splits each overlap down the middle and leaves every body a target of its own size.
     * On an even distance the front one keeps it, so nothing flickers between two candidates. */
    {
        int32_t best = -1, bestd = 0x7FFFFFFF;
        for (k = (int32_t)NPL - 1; k >= 0; k--) {
            int32_t i = pl_order[k];
            int32_t dx = x - pl_sx[i], dy = y - pl_sy[i], r = (int32_t)pl_sr[i] + 3;
            int32_t d;
            if (!pl_sr[i] || dx * dx + dy * dy > r * r)
                continue;
            d = dx * dx + dy * dy;
            if (d < bestd) { bestd = d; best = i; }
        }
        return best;
    }
}

/* (FASE 2) which body answered last, and when: the selection ring reads it (gw_body_sel, below) */
static uint32_t pl_sel_i = 0xFFu, pl_sel_t;
static __attribute__((noinline)) void gw_planet_poke(int32_t i)
{
    if (i == (int32_t)NPL) {
        gal_t = ui.frame;                                   /* (3.1.5) and the band lights from its centre */                        /* (4.0) THE STAR: an expansion that reaches the bodies.
                                                     * Its rings are HOME's own pulse (graph_ecosys), and every
                                                     * world inside them takes the light: each body's flash is
                                                     * set and its excitation lifted, so the near ones swell by a
                                                     * pixel and brighten as the wave passes them. No new state:
                                                     * the two fields a note already uses. */
        uint32_t k;
        for (k = 0; k < NPL; k++) {
            pl_flash[k] = 256;
            pl_ex[k] = (int16_t)clamp((int32_t)pl_ex[k] + 120, 0, 256);
        }
        pl_sel_i = 0xFFu;                           /* the star answers for the system: no single body lit */
        ui.home_pulse = ui.frame | 1u;              /* 0 means "no pulse": never store one */
        ui.force = 1;
        return;
    }
    if (i < 0 || i >= (int32_t)NPL)
        return;
    pl_poke_t[i] = ui.frame;
    pl_poke_on[i] = 1u;
    pl_sel_i = (uint32_t)i;                         /* (FASE 2) and the ring says which one heard it */
    pl_sel_t = ui.frame;
    pl_flash[i] = 256;                              /* the strike itself, as a note's is */
    if (PL_POKE[i] == PK_FROST) {                   /* the frozen world's fan starts with the touch */
        int32_t k;
        vz_np_layer(VZ_OWN_HOME);
        for (k = 0; k < 5; k++)
            vz_np_add(pl_sx[i] + (k & 1) * 2 - 1, pl_sy[i] + ((k >> 1) & 1) * 2 - 1,
                      (k & 1 ? 2 : -2), -2 + (k >> 2), bg_col(150 + k * 16), 9, k & 1 ? 1 : 2);
    }
    ui.force = 1;                                   /* the bay is redrawn now, not on its next lazy pass */
}

/* (3.5) the touch's own answer, drawn under the body it belongs to: a different mechanism for each world, and
 * only while that body is inside its own reaction. Called for the bodies behind the star as well as for those in
 * front of it, so a touch answers wherever the body happens to be on its path. */
/* (FASE 2) SELECTION. The body that answered last keeps a thin ring, drawn in its own tone and fading over the
 * reaction: at 240 x 240 the five are a few pixels apart and a touch had no lasting trace, so there was no way
 * to see which one heard you. One arc pass, no state beyond the frame the poke happened. */
static __attribute__((noinline)) void gw_body_sel(int32_t bx, int32_t by, int32_t r, int32_t heat)
{
    uint32_t age;
    int32_t a;
    if (pl_sel_i == 0xFFu || !pl_sel_t)
        return;
    age = ui.frame - pl_sel_t;
    if (age >= 40u) {
        pl_sel_i = 0xFFu;
        return;
    }
    for (a = 0; a < 65536; a += 2048) {                 /* every 11 degrees: a ring, not a disc */
        int32_t rx = (r + 3) * (bg_sin((uint32_t)a + 0x4000u) >> 15);
        int32_t ry = (r + 3) * (bg_sin((uint32_t)a) >> 15) * 2 / 3;
        sys_pset(bx + rx, by + ry, ux_mix(T_BG, bg_col(heat + 90), 190 - (int32_t)age * 4));
    }
}

/* (FASE 4) ONE ANSWER FOR EVERY WORLD. Each body already had its own reaction, but they were not the same
 * weight: the ocean pushed a wide unbroken front and the ringed one drew five pixels of wake, so the panel read
 * as if only one world answered a touch. The ring and the three sparks below are drawn for all five, and the
 * per-kind block that follows adds the signature on top (the echo, the flakes, the turning arcs, the halo, the
 * wake). Same gesture, five accents. */
static __attribute__((noinline)) void gw_body_react(int32_t pi, int32_t bx, int32_t by, int32_t heat)
{
    uint32_t pa = pl_age(pi, POKE_L[pi]);
    int32_t common_br = pl_sr[pi] ? (int32_t)pl_sr[pi] : 4;
    /* (FASE 2/5) the wave leaves the LIMB, not a 3 px circle inside the body. Every body now lays a pixel of
     * the page's background at r + 2 (gw_sphere) so two overlapping worlds still read as two, and a ring that
     * began under the surface was wiped by it: the ocean's touch measured 5 pixels of change instead of a
     * hundred. br is the radius the body was drawn with last frame (pl_sr, one frame of lag, invisible at
     * 60 fps) and the reactions start outside it. */
    int32_t br = common_br;
    if (pa >= 64u)
        return;
    {   /* the shared answer: a front leaving the limb, and three sparks thrown off it */
        int32_t r0 = br + 2 + (int32_t)pa / 2;
        gw_ring_clip(bx, by, r0, r0 * 2 / 3, ux_mix(T_BG, bg_col(heat + 90), clamp(220 - (int32_t)pa * 5, 0, 220)), 0);
        if (pa < 30u && !(pa & 2u)) {
            int32_t k, a0 = heat * 91 + (int32_t)pa * 700;
            for (k = 0; k < 3; k++) {
                uint32_t a = (uint32_t)(a0 + k * 21845) & 0xFFFF;
                int32_t sx = bg_sin(a + 0x4000u) * (br + 1) >> 15;
                int32_t sy = (bg_sin(a) * (br + 1) >> 15) * 2 / 3;
                int32_t sp = 1 + (int32_t)pa / 6;
                sys_pset(bx + sx + (sx >= 0 ? sp : -sp), by + sy + (sy >= 0 ? sp : -sp),
                         ux_mix(T_BG, bg_col(heat + 120), clamp(190 - (int32_t)pa * 7, 0, 190)));
            }
        }
    }
    if (PL_POKE[pi] == PK_RIPPLE) {                 /* OCEAN: a wave leaves the surface -- one unbroken front
                                                     * and a broken echo behind it -- with a crest running
                                                     * out along it, so a touch reads as water, not as a pulse */
        if (pa > 9u) {
            int32_t r2 = br + 2 + (int32_t)(pa - 9u) / 2;
            gw_ring_clip(bx, by, r2, r2 * 2 / 3, ux_mix(T_BG, bg_col(heat + 30), 150 - (int32_t)pa * 3), 1);
        }
        sys_pset(bx + (int32_t)pa / 2, by - (int32_t)pa / 3,
                 ux_mix(T_BG, bg_col(heat + 130), 200 - (int32_t)pa * 5));
        if (pa < 12u && !(pa & 1u))
            vz_np_add(bx, by, (pa & 2u) ? 1 : -1, -1, bg_col(heat + 100), 7, 1);
    } else if (PL_POKE[pi] == PK_FROST) {           /* FROZEN: a ring of frost, and a fan of flakes */
        int32_t r1 = br + 3 + (int32_t)pa / 3;
        gw_ring_clip(bx, by, r1, r1 * 2 / 3, ux_mix(T_BG, bg_col(heat + 90), 170 - (int32_t)pa * 4), 1);
        if (pa < 14u && !(pa & 1u))                 /* two flakes a frame, from the bay's own pool */
            vz_np_add(bx + (int32_t)(pa & 3u) - 2, by - 1,
                      (int32_t)(pa & 3u) - 2, -2, bg_col(heat + 90), 8, 1);
    } else if (PL_POKE[pi] == PK_SPIN) {            /* ROCKY: it turns. An arc runs round the limb, bright at
                                                     * its head and fading behind, and a grain or two is thrown
                                                     * off the terminator, so the touch reads as rotation */
        int32_t k, r = br + 4 + (int32_t)pa / 2, a0 = (int32_t)pa * 11 + heat;
        /* (3.1.5) TWO ARCS, NOT ONE. The turn was drawn as five pixels on a single radius and measured 115 px
         * of ink against the frozen world's 526: it was there, and it did not read. The limb now carries the
         * bright arc and, just outside it, a dimmer one turning a step behind -- the lag is what an eye reads
         * as rotation rather than a blinking dot. */
        for (k = 0; k < 9; k++) {
            int32_t a = (a0 + k * 1600) & 0xFFFF, sx = bg_sin(a), sy = bg_sin(a + 16384);
            int32_t px = sx * r >> 15, py = (sy * r >> 15) * 2 / 3;
            sys_pset(bx + px, by + py, ux_mix(T_BG, bg_col(heat + 40), clamp(96 - k * 10 - (int32_t)pa * 2, 0, 96)));
            if (k < 7) {
                sys_pset(bx + px * (r + 2) / r, by + py * (r + 2) / r,
                         ux_mix(T_BG, bg_col(heat + 90), clamp(92 - k * 10 - (int32_t)pa, 0, 92)));
                sys_pset(bx + px * (r + 3) / r, by + py * (r + 3) / r,
                         ux_mix(T_BG, bg_col(heat + 140), clamp(72 - k * 6 - (int32_t)pa, 0, 72)));
            }
        }
        if (pa < 16u && !(pa & 3u))
            vz_np_add(bx + br - 1, by - 1, 1, -2, bg_col(heat + 80), 6, 1);
    } else if (PL_POKE[pi] == PK_SWELL) {          /* BANDED GIANT: it breathes -- a halo that widens and
                                                     * thins, and two glints sliding along its belts */
        /* (3.1.5) A HALO THAT BREATHES IN TWO SHELLS. One thin ring of 249 px read as a flicker next to the
         * frozen world; the giant now pushes a bright shell and a wider, dimmer one behind it, which is what
         * an inhale looks like from outside. */
        int32_t r1 = br + 4 + (int32_t)pa / 3;
        gw_ring_clip(bx, by, r1, r1 * 2 / 3, ux_mix(T_BG, bg_col(heat + 40), clamp(96 - (int32_t)pa * 2, 0, 96)), 1);
        gw_ring_clip(bx, by, r1 + 2, (r1 + 2) * 2 / 3, ux_mix(T_BG, bg_col(heat + 80), clamp(72 - (int32_t)pa * 2, 0, 72)), 0);
        if (pa < 20u && !(pa & 1u)) {
            sys_pset(bx - (int32_t)pa / 2, by - 1, ux_mix(T_BG, bg_col(heat + 90), 150 - (int32_t)pa * 6));
            sys_pset(bx + (int32_t)pa / 2, by + 2, ux_mix(T_BG, bg_col(heat + 70), 120 - (int32_t)pa * 5));
        }
    } else if (PL_POKE[pi] == PK_SLING) {          /* RINGED: the wake of its push -- the arc widens as the
                                                     * body is drawn back, and embers leave the ring's edge */
        int32_t w = 2 + (int32_t)pa / 8;
        sys_line(bx - w, by + br + 2, bx + w, by + br + 2, ux_mix(T_BG, bg_col(heat + 50), 150 - (int32_t)pa * 2));
        if (pa < 24u && !(pa & 2u)) {
            sys_pset(bx + br + (int32_t)pa / 4, by + 1, ux_mix(T_BG, bg_col(heat + 60), 140 - (int32_t)pa * 5));
            sys_pset(bx - br - (int32_t)pa / 4, by + 1, ux_mix(T_BG, bg_col(heat + 60), 140 - (int32_t)pa * 5));
        }
    }
}


/* (4.0) HOME'S STAR: a core with depth, a corona that turns, and flares of its own.
 * Until 3.5 the star was five concentric discs and a sparkle: a bright circle, not the centre of a
 * system. It has an outer breath now (a fourth, wider disc), a cross of eight rays that turns slowly
 * and whose length follows the patch's energy, and a flare every ~7 s that throws five embers off its
 * rim through the pool the bay already owns. The rays are points on a circle of radius r + k, so a
 * corona costs 8 sines and at most 8 x 6 pixels -- no per-pixel trigonometry anywhere. */
static __attribute__((noinline)) void gw_star(int32_t cx, int32_t cy, int32_t r, int32_t en, int32_t heat, int32_t big, uint32_t tm)
{
    int32_t i, k;
    uint16_t glow = ux_mix(T_BG, bg_col(70 + heat / 3), 30 + big / 14);
    uint16_t corona = ux_mix(T_BG, bg_col(110 + heat / 4), 46 + en / 12);
    gw_disc(cx, cy, r + 3, ux_mix(T_BG, glow, 46));    /* the outer breath: the star lights its own gap */
    gw_disc(cx, cy, r + 2, glow);
    gw_disc(cx, cy, r + 1, ux_mix(T_BG, bg_col(70 + heat / 3), 54 + big / 11));
    /* the turning cross: eight rays, each its own length, one sine pair for all of them */
    for (i = 0; i < 8; i++) {
        uint32_t a = tm * 5u + (uint32_t)i * 8192u;                 /* ~13 s a turn */
        int32_t dx = bg_sin(a + 0x4000u) >> 15, dy = bg_sin(a) >> 15;
        int32_t len = 1 + (int32_t)((bg_sin(a * 3u + 1000u) + 32768) >> 15) + en / 90;
        for (k = 1; k <= len; k++)
            sys_pset(cx + dx * (r + k), cy + dy * (r + k), ux_mix(T_BG, corona, 150 - k * 34));
    }
    gw_disc(cx, cy, r, bg_col(170 + en / 5));
    gw_disc(cx, cy, r * 62 / 100, bg_col(228 + en / 9));
    gw_disc(cx, cy, r * 30 / 100, bg_col(252));
    if (en > 40 || big > 48) {                          /* (7.4) a four-point sparkle while it sounds */
        int32_t sp = 3 + en / 60 + big / 100, s;
        for (s = 1; s <= sp; s++) {
            uint16_t sc = ux_mix(T_BG, bg_col(246), 170 - s * 24);
            sys_pset(cx + r + 1 + s, cy, sc);
            sys_pset(cx - r - 1 - s, cy, sc);
            sys_pset(cx, cy + r + 1 + s, sc);
            sys_pset(cx, cy - r - 1 - s, sc);
        }
    }
    /* (7.4) THE STAR ANSWERS THE WAY A WORLD DOES. The touch's ring -- gw_body_sel's expanding ellipse -- leaves the
     * star every ~1.6 s, so the system's centre has a life of its own and not only a flare: one ring, two psets a
     * row, no state beyond the clock. */
    if (!UI_NO_DRIFT && !(ui_prefs & PREF_ANIM_OFF)) {
        uint32_t hp = (tm * 3u) % 1600u;
        if (hp < 900u) {
            int32_t rr = r + 2 + (int32_t)(hp / 55u);
            gw_ring_clip(cx, cy, rr, rr * 2 / 3,
                         ux_mix(T_BG, bg_col(120 + heat / 2), 130 - (int32_t)(hp / 8u)), 1);
        }
    }
    if (!UI_NO_DRIFT && !(ui_prefs & PREF_ANIM_OFF)) {  /* its flares: a burst, then quiet for seconds */
        uint32_t ph = (tm + 1200u) % 7300u;
        if (ph < 16u) {
            int32_t k2 = (int32_t)(ph >> 2);
            vz_np_layer(VZ_OWN_HOME);
            vz_np_add(cx + k2 - 1, cy - r - 2, k2 - 1, -2, bg_col(200 + k2 * 16), 7, 1);   /* (7.4) it leaves from
                                                                                            * the sparkle, not the disc */
        }
    }
    sys_star_x = (int16_t)cx;
    sys_star_y = (int16_t)cy;
    sys_star_r = (uint8_t)r;
}

/* (4.0) THE WORLD THAT DOES NOT ORBIT. Two bodies stay in the bay's own depth: they turn on their own
 * axis at their own rates, they never leave their place, and they are mixed far out of the page's
 * background so they read behind the system rather than in it. Their terminator is their own phase,
 * not the star's -- a distant world is lit by a sun that is not on this screen. Called before the
 * orbits, so a path passes in front of them and the bay gains a front and a back. */
static __attribute__((noinline)) void gw_far_world(int32_t cx, int32_t cy, int32_t r, uint32_t tm, int32_t rate, int32_t heat, int ring)
{
    int32_t dy, span = 0;
    int32_t ph = (int32_t)((tm * (uint32_t)rate) & 0x7FFFu);
    uint16_t lit = ux_mix(T_BG, bg_col(heat), 32), shade = ux_mix(T_BG, bg_col(heat - 26), 40);
    if (ring)
        gw_ring_clip(cx, cy, r + 2, (r + 2) / 3, ux_mix(T_BG, bg_col(heat + 20), 60), 0);
    for (dy = -r; dy <= r; dy++) {
        int32_t q;
        while (span > 0 && span * span + dy * dy > r * r)
            span--;
        while (span < r && (span + 1) * (span + 1) + dy * dy <= r * r)
            span++;
        q = -span + (2 * span * ph) / 32768;               /* its own terminator */
        if (r <= 2) {                                  /* too small to carry a phase: a mote, one tone */
            sys_rect(cx - span, cy + dy, 2 * span + 1, 1, lit);
        } else {
            if (q > -span)
                sys_rect(cx - span, cy + dy, q + span, 1, shade);
            sys_rect(cx + q, cy + dy, span - q + 1, 1, lit);
        }
        if (span)
            sys_pset(cx + span, cy + dy, ux_mix(T_BG, bg_col(heat + 10), 34));
    }
}

/* (4.0) EACH WORLD'S OWN WEATHER. A body's surface turns already; what it lacked was an event of its
 * own, so five bodies on five paths still looked like five still pictures when no note was playing.
 * Each kind has a period of its own (5.4 s .. 10.6 s, offset by its index) and a different event:
 * the rocky one blows dust off its limb, the gas giant's storm eye surfaces, the ocean world glints,
 * the ringed giant's ring shimmers, the frozen one throws frost off its pole. One compare when an
 * event is not due, a handful of pixels when it is. */
static __attribute__((noinline)) void gw_body_life(int32_t kind, int32_t bx, int32_t by, int32_t r, int32_t heat, uint32_t tm)
{
    uint32_t per = 5400u + (uint32_t)kind * 1300u;
    uint32_t ph = (tm + (uint32_t)kind * 2100u) % per;
    if (UI_NO_DRIFT || (ui_prefs & PREF_ANIM_OFF) || r < 3)
        return;
    switch (kind) {
    case 0:                                             /* ROCKY: a puff off the limb, drifting up */
        if (ph < 26u) {
            int32_t k = (int32_t)ph / 6;
            sys_pset(bx - r - 1 - k, by + 1 - k, ux_mix(T_BG, bg_col(heat + 60), 150 - k * 40));
            sys_pset(bx - r - 1 - k, by + 2 - k, ux_mix(T_BG, bg_col(heat + 20), 90 - k * 20));
        }
        break;
    case 1:                                             /* GAS: the storm eye surfaces and sinks */
        if (ph < 130u) {
            int32_t w = (int32_t)(ph < 65u ? ph : 130u - ph);
            int32_t ey = by - 1 + (int32_t)(ph & 1u);
            sys_pset(bx + r / 3, ey, ux_mix(bg_col(heat + 100), T_TEXT, 40 + w));
            sys_pset(bx + r / 3 + 1, ey, ux_mix(bg_col(heat + 60), T_TEXT, 20 + w / 2));
        }
        break;
    case 2:                                             /* OCEAN: a glint, three pixels, and gone */
        if (ph < 10u) {
            int32_t gx = bx - r / 2, gy = by + r / 3;
            sys_pset(gx, gy, ux_mix(T_TEXT, bg_col(230), 60 - (int32_t)ph * 5));
            sys_pset(gx - 1, gy, ux_mix(bg_col(heat + 40), T_TEXT, 70));
            sys_pset(gx + 1, gy, ux_mix(bg_col(heat + 40), T_TEXT, 70));
        }
        break;
    case 3:                                             /* RINGED: the ring shimmers where it turns */
        if (ph < 40u) {
            int32_t a = (int32_t)((tm * 3u + ph * 800u) & 0xFFFF);
            int32_t rx = (r + 4) * (bg_sin((uint32_t)a + 0x4000u) >> 15);
            int32_t ry = 3 * (bg_sin((uint32_t)a) >> 15);
            sys_pset(bx + rx, by + ry, ux_mix(T_BG, bg_col(heat + 140), 120 + (int32_t)ph * 3));
        }
        break;
    default:                                            /* FROZEN: frost off the pole */
        if (ph < 14u) {
            int32_t k = (int32_t)ph / 4;
            sys_pset(bx, by - r - 1 - k, ux_mix(T_BG, bg_col(heat + 70), 170 - k * 40));
            sys_pset(bx - 1 - k, by - r, ux_mix(T_BG, bg_col(heat + 30), 110 - k * 25));
            sys_pset(bx + 1 + k, by - r, ux_mix(T_BG, bg_col(heat + 30), 110 - k * 25));
        }
        break;
    }
}
static __attribute__((noinline)) void gw_draw_system(int32_t cx, int32_t cy, int32_t r, int32_t en, int32_t heat)
{
    /* three orbits: radius, tilt, rate (16-bit units a millisecond) and planet size. (3.1) SIZES are derived
     * from the bay, not chosen by eye: for orbit i the room left is min(half_w - ORX[i], half_h - ORY[i]);
     * the outer body is 4 px + 1 px atmosphere + a 3 px ring = 8 px, against 10 px of room, so it keeps a
     * 2 px margin at its widest point on every side. The old 4 / 6 / 7 px bodies (with a 4 px ring on the
     * last) needed 11 px and crossed the edge. */
    static const int8_t ORX[5] = {11, 18, 26, 33, 38};         /* (3.2) a fourth orbit, 11 px inside the first:
                                                                * a moon, so the system is not three rings and a sun;
                                                                * (2.1) a fifth outside it. (3.6) the bay is 91 x 55
                                                                * and the clip 9..97 from cx 53, so 39 + 4 px body +
                                                                * 1 px atmosphere = 44 is the clip's own half width:
                                                                * it touches the edge at its widest and never crosses */
    static const uint8_t ORMS[5] = {9, 7, 6, 5, 4};            /* 7.3 s .. 16.4 s a turn (was 9-16 s) */
    /* (3.3) the body's own spin, 16-bit turn units a millisecond: 3.1 s, 4.7 s, 3.9 s and 2.5 s a turn. Faster
     * than the orbit on purpose -- a moon that spins in step with its orbit reads as a cut-out -- and each one
     * its own rate, so no two bodies turn together. */
    static const uint8_t ROTMS[5] = {21, 14, 17, 26, 31};
    /* (2.2) the starting angles, spread evenly round the ellipse (72 degrees apart) instead of the old
     * near-misses: four of them began within a third of a turn, so the bodies bunched into one blob beside
     * the star for most of the first second. They still drift apart on their own rates. */
    static const uint16_t ORPHS[5] = {0x0000, 0x1999, 0x3333, 0x4CCC, 0x6666};
    static const uint8_t ORR[5] = {5, 5, 6, 6, 5};             /* (3.5) each body's own size: the banded giant and
                                                                * the ocean world are the largest, the frozen one a
                                                                * small cold moon. 38 + 4 + 2 = 44 of the bay's 44 */
    static const uint8_t ORYS[5] = {70, 60, 52, 44, 36};       /* tilt: ry = 8 / 11 / 14 / 15 / 13 px */
    /* (3.1.4) WHERE EACH BODY LIVES, relative to the bay's centre. Two rings around it, three inside: the
     * closest pair of centres is 26 px, against 19 px needed for two 6 px bodies with their 3 px tap halo, so
     * no two targets can touch however the clock runs. SEAT_R is the drift: 2 px keeps 22 px of clearance. */
    static const int8_t SEAT_X[5] = {0, 34, 21, -20, -34};
    static const int8_t SEAT_Y[5] = {-19, -6, 17, 17, -6};
    static const int8_t SEAT_R = 2;
    static const uint8_t ORK[5] = {0, 1, 2, 3, 4};             /* rocky, banded, ocean, ringed, frozen */
    /* (4.0) FIVE TONES, NOT FIVE SHADES OF AMBER. Every body took its colour from the same point of the
     * thermal ramp (heat + index * 18), so at 240 x 240 they read as five warm discs told apart only by
     * size. Each now sits on its own part of the ramp -- the rocky and the gas worlds warm, the ringed
     * giant rose, the ocean world violet and the frozen one ice blue -- and the note still pushes a body
     * up the same ramp from there, so the music's light keeps working and the identities stop relying on
     * shape alone. */
    static const int16_t ORK_HUE[5] = {-150, -100, -30, 35, 85};   /* (3.5) spread over the ramp: 60, 110, 180,
                                                                    * 245 and 256 from a 210 base, so the five bodies
                                                                    * are five colours of the theme, not five ambers */
    int32_t kheat[NPL];
    /* fixed stars: x, y in the bay, size (1..3), luminance, twinkle phase */
    static const uint8_t ST[20][5] = {
        {4, 4, 2, 150, 11}, {13, 2, 1, 110, 47}, {31, 5, 1, 90, 83}, {46, 2, 2, 170, 29},
        {66, 6, 1, 120, 61}, {79, 13, 2, 190, 7}, {7, 19, 1, 80, 97}, {21, 15, 1, 140, 53},
        {38, 10, 1, 100, 23}, {57, 19, 2, 160, 71}, {82, 34, 1, 95, 37}, {5, 37, 1, 115, 89},
        {27, 43, 2, 175, 19}, {69, 41, 1, 105, 67},
        {12, 33, 1, 130, 41}, {44, 27, 2, 180, 59}, {71, 24, 1, 95, 73}, {84, 39, 2, 160, 29},
        {3, 46, 1, 115, 87}, {52, 44, 1, 140, 13}
    };
    /* (6.3) THE MILKY WAY. The bay's sky carried two dotted nebula bands of twelve pixels each, which at
     * 240 x 240 read as a handful of specks rather than as a galaxy, so the background never had the depth the
     * planets were tuned against. A galactic band crosses the bay now: a luminous core line that thickens into
     * a bulge where the arms turn, a halo two pixels either side, dust lanes that bite into it, and one slow
     * sine for its shimmer -- so the whole sky breathes together instead of twinkling per pixel. Positions come
     * from three tables (x, y and thickness), which is what keeps the cost at one pset per entry: no per-pixel
     * trigonometry, and the band cannot flicker because its shape never depends on the clock. */
    static const uint8_t MW_X[36] = {
        1, 3, 5, 8, 10, 13, 15, 18, 20, 23, 25, 27,
        30, 32, 35, 37, 40, 42, 45, 47, 50, 52, 54, 57,
        59, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86
    };
    static const uint8_t MW_Y[36] = {                       /* the band's centre line, rising to the right */
        49, 48, 47, 46, 45, 43, 41, 39, 36, 34, 32, 30,
        29, 28, 28, 28, 28, 28, 27, 26, 25, 23, 21, 19,
        17, 16, 14, 13, 12, 11, 11, 10, 9, 8, 7, 6
    };
    static const uint8_t MW_T[36] = {                       /* 3 = the bulge, 2 = the arm, 1 = its taper */
        2, 2, 1, 2, 2, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 2, 2, 1, 2, 2, 1,
        2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2, 1
    };
    int32_t i, j, orb_x[NPL], orb_y[NPL], orb_rot[NPL], order[NPL], big = 0;
    int32_t pk_ang[NPL] = {0}, pk_rad[NPL] = {0}, pk_push[NPL] = {0}, pk_ry[NPL];
    uint32_t tm = fm1_ms, twf = ui.frame >> 2;
    /* (2.2) THE SYSTEM IS NOT A CLOCKWORK. Every orbit ran at a constant rate for ever, so the bay
     * repeated itself exactly: the same configuration every seven and a half seconds, and an eye reads
     * that as a mechanism, not as space. One slow field -- a sine of the clock, ~47 s a turn -- now
     * stretches and compresses each orbit a little, weighted by its index, and its tilt with it: the
     * paths breathe together but never in step, and one sine pays for the whole system. */
    int32_t wob = bg_sin(tm * 3u);
    int32_t tilt_wob = bg_sin(tm * 3u + 0x2000u) >> 12;   /* +-8: the tilt must stay a few pixels */
    int32_t orb_ry[NPL];
    for (i = 0; i < NPL; i++) {                        /* the tilt the path and its planet both use */
        kheat[i] = heat + ORK_HUE[i];                  /* (4.0) this body's own tone on the ramp */
        orb_ry[i] = ORX[i] * ORYS[i] / 100 + tilt_wob * (i + 1) / 24;
        if (orb_ry[i] < ORX[i] * 30 / 100)
            orb_ry[i] = ORX[i] * 30 / 100;             /* the ellipse never closes up into a line */
        pk_ry[i] = orb_ry[i];                           /* (3.5) its rest tilt, the reaction's baseline */
    }
    /* (3.5) THE TOUCH'S GESTURE, resolved once a frame into the three offsets the draw below applies. They are
     * all zero unless a body is inside its own reaction, so an untouched bay costs one pl_age() a body. */
    for (i = 0; i < NPL; i++) {
        uint32_t pa = pl_age(i, POKE_L[i]);
        if (pa >= 64u)
            continue;
        switch (PL_POKE[i]) {
        case PK_SPIN:                               /* the orbit surges forward and settles: a bump that begins
                                                     * and ends on the path's own place, so nothing jumps */
            pk_ang[i] = (int32_t)(pa * ((uint32_t)POKE_L[i] - pa)) * 12000 /
                        ((int32_t)POKE_L[i] * (int32_t)POKE_L[i]);
            /* (3.1.5) and the body itself leans into the turn: 3 px out at the height of the surge and back,
             * which is what makes the rocky world's touch read at arm's length. The arcs alone measured 124 px
             * of ink; the body moving carries the rest. */
            pk_rad[i] = pa < 10u ? (int32_t)pa * 3 / 10
                                 : (int32_t)((uint32_t)POKE_L[i] - pa) * 3 / (int32_t)(POKE_L[i] - 10u);
            break;
        case PK_SWELL:                              /* the body inhales two pixels and lets them go */
            pk_rad[i] = pa < 8u ? (int32_t)pa * 2 / 8 : (int32_t)((uint32_t)POKE_L[i] - pa) * 2 /
                                 (int32_t)(POKE_L[i] - 8u);
            break;
        case PK_SLING:                              /* pushed out along its own radius, then drawn back */
            pk_push[i] = pa < 12u ? (int32_t)pa * 2 / 12 : (int32_t)((uint32_t)POKE_L[i] - pa) * 2 /
                                 (int32_t)(POKE_L[i] - 12u);
            if (pk_push[i]) {                       /* and its path breathes with it, half a pixel */
                orb_ry[i] += pk_push[i] / 2;
                pk_ry[i] = orb_ry[i];
            }
            break;
        }
        pk_ry[i] = orb_ry[i];
    }
    if (r < 4) r = 4;
    if (r > 7) r = 7;                                 /* (7.4) one pixel more: a star, not a sun */
    {   /* the band, under everything: core, halo and dust, with one shimmer for the whole sky */
        int32_t sh = (int32_t)((bg_sin(twf * 2u + 0x2A00u) + 32768) >> 11);   /* 0..32, a slow breath */
        /* (3.1.5) AND IT ANSWERS WHEN IT IS TOUCHED. The star's tap used to reach the bodies only; the band it
         * sits in stayed the same picture. A wave now runs out from the centre along the band, column by
         * column, brightening the core as it passes and leaving a dimmer lane behind it, so the tap reads as
         * light crossing the galaxy. One age, no state beyond the frame it was touched. */
        int32_t gw2 = -1;
        if (gal_t) {
            uint32_t ga = ui.frame - gal_t;
            if (ga < 44u)
                gw2 = (int32_t)ga;
            else
                gal_t = 0;
        }
        uint16_t mcore = ux_mix(T_BG, T_TEXT, 16 + sh / 3);
        uint16_t mhalo = ux_mix(T_BG, T_THEME, 9 + sh / 5);
        uint16_t mdust = ux_mix(T_BG, T_LINE, 7);
        uint16_t mmid = ux_mix(mhalo, mcore, 60);    /* (PHASE 1) the bulge's inner lane, mixed once a frame:
                                                      * it was mixed again for each of the three bulge columns */
        /* (FASE 4) THE BULGE HAS LIGHT, NOT ONLY LINES. Eleven rows of a soft ellipse behind the core: the
         * band was five 1 px lanes and a glow would have been one more, so the light is a gradient the band is
         * then drawn on top of. Rows, not a disc: 22 spans, no trigonometry, and it never touches the clock. */
        {
            int32_t gy, gx;
            int32_t gcx = sys_x0 + MW_X[18], gcy = sys_y0 + MW_Y[18];
            for (gy = -6; gy <= 6; gy++) {
                int32_t ay = gy < 0 ? -gy : gy;
                int32_t half = 13 - ay * 2;
                uint16_t gc = ux_mix(T_BG, bg_col(130), 6 + (6 - ay) * 3);
                for (gx = -half; gx <= half; gx++)
                    sys_pset(gcx + gx, gcy + gy, gc);
            }
        }
        for (j = 0; j < 36; j++) {
            int32_t mx = sys_x0 + MW_X[j], my = sys_y0 + MW_Y[j];
            if (MW_T[j] == 3) {                      /* the bulge: a wider, warmer core with its lanes */
                sys_pset(mx, my - 1, mhalo);
                sys_pset(mx, my + 1, mmid);
                sys_pset(mx, my - 2, mdust);
                sys_pset(mx, my + 2, mhalo);
                sys_pset(mx, my + 3, mdust);
            } else if (MW_T[j] == 2) {               /* the arms: a core with a halo either side */
                sys_pset(mx, my - 1, mhalo);
                sys_pset(mx, my + 1, mhalo);
                sys_pset(mx, my + 2, mdust);
            } else {
                sys_pset(mx, my + 1, mdust);
            }
            {   /* (3.1.5) the band's own detail: a knot every sixth column, a second lane under it, and the
                 * wave a touch sends out from the centre. All of it is one colour mixed once per column. */
                int32_t dj = j - 18;
                if (dj < 0)
                    dj = -dj;
                sys_pset(mx, my, mcore);
                if ((j % 6) == 3 && MW_T[j] >= 2)   /* the knots: one pixel deeper than the core line, which is
                                                     * already at mcore above. The old code set that same pixel
                                                     * to that same colour a second time -- six psets a frame of
                                                     * dead work (PHASE 1). */
                    sys_pset(mx, my + 1, mhalo);
                sys_pset(mx, my + 4, mdust);      /* the second lane, under the band */
                /* (7.3) TWO SPIRAL ARMS. The band was a straight rule with a halo, so the sky read as a stripe.
                 * Two lanes now leave it and come back, their offset growing with the distance from the bulge --
                 * a spiral read without one trigonometry call per pixel and without depending on the clock, so the
                 * arms are the band's shape and never a second thing to flicker. */
                if (MW_T[j] >= 2) {
                    int32_t dj2 = j > 18 ? j - 18 : 18 - j;
                    int32_t sp3 = 2 + dj2 / 4, sgn = (j & 8) ? 1 : -1;
                    sys_pset(mx, my + sgn * sp3, ux_mix(T_BG, T_THEME, 44));
                    if (sp3 < 8)
                        sys_pset(mx, my + sgn * (sp3 + 1), ux_mix(T_BG, T_LINE, 26));
                }
                if (gw2 >= 0) {
                    int32_t reach = gw2 * 3 - dj * 2;                /* the front, running outward */
                    if (reach >= 0 && reach < 20)     /* weight under 100: ux_mix past it bleeds a channel */
                        sys_pset(mx, my - 1, mhalo);                /* the front, in the band's own halo tone */
                }
            }
            /* (FASE 2) NEBULAE: three patches of dithered light on the band's own shoulder, so the sky has
             * clouds and not only a line. Seven psets each, in the palette's tokens, and their shape comes from
             * the same j as the band: no clock, nothing to flicker. */
            if (j == 7 || j == 19 || j == 29) {
                int32_t nx = mx, ny = my + ((j == 19) ? -4 : 3), r;
                for (r = 0; r < 3; r++) {
                    sys_pset(nx - 2 + r, ny, ux_mix(T_BG, T_THEME, 42 + r * 10));
                    sys_pset(nx - 1 + r, ny - 1, ux_mix(T_BG, T_LINE, 30 + r * 7));
                    sys_pset(nx - 1 + r, ny + 1, ux_mix(T_BG, T_THEME, 26 + r * 6));
                }
                sys_pset(nx, ny - 2, ux_mix(T_BG, T_TEXT, 44));
            }
            if ((j & 3) == 1 && j > 4 && j < 32)     /* the dust lanes that cross the band */
                sys_pset(mx, my - 1, mdust);
        }
    }
    {   /* (FASE 4) DUST IN THE AIR, NOT A LINE ACROSS THE SKY. The band was lanes and knots, all of them
         * tied to a column: nothing moved along it and nothing sat off it, so the "galaxy" read as a rule with
         * a glow. Eighteen motes sit above and below the core and drift along it at three speeds; their place
         * is a pure function of the clock (no pool, no state), each one is a pixel or two, and they wrap at the
         * band's own ends, so the bay breathes without a single extra byte. */
        int32_t k;
        for (k = 0; k < 18; k++) {
            uint32_t h = (uint32_t)k * 2654435761u + 0x9E3779B9u;
            int32_t span = 84;
            int32_t bx = (int32_t)(h % (uint32_t)span);
            int32_t sp = 1 + (int32_t)((h >> 9) % 3u);
            int32_t dy = (int32_t)((h >> 13) % 7u) - 3;
            int32_t px = 6 + (bx + (int32_t)(twf * (uint32_t)sp / 30u)) % span;
            int32_t bi = clamp(px * 35 / span, 0, 35);
            int32_t to = 22 + (int32_t)((h >> 17) % 36u);
            sys_pset(sys_x0 + px, sys_y0 + (int32_t)MW_Y[bi] + dy, ux_mix(T_BG, T_LINE, to));
            if (k & 1)
                sys_pset(sys_x0 + px, sys_y0 + (int32_t)MW_Y[bi] + dy + 1, ux_mix(T_BG, T_THEME, to / 2));
        }
    }
    for (i = 0; i < 20; i++) {                         /* a slow twinkle, one sine a star, a quarter rate */
        int32_t tw = (int32_t)((bg_sin((twf * (2u + (uint32_t)(i & 3u)) + ST[i][4]) << 8) + 32768) >> 11);
        int32_t x = sys_x0 + 1 + ST[i][0], y = sys_y0 + 2 + ST[i][1];
        {   /* a star inside the band is the band's light: it keeps its place, it does not double up */
            int32_t bi = clamp((x - sys_x0) * 35 / 88, 0, 35);
            if (y - sys_y0 > (int32_t)MW_Y[bi] - 3 && y - sys_y0 < (int32_t)MW_Y[bi] + 4)
                continue;
        }
        uint16_t core = ux_mix(T_BG, bg_col(150 + ST[i][3] / 3 + tw), 46 + tw);
        uint16_t halo = ux_mix(T_BG, core, 55);
        switch (ST[i][2]) {
        case 1:
            sys_pset(x, y, core);
            break;
        case 2:
            sys_pset(x, y, core);
            sys_pset(x - 1, y, halo);
            sys_pset(x + 1, y, halo);
            sys_pset(x, y - 1, halo);
            sys_pset(x, y + 1, halo);
            break;
        default:
            sys_pset(x, y, ux_mix(core, T_TEXT, 60));
            sys_pset(x - 1, y, core);
            sys_pset(x + 1, y, core);
            sys_pset(x, y - 1, core);
            sys_pset(x, y + 1, core);
            sys_pset(x - 2, y, halo);
            sys_pset(x + 2, y, halo);
            sys_pset(x, y - 2, halo);
            sys_pset(x, y + 2, halo);
            break;
        }
    }
    /* (4.0) TWO WORLDS THAT STAY. The bay's depth was dust and paths; these turn where they are, on their
     * own rates, mixed far out of the background. The outer path crosses in front of one of them, so the
     * square has a back and a front. Regulars: their place never changes, only their face. */
    gw_far_world(sys_x0 + 11, sys_y0 + 7, 2, tm, 9, 150, 0);
    gw_far_world(sys_x0 + 78, sys_y0 + 44, 2, tm, 13, 120, 0);
    /* (3.1.4) ONE PATH, NOT FIVE. Five concentric ellipses of 11..38 px put two bodies 7 px apart whenever
     * their angles lined up, and no phase offset can prevent it: with orbits this close the relative angle
     * of two neighbours sweeps every value in a few seconds. The bodies now hold five seats (below) and the
     * bay keeps a single faint path as its depth, wide enough to read as space and not as a wire frame. */
    gw_ring_semi(cx, cy, 36, 20, ux_mix(T_BG, T_LINE, 32), 1);
    /* (3.1.4) FIVE SEATS, AND EACH BODY OWNS ITS TARGET. A body rode its own ellipse, so where it sat was
     * the clock's business: two of them could be 7 px apart and the tap that landed between went to whichever
     * was drawn last. The seats are fixed in the bay and far enough apart that the discs never meet (the
     * closest pair is 26 px, and the tap reaches 3 px past each body), and each body drifts +-2 px inside its
     * own seat, on its own rate, so the bay still breathes without ever trading targets.
     * Positions stay a pure function of the clock: no frame rate, no audio level, and nothing to reset. */
    for (i = 0; i < NPL; i++) {
        uint32_t ph = (uint16_t)(tm * (uint32_t)ORMS[i] + ORPHS[i] + (uint32_t)pk_ang[i]);
        int32_t srad = SEAT_R + (pk_ang[i] ? 1 : 0);   /* (3.5) the surge widens its own little orbit */
        orb_x[i] = cx + SEAT_X[i] + (bg_sin(ph + 0x4000u) * srad >> 15);
        orb_y[i] = cy + SEAT_Y[i] + (bg_sin(ph) * srad >> 15);
        if (pk_ang[i]) {                               /* and leaves a comet trail behind it, on its seat */
            int32_t k;
            for (k = 1; k <= 3; k++) {
                uint32_t ta = (uint16_t)(ph - (uint32_t)(k * 3000));
                sys_pset(cx + SEAT_X[i] + (bg_sin(ta + 0x4000u) * srad >> 15),
                         cy + SEAT_Y[i] + (bg_sin(ta) * srad >> 15),
                         ux_mix(T_BG, bg_col(kheat[i] + 70), 120 - k * 30));
            }
        }
        if (pk_push[i]) {                               /* (3.5) the slingshot: out from the star, back */
            orb_x[i] += orb_x[i] >= cx ? pk_push[i] : -pk_push[i];
            orb_y[i] += orb_y[i] >= cy ? pk_push[i] : -pk_push[i];
        }
        /* its own spin, also a pure function of the clock: the surface phase never follows the orbit */
        orb_rot[i] = (int32_t)(uint16_t)(tm * (uint32_t)ROTMS[i] + ORPHS[i] * 3u);
        order[i] = i;
    }
    for (i = 0; i < NPL; i++)                          /* farthest (top) first, so overlaps read as depth */
        for (j = i + 1; j < NPL; j++)
            if (orb_y[order[j]] < orb_y[order[i]]) {
                int32_t t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
    for (i = 0; i < NPL; i++)                          /* (3.5) the hit test reads the same order: the body in
                                                        * front is the one a finger lands on */
        pl_order[i] = (uint8_t)order[i];
    for (i = 0; i < NPL; i++) {
        int32_t pi = order[i];
        if (orb_y[pi] >= cy)
            continue;
        if (!UI_NO_DRIFT && pl_ex[pi] > 32) {          /* a short afterglow arc under the body, not a second orbit */
            uint16_t gc = ux_mix(T_BG, bg_col(kheat[pi] + pl_ex[pi] / 3), 60);
            sys_line(orb_x[pi] - 2, orb_y[pi] + 2, orb_x[pi] + 2, orb_y[pi] + 2, gc);
        }
        if (pk_ang[pi]) {                              /* (3.5) the surge leaves a comet trail on its own path */
            int32_t k, aa = (int32_t)((uint16_t)(tm * (uint32_t)ORMS[pi] + ORPHS[pi] +
                                                 (uint32_t)(wob >> 6) * (uint32_t)(pi + 1))) + pk_ang[pi];
            for (k = 1; k <= 3; k++) {
                int32_t ta = aa - k * 1100;
                sys_pset(cx + (ORX[pi] * bg_sin((uint32_t)ta + 0x4000u) >> 15),
                         cy + (orb_ry[pi] * bg_sin((uint32_t)ta) >> 15),
                         ux_mix(T_BG, bg_col(kheat[pi] + 70), 120 - k * 30));
            }
        }
        if (pl_sel_i == (uint32_t)pi)                /* (FASE 2) the selection ring, under the body */
            gw_body_sel(orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi], kheat[pi]);
        gw_body_react(pi, orb_x[pi], orb_y[pi], kheat[pi]);
        sys_ring_lift = ORK[pi] == 3 ? pk_push[pi] : 0; /* (3.5) the giant's plane follows its push */
        gw_sphere(orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi],
                  kheat[pi] + pl_ex[pi] / 2 + pk_rad[pi] * 14,
                  orb_x[pi] < cx ? 1 : -1, orb_y[pi] < cy ? -1 : 1, ORK[pi],
                  (uint32_t)orb_rot[pi], pl_flash[pi] + pk_rad[pi] * 80);
        sys_ring_lift = 0;
        gw_body_fx(ORK[pi], orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi], kheat[pi],
                   orb_x[pi] < cx ? 1 : -1, tm);
        gw_body_life(ORK[pi], orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi], kheat[pi], tm);
        if (ORK[pi] == 1 || ORK[pi] == 4)              /* (7.3) its own moon, at its own rate */
            gw_moon(orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi], tm,
                    (uint32_t)pi * 37u + 11u, kheat[pi]);
        pl_sx[pi] = (int16_t)orb_x[pi];                /* (3.5) where it was drawn: what a tap is measured against */
        pl_sy[pi] = (int16_t)orb_y[pi];
        pl_sr[pi] = (uint8_t)(ORR[pi] + pl_ex[pi] / 96 + pk_rad[pi]);
        if (pl_ex[pi] > big) big = pl_ex[pi];
    }
    /* (4.0) the star: its core, its turning corona and its own flares (gw_star). The biggest planet's
     * note still lifts it a pixel, so the system answers as a whole. */
    gw_star(cx, cy, r, en, heat, big, tm);
    /* the near half of the one path, over the star */
    gw_ring_semi(cx, cy, 36, 20, ux_mix(T_BG, T_LINE, 52), 0);
    for (i = 0; i < NPL; i++) {
        int32_t pi = order[i], ex = pl_ex[pi];
        if (orb_y[pi] < cy)
            continue;
        if (pl_sel_i == (uint32_t)pi)
            gw_body_sel(orb_x[pi], orb_y[pi], ORR[pi] + ex / 96 + pk_rad[pi], kheat[pi]);
        gw_body_react(pi, orb_x[pi], orb_y[pi], heat + pi * 18);
        sys_ring_lift = ORK[pi] == 3 ? pk_push[pi] : 0;
        gw_sphere(orb_x[pi], orb_y[pi], ORR[pi] + ex / 96 + pk_rad[pi],
                  kheat[pi] + ex / 2 + pk_rad[pi] * 14,
                  orb_x[pi] < cx ? 1 : -1, orb_y[pi] < cy ? -1 : 1, ORK[pi],
                  (uint32_t)orb_rot[pi], pl_flash[pi] + pk_rad[pi] * 80);
        sys_ring_lift = 0;
        gw_body_fx(ORK[pi], orb_x[pi], orb_y[pi], ORR[pi] + ex / 96 + pk_rad[pi], kheat[pi],
                   orb_x[pi] < cx ? 1 : -1, tm);
        gw_body_life(ORK[pi], orb_x[pi], orb_y[pi], ORR[pi] + ex / 96 + pk_rad[pi], kheat[pi], tm);
        if (ORK[pi] == 1 || ORK[pi] == 4)
            gw_moon(orb_x[pi], orb_y[pi], ORR[pi] + ex / 96 + pk_rad[pi], tm, (uint32_t)pi * 37u + 11u, kheat[pi]);
        pl_sx[pi] = (int16_t)orb_x[pi];
        pl_sy[pi] = (int16_t)orb_y[pi];
        pl_sr[pi] = (uint8_t)(ORR[pi] + ex / 96 + pk_rad[pi]);
        if (!UI_NO_DRIFT && ex > 40) {                 /* (3.2) the note's ring: the body answers at once,
                                                        * two pixels out, and fades with its own excitation */
            int32_t rr = ORR[pi] + 2 + ex / 80;
            gw_ring_clip(orb_x[pi], orb_y[pi], rr, rr * 2 / 3,
                         ux_mix(T_BG, bg_col(kheat[pi] + ex / 2), clamp(ex, 30, 200)), 1);
        }
        if (UI_NO_DRIFT || ex <= 40)
            continue;
        {   /* the note's sparks: the same bounded pool the other visualizers draw */
            uint16_t pc = bg_col(kheat[pi] + ex / 3);
            vz_np_add(orb_x[pi], orb_y[pi], (orb_x[pi] < cx ? -2 : 2), -2, pc, 9, 1);
        }
    }
}

/* the ambient dust of a bay: deterministic, no allocation, one sine per particle per frame. It is
 * the same field the full-screen visualizer draws (graph_home_viz), moved here so there is one
 * implementation: n embers drifting in a rect, dimmed to `bg` so they read as depth and never as
 * data. MENU > VISUALIZER QUALITY thins them; PREF_ANIM_OFF stops the motion (not the field). */
static __attribute__((noinline)) void gw_embers(int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t n, uint32_t seed,
                      int32_t heat0, uint16_t bg)
{
    uint32_t i;
    int mono = ux.mono || ux.bw;
    if (!w || !h || !n)
        return;
    if (n > GW_EMBER_MAX)
        n = GW_EMBER_MAX;                           /* (3.5) the field's own ceiling, whatever a caller asks */
    for (i = 0; i < n; i++) {
        /* (3.5) three depths, not one sheet: a far grain drifts slowly and dim, a near ember faster and two
         * pixels wide, so the field has parallax. Every grain's place is a function of the frame and its
         * index (no state, no allocation) and its drift wraps inside the rect it was given. */
        uint32_t tier = i % 3u;
        int32_t spd = tier == 0u ? 26 : tier == 1u ? 48 : 78;
        int32_t amp = h / 3 + (tier == 2u ? 4 : tier == 1u ? 2 : 0);
        int32_t px = x0 + 1 + (int32_t)(((i * 11u + (i % 5u) * 7u + seed) * 37u) % (uint32_t)(w - 2));
        int32_t drift = (int32_t)(bg_sin(ui.frame * (12u + tier * 7u) + i * 2048u + seed) >> 14);
        int32_t py = y0 + h / 2 + (int32_t)((bg_sin(ui.frame * (spd + (i % 7u) * 5u) + i * 4096u + seed) * amp) >> 15);
        int32_t lvl = (int32_t)((bg_sin(ui.frame * (39u + (i % 5u) * 8u) + i * 3072u + seed) + 32768) >> 9);
        uint16_t pc;
        px += drift;
        if (px < x0 + 1)
            px += w - 2;
        else if (px > x0 + w - 2)
            px -= w - 2;
        if (py < y0 || py >= y0 + h || px < x0 || px >= x0 + w)
            continue;
        pc = mono ? (lvl > 90 ? T_TEXT : T_DIM) : ux_mix(bg, bg_col(heat0 + (int32_t)i * 7), 26 + (int32_t)tier * 7 + lvl / 6);
        cv_rect(px, py, (i % 5u) == 0u && tier == 2u ? 2 : 1, 1, pc);
    }
}

/* ============================================================================
 * 1b. THE DEEP FIELD (3.5): the bodies the visualizer sits in front of
 *    The full-screen page had one layer -- dust -- between it and the data, over the palette's own
 *    background, so it read as a panel rather than as space. Distant worlds are drawn first now, under
 *    everything the music draws: opaque by construction (every row of the disc is filled, so nothing shows
 *    through), tenuous (mixed 22..46 % out of the page's own tone, so they recede), and lit by a phase of
 *    their own rather than by HOME's star. Each carries one anomaly: a ring across a limb that is mostly
 *    outside the page, a terminator that does not pass through its centre, or a small body whose bright edge
 *    is on the side that should be dark. A 32 s tide moves them and a ~22 s one breathes their light. The
 *    music's content stays the only bright thing on the page.
 * ============================================================================ */
static __attribute__((noinline)) void gw_deep_row(int32_t cx, int32_t y, int32_t span, int32_t ph, uint16_t lit, uint16_t shade,
                        uint16_t edge)
{
    int32_t q;
    if (y < -1 || y > 241)                              /* the rows outside the page cost nothing */
        return;
    if (!span) {
        cv_pset(cx, y, lit);
        return;
    }
    /* its own terminator: a phase that is not the star's, and need not pass through its centre */
    q = -span + (2 * span * (ph & 0x7FFF)) / 32768;
    if (q > -span)
        cv_rect(cx - span, y, q + span, 1, shade);
    cv_rect(cx + q, y, span - q + 1, 1, lit);
    cv_pset(cx - span, y, edge);
    cv_pset(cx + span, y, edge);
}

/* A body of the visualizer's deep field. Same shading law as HOME's worlds -- a terminator, a lit face and
 * a rim -- but mixed far out of the page's own tone, so it sits behind the data, and with its own surface
 * marks so an enlarged disc reads as a world instead of as a bigger circle. */
static __attribute__((noinline)) void gw_deep_body(int32_t cx, int32_t cy, int32_t r, int32_t ph, uint16_t lit, uint16_t shade,
                         uint16_t edge, int32_t ring, int32_t kind)
{
    int32_t dy, span = 0;
    if (ring)                                          /* its ring first, so the body hides its middle */
        gw_ring_tab(cx, cy, r + 3, (r + 3) / 3, edge, 0);
    for (dy = -r; dy <= 0; dy++) {
        while (span < r && (span + 1) * (span + 1) + dy * dy <= r * r)
            span++;
        gw_deep_row(cx, cy + dy, span, ph, lit, shade, edge);
        if (span > 2 && kind == 1 && (dy == -(r / 3) || dy == r / 3))     /* a gas giant's belts */
            cv_rect(cx - span + 1, cy + dy, 2 * span - 1, 1, shade);
        if (span > 3 && kind == 0 && (dy == 0 || dy == r / 2))            /* a rocky world's craters */
            cv_pset(cx + span / 3, cy + dy, shade);
    }
    for (dy = 1; dy <= r; dy++) {
        while (span > 0 && span * span + dy * dy > r * r)
            span--;
        gw_deep_row(cx, cy + dy, span, ph, lit, shade, edge);
        if (span > 2 && kind == 1 && (dy == -(r / 3) || dy == r / 3))
            cv_rect(cx - span + 1, cy + dy, 2 * span - 1, 1, shade);
        if (span > 3 && kind == 0 && (dy == 0 || dy == r / 2))
            cv_pset(cx + span / 3, cy + dy, shade);
    }
}

/* (6.6) THE TWO DEEP WORLDS, AND WHAT THE TRACKS DO TO THEM. They were the same two-tone discs as HOME's bay
 * at 11 and 8 px: a terminator, one belt each, and nothing that moved unless a finger found them. Each now
 * carries its own surface and its own half of the tracks -- the giant reads tracks 1-3, the rocky world 4-6 --
 * so both answer the music whichever track is selected. A loud half lifts the lit face (the tone moves up),
 * widens the atmosphere and brings the giant's storm eye to the surface, or the rock's glint on its limb; a
 * tap does the same for twenty frames. The halo is two psets a row and one more on the lit side, the marks are
 * a few per row, and nothing here allocates, divides by anything but a constant, or reads a global it does not
 * need. */
/* (6.6) THE MILKY WAY, BEHIND THE SCOPE. 6.5 put a band in the panel: twenty-four columns, one pixel each,
 * which at 240 px read as a dotted line rather than as a galaxy. This draws the same sky at the scale the page
 * can carry: a core line that swells into a bulge near the middle, a halo two rows either side of it, a dust
 * lane biting into the lit side, and twelve stars with their own twinkle. All from tables -- no trigonometry,
 * no allocation, one pset per element -- and every tone is mixed out of the page's own background, so the
 * measured data still leads. It sits before the bodies and the trace, so the scope is in front of it. */
static __attribute__((noinline)) void gw_viz_milky(uint16_t page, int32_t g, int32_t sway, int32_t wave_x, int32_t wave_age)
{
    static const uint8_t SX[12] = {22, 47, 63, 79, 96, 108, 121, 139, 154, 171, 188, 206};
    static const uint8_t SY[12] = {149, 138, 130, 121, 110, 103, 96, 87, 78, 70, 63, 54};
    /* (FASE 2) A SECOND RING OF STARS, smaller and dimmer, behind the first: the sky had twelve dots at one
     * depth and read as a printed backdrop. These eighteen turn slower and never grow past one pixel, so the
     * band has stars in front of it and stars behind it instead of a single sheet. */
    static const uint8_t FX[18] = {9, 18, 31, 40, 55, 70, 87, 101, 116, 128, 145, 160, 176, 192, 203, 214, 224, 231};
    static const uint8_t FY[18] = {163, 172, 155, 168, 147, 140, 133, 126, 118, 112, 104, 97, 88, 80, 72, 64, 56, 47};
    uint16_t dust = bg_tone_mix(page, bg_col(4), 58);
    int32_t x, j;
    {   /* (FASE 4) the bulge, lit: the same gradient HOME's bay wears, wider here because the page is */
        int32_t gy, gx, gcy = 160 - (112 * 118) / 236 + sway;
        for (gy = -8; gy <= 8; gy++) {
            int32_t ay = gy < 0 ? -gy : gy;
            int32_t half = 26 - ay * 3;
            /* (FASE 9) the row takes one colour: the mix and the ramp were per pixel and depend only on ay */
            cv_rect(112 - half, gcy + gy, 2 * half + 1, 1, bg_tone_mix(page, bg_col(110), 5 + (8 - ay) * 2));
        }
    }
    for (x = 2; x < 236; x += 2) {                    /* the band: two px wide, thicker at the bulge */
        int32_t y = 160 - (x * 118) / 236 + sway;
        int32_t d = x > 112 ? x - 112 : 112 - x;
        int32_t t = 2 + (d < 104 ? 4 * (104 - d) / 104 : 0);
        /* (FASE 2) THE LOCALISED WAVE. Touching one of the two foreground worlds sends a front along the band
         * from that x: the core and the halo lift for a few pixels as it passes and nothing else on the sky
         * changes, so the answer is where the hand is and not a global flash. wave_age is the age of the
         * newest touch (0 = none), a pure function of the frame, so two paints of one frame agree. */
        int32_t wv = 0;
        if (wave_age > 0 && wave_age < 26) {
            int32_t wd = (x - wave_x) < 0 ? wave_x - x : x - wave_x;
            int32_t front = wave_age * 5;
            int32_t dd = wd - front;
            if (dd < 0)
                dd = -dd;
            if (dd < 7)
                wv = (7 - dd) * (26 - wave_age) / 4;
        }   /* 2 at the ends, 6 across the core */
        int32_t near = d < 104 ? 104 - d : 0;                  /* the band fades out at both ends */
        uint16_t core = bg_tone_mix(page, bg_col(170 + g * 3 + wv * 2), 30 + near * 20 / 104 + g / 3 + wv);
        uint16_t halo = bg_tone_mix(page, bg_col(110 + g * 2 + wv), 22 + near * 14 / 104 + g / 4 + wv / 2);
        uint16_t outer = bg_tone_mix(page, bg_col(60 + g), 16 + near * 10 / 104);
        cv_pset(x, y, core);
        cv_pset(x + 1, y, core);
        cv_pset(x, y + 1, halo);
        cv_pset(x + 1, y + 1, halo);
        cv_pset(x, y - 1, halo);
        if (t >= 4) {
            cv_pset(x, y + 2, outer);
            cv_pset(x + 1, y + 2, outer);
            cv_pset(x, y - 2, outer);
        }
        if (t >= 6) {
            cv_pset(x, y + 3, outer);
            cv_pset(x, y - 3, outer);
        }
        if (wv > 3) {                                 /* the front, one px above and below the band */
            cv_pset(x, y - 4, outer);
            cv_pset(x + 1, y + 4, outer);
        }
        if ((x % 14) == 6)                            /* the dust lane: what bites into the lit side */
            cv_pset(x, y + 1, dust), cv_pset(x + 1, y + 2, dust);
    }
    {   /* (FASE 4) DUST, MOVING. Twenty-two motes off the core line, each drifting along the band at its own
         * rate and fading with the band's own taper: the sky has something in it besides the band and the
         * stars, and it is a pure function of the millisecond clock, so two paints of one frame agree. */
        int32_t k;
        for (k = 0; k < 22; k++) {
            uint32_t h = (uint32_t)k * 2654435761u + 0x7F4A7C15u;
            int32_t bx = 2 + (int32_t)(h % 232u);
            int32_t sp = 1 + (int32_t)((h >> 9) % 3u);
            int32_t dy = (int32_t)((h >> 13) % 9u) - 4;
            int32_t px = 2 + (bx + (int32_t)((fm1_ms / 16u) * (uint32_t)sp / 30u)) % 232;
            int32_t py = 160 - (px * 118) / 236 + sway + dy;
            int32_t d = px > 112 ? px - 112 : 112 - px;
            int32_t to = 20 + (int32_t)((h >> 19) % 30u) + (d < 90 ? (90 - d) / 6 : 0);
            cv_pset(px, py, bg_tone_mix(page, bg_col(120 + (int32_t)((h >> 7) % 60u)), to));
            if (k & 3)
                cv_pset(px, py + 1, bg_tone_mix(page, bg_col(90), to / 2));
        }
    }
    for (j = 0; j < 18; j++) {                        /* (FASE 2) the far ring: dim, small, its own slow twinkle */
        int32_t tw = (int32_t)((bg_sin(fm1_ms + (uint32_t)j * 2311u + 9000u) + 32768) >> 15);   /* 0..1 */
        cv_pset(FX[j], FY[j] + sway / 2, bg_tone_mix(page, bg_col(150 + tw * 70), 34 + tw * 14));
    }
    /* (FASE 2) THREE NEBULAE in the band's own shoulder: dithered light, no clock, so the sky has clouds as
     * well as a line. Nine psets each, on the band's own y at that x. */
    for (j = 0; j < 3; j++) {
        static const uint8_t NX[3] = {58, 118, 186};
        int32_t nx = NX[j], ny = 160 - (nx * 118) / 236 + sway + (j == 1 ? -4 : 3), k;
        for (k = 0; k < 3; k++) {
            cv_pset(nx - 2 + k, ny, bg_tone_mix(page, bg_col(170), 30 + k * 8));
            cv_pset(nx - 1 + k, ny - 1, bg_tone_mix(page, bg_col(120), 22 + k * 6));
            cv_pset(nx - 1 + k, ny + 1, bg_tone_mix(page, bg_col(120), 18 + k * 6));
        }
    }
    for (j = 0; j < 12; j++) {                        /* twelve stars, each with its own twinkle */
        int32_t tw = (int32_t)((bg_sin(fm1_ms * 2u + (uint32_t)j * 4369u) + 32768) >> 14);   /* 0..4 */
        cv_pset(SX[j], SY[j] + sway, bg_tone_mix(page, bg_col(190 + tw * 16), 44 + tw * 6));
        if (tw >= 3)
            cv_pset(SX[j] + 1, SY[j] + sway, bg_tone_mix(page, bg_col(150), 30));
    }
}

static __attribute__((noinline)) void gw_viz_world(uint16_t page, int32_t cx, int32_t cy, int32_t r, int32_t ph, int32_t kind,
                         int32_t heat, int32_t en, uint32_t pulse)
{
    int32_t dy, span = 0, span_any = 0;
    int32_t lift = en / 4 + (pulse && pulse < 20u ? (int32_t)(20u - pulse) : 0);
    uint16_t lit = bg_tone_mix(page, bg_col(heat + lift), 26 + en / 8);
    uint16_t shade = bg_tone_mix(page, bg_col(heat - 40 + lift / 2), 40 + en / 10);
    uint16_t edge = bg_tone_mix(page, bg_col(200 + lift), 50 + en / 12);
    uint16_t halo = bg_tone_mix(page, bg_col(heat + 30), 18 + en / 10);
    uint16_t halolit = bg_tone_mix(page, bg_col(heat + 90), 26 + en / 8);
    uint16_t cap = bg_tone_mix(page, bg_col(heat + 120), 30 + en / 12);
    int32_t loud = en > 14 || (pulse && pulse < 20u);
    /* (FASE 9) the glow and the moon's shadow take tones that are fixed for the whole body: they were mixed
     * again on every row, or on every pixel of the shadow, for the same result */
    uint16_t glow1 = bg_tone_mix(page, bg_col(heat + 40), 10 + en / 12);
    uint16_t glow2 = bg_tone_mix(page, bg_col(heat + 60), 7 + en / 14);
    uint16_t moon1 = bg_tone_mix(page, bg_col(heat - 60), 46);
    uint16_t moon2 = bg_tone_mix(page, bg_col(heat - 40), 40);
    if (kind == 1)                                    /* the giant's ring, before the body hides it */
        gw_ring_tab(cx, cy, r + 3, (r + 3) / 3, edge, 0);
    for (dy = -r; dy <= r; dy++) {
        int32_t q;
        while (span > 0 && span * span + dy * dy > r * r)
            span--;
        while (span < r && (span + 1) * (span + 1) + dy * dy <= r * r)
            span++;
        if (!span)
            continue;
        span_any = 1;
        q = -span + (2 * span * (ph & 0x7FFF)) / 32768;
        /* the atmosphere, one px off the limb and two more on the lit side */
        if (cy + dy > 27 && cy + dy < 201) {
            cv_pset(cx - span - 1, cy + dy, halo);
            cv_pset(cx + span + 1, cy + dy, halo);
            if (q >= 0) {
                cv_pset(cx + span + 2, cy + dy, halolit);
                if (q > span / 2)
                    cv_pset(cx + span + 3, cy + dy, halolit);
            }
        }
        gw_deep_row(cx, cy + dy, span, ph, lit, shade, edge);
        if (dy > r / 2)                                    /* limb darkening: the underside turns away */
            cv_pset(cx + span, cy + dy, shade);
        /* (FASE 2) THE OUTER GLOW: a second, dimmer ring two px off the lit limb, so the body sits in its own
         * light instead of being cut out of the sky with a single halo line. */
        if (cy + dy > 27 && cy + dy < 201 && q >= 0 && span > 2) {
            cv_pset(cx + span + 4, cy + dy, glow1);
            if (q > span / 2)
                cv_pset(cx + span + 5, cy + dy, glow2);
        }
        if (span < 3)
            continue;
        /* (FASE 2) ITS OWN MOON'S SHADOW crossing the face: an ellipse that walks with the spin, so the surface
         * turns under something instead of only shading from the limb. Rocky worlds get a second crater. */
        if (kind == 1 && q > -span && q < span) {
            int32_t sx2 = q - span / 2, sy2 = dy + r / 3;
            if (sx2 * sx2 / 4 + sy2 * sy2 / 2 <= r * r / 9) {
                cv_pset(cx + sx2, cy + dy, moon1);
                cv_pset(cx + sx2 + 1, cy + dy, moon2);
            }
        }
        if (kind == 1) {                              /* five belts, a lit crest on each */
            int32_t ad = dy < 0 ? -dy : dy;
            if (ad == r / 2 || ad == r / 4 || dy == r / 3 || dy == 2 * r / 3) {
                cv_rect(cx - span + 1, cy + dy, 2 * span - 1, 1, shade);
                cv_pset(cx + span - 2, cy + dy, lit);
            }
        } else if (dy == -r + 1 || dy == -r + 2) {    /* the polar cap */
            cv_rect(cx - span + 2, cy + dy, 2 * span - 3, 1, cap);
        } else if (dy == r / 4) {                     /* a crater: its shadow, its lit rim */
            cv_rect(cx - span / 3, cy + dy, 2, 1, shade);
            cv_pset(cx - span / 3 + 2, cy + dy, lit);
        } else if (dy == -r / 4) {
            cv_rect(cx + span / 4, cy + dy, 2, 1, shade);
            cv_pset(cx + span / 4 + 2, cy + dy, lit);
        }
    }
    if (kind == 0 && span_any) {                      /* the rock's specular, and its second crater */
        int32_t sx = cx + r / 2, sy = cy - r / 3;
        cv_pset(sx, sy, bg_tone_mix(page, bg_col(240), 62 + en / 5));
        cv_pset(sx - 1, sy + 1, bg_tone_mix(page, bg_col(210), 40 + en / 6));
    }
    if (kind == 1) {                                  /* the specular on its lit shoulder: volume */
        int32_t sx = cx + r / 2, sy = cy - r / 2;
        cv_pset(sx, sy, bg_tone_mix(page, bg_col(240), 54 + en / 5));
        cv_pset(sx - 1, sy, bg_tone_mix(page, bg_col(216), 38 + en / 6));
        cv_pset(sx, sy - 1, bg_tone_mix(page, bg_col(216), 38 + en / 6));
    }
    if (kind == 1 && loud) {                          /* the storm eye, while that half plays */
        int32_t ex = cx + r / 3, ey = cy + r / 4;
        cv_rect(ex - 2, ey, 5, 1, shade);
        cv_rect(ex - 1, ey + 1, 3, 1, shade);
        cv_pset(ex, ey, edge);
        cv_pset(ex + 1, ey + 1, edge);
    } else if (kind == 0 && loud) {                   /* the glint on the rock's lit limb */
        int32_t gx = cx + r - 3, gy = cy - r / 3;
        cv_pset(gx, gy, bg_tone_mix(page, bg_col(240), 88));
        cv_pset(gx - 1, gy, bg_tone_mix(page, bg_col(220), 56));
        cv_pset(gx, gy + 1, bg_tone_mix(page, bg_col(220), 56));
    }
}

/* THE TWO DEEP WORLDS ANSWER THE HAND. They were two 7 px and 5 px discs with no surface and no gesture, so
 * the field behind the scope was wallpaper. They keep HOME's own bodies (a banded giant and a rocky world,
 * shaded by the same terminator law) at a larger size and mixed darker, and each remembers the frame it was
 * touched in: a ring leaves its limb and a couple of motes drift off it, then it is still again. The state is
 * the frame of the touch, like HOME's, so a second tap interrupts the first and nothing accumulates. */
static int16_t vzb_x[2], vzb_y[2], vzb_r[2];
static uint32_t vzb_t[2];

static __attribute__((noinline)) void gw_viz_poke(int32_t i)
{
    if (i < 0 || i > 1 || !vzb_r[i])
        return;
    vzb_t[i] = ui.frame | 1u;
    ui.force = 1;
}

static int32_t gw_viz_hit(int32_t x, int32_t y)
{
    int32_t i;
    for (i = 1; i >= 0; i--) {
        int32_t dx = x - vzb_x[i], dy = y - vzb_y[i], r = (int32_t)vzb_r[i] + 4;
        if (vzb_r[i] && dx * dx + dy * dy <= r * r)
            return i;
    }
    return -1;
}

#ifdef VIZ_PROF
/* (temporary) where the deep field's time goes. Never compiled into a release build. */
#include <time.h>
double vprof[6], vprof_n;
#define VPROF_T(i, expr) do { clock_t t0_ = clock(); expr; vprof[i] += (double)(clock() - t0_) / (double)CLOCKS_PER_SEC * 1e9; } while (0)
#else
#define VPROF_T(i, expr) expr
#endif
static __attribute__((noinline)) void gw_viz_deep(uint32_t q)
{
#ifdef VIZ_PROF
    vprof_n += 1.0;
#endif
    uint16_t page = cv_bg;
    int32_t sway = q >= 2u ? 0 : (int32_t)(bg_sin(fm1_ms * 2u) >> 14);      /* +-2 px, one 32 s tide */
    int32_t g = (int32_t)((bg_sin(fm1_ms * 3u) + 32768) >> 12);           /* 0..16: the field's own light */
    uint16_t lit = bg_tone_mix(page, bg_col(150 + g), 28 + g / 2);
    uint16_t sh = bg_tone_mix(page, bg_col(58 + g), 42 + g / 2);
    uint16_t ed = bg_tone_mix(page, bg_col(200), 52);
    if (q >= 2u) {
        vzb_r[0] = vzb_r[1] = 0;                       /* LOW: nothing to hit, nothing to draw */
        return;
    }
    {
        int32_t wx = 0, wa = 0;
        uint32_t a0 = vzb_t[0] && ui.frame - vzb_t[0] < 26u ? ui.frame - vzb_t[0] : 0u,
                 a1 = vzb_t[1] && ui.frame - vzb_t[1] < 26u ? ui.frame - vzb_t[1] : 0u;
        if (vzb_t[0] && a0 && (!a1 || a0 <= a1)) { wx = vzb_x[0]; wa = (int32_t)a0; }
        else if (vzb_t[1] && a1) { wx = vzb_x[1]; wa = (int32_t)a1; }
        VPROF_T(0, gw_viz_milky(page, g, sway, wx, wa));
    }
    /* the giant's limb in the page's top-left corner: most of it outside the screen, its ring crossing the part
     * we do see, so the frame reads as a window onto something larger than it is */
    VPROF_T(1, gw_deep_body(-16 + sway / 2, 2, 34, 26000, lit, sh, ed, 0, 2));
    /* THE TWO WORLDS. A banded giant with a ring and a rocky moon: HOME's own bodies, larger and mixed
     * darker so they read behind the data. Their faces turn with the clock (a pure function of fm1_ms, never
     * an accumulator) and each answers a tap in its own degree. */
    {
        /* each world reads its own half of the tracks: 1-3 play the giant, 4-6 the rock */
        int32_t ax = 34 + sway, ay = 143 - sway / 2, bx = 214 - sway, by = 141 + sway / 3;
        int32_t en_lo = 0, en_hi = 0, k;
        for (k = 0; k < NTRK && k < (int32_t)trk_live; k++) {
            int32_t e = trk[k].peak > 0 ? clamp(trk[k].peak >> 8, 0, 127) : 0;
            if (k < NTRK / 2) {
                if (e > en_lo)
                    en_lo = e;
            } else if (e > en_hi)
                en_hi = e;
        }
        VPROF_T(2, gw_viz_world(page, ax, ay, 13, (int32_t)((fm1_ms * 7u) & 0x7FFFu), 1, 150, en_lo,
                     vzb_t[0] && ui.frame - vzb_t[0] < 26u ? ui.frame - vzb_t[0] : 0u));
        vzb_x[0] = (int16_t)ax;
        vzb_y[0] = (int16_t)ay;
        vzb_r[0] = 13;
        if (q == 0u) {
            VPROF_T(3, gw_viz_world(page, bx, by, 10, (int32_t)((fm1_ms * 5u + 16000u) & 0x7FFFu), 0, 62, en_hi,
                         vzb_t[1] && ui.frame - vzb_t[1] < 26u ? ui.frame - vzb_t[1] : 0u));
            vzb_x[1] = (int16_t)bx;
            vzb_y[1] = (int16_t)by;
            vzb_r[1] = 10;
        } else {
            vzb_r[1] = 0;
        }
        /* and a low arc in the strip under the panel: the field does not end at its border */
        VPROF_T(4, gw_deep_body(200, 238, 18, 8000, bg_tone_mix(page, bg_col(120), 22), bg_tone_mix(page, bg_col(40), 34), ed, 0, 2));
    }
    {   /* the touch's answer, gentler than HOME's: one ring out of the limb and a mote or two */
        uint32_t i, age;
        int32_t rr;
        for (i = 0; i < 2u; i++) {
            if (!vzb_r[i] || !vzb_t[i])
                continue;
            age = ui.frame - vzb_t[i];
            if (age >= 26u)
                continue;
            rr = (int32_t)vzb_r[i] + 2 + (int32_t)age / 2;
            gw_ring_tab(vzb_x[i], vzb_y[i], rr, rr / 3,
                        bg_tone_mix(page, bg_col(196), 104 - (int32_t)age * 3), 1);
            if ((age & 3u) == 0u)
                cv_pset(vzb_x[i] + (int32_t)(age & 7u) - 3,
                        vzb_y[i] - (int32_t)vzb_r[i] - 2 - (int32_t)(age >> 2),
                        bg_tone_mix(page, bg_col(214), 126 - (int32_t)age * 4));
        }
    }
}

/* ============================================================================
 * 2. WRAITH 6-BAND CHARACTER RIBBON & 5-BAND VOWEL RIBBON
 *    (Ported from CharacterRibbon & VowelSelector in PluginEditorWraith.cpp)
 * ============================================================================ */
static __attribute__((noinline)) void gw_wraith_glyph(int32_t idx, int32_t cx, int32_t cy, uint16_t col)
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

static __attribute__((noinline)) void gw_draw_wraith_ribbon(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h)
{
    static const char *const CHAR_N[6] = {"FLU", "WHI", "VOX", "CHR", "BEL", "SCR"};
    static const char *const VOW_N[5]  = {"OO", "OH", "AH", "EH", "EE"};
    static const uint8_t CHAR_HEAT[6]  = {36, 68, 128, 96, 176, 232};
    uint32_t eng = t->eng_req % NENGINES;
    int is_voice = str_eq(ENGINES[eng]->name, "VOICE");
    int is_drum = str_eq(ENGINES[eng]->name, "DRUM");
    int is_vowel = is_voice || (t->p[P_FTYPE] >= FT_VOWEL && t->p[P_FTYPE] <= FT_SCREAM);
    int32_t nb = is_vowel ? 5 : 6, bw = w / nb, i;
    int32_t cut = gw_trk_cut(t);
    int32_t lfo_mod = (t->p[P_LD_FLT] * mulq15(t->lfo_val, t->lfo_fade)) >> 14;
    int32_t morph = cut;
    if (is_voice) {
        int32_t v0 = t->p[P_E0], talk = (t->p[P_E2] > 0 && bg_energy() > 8) ? ((bg_sin(ui.frame * 900u) + 32768) >> 9) : 0;
        morph = v0 + (((t->p[P_E1] - v0) * talk) >> 7);
    } else if (is_drum) {
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
    case 6: /* (FASE 3) FT_NOTCH: the cutoff taken out, the band beside it passes */
        if (ad < 22) att = (22 - ad) * (h_max / 2) / 22;
        res_pk = 0;
        break;
    case 7: /* (FASE 3) FT_SHELF: the bass up, the top where it was */
        if (d < 0) res_pk = 0;
        else att = (h_max / 4) * (ad > 24 ? 24 : ad) / 24;
        break;
    default: /* LP: low-pass roll-off above cutoff */
        if (d > 0) att = (d * d) * h_max / (w * 18 + 1);
        break;
    }
    return clamp(y_zero - res_pk + att, y_zero - h_max / 2, y_zero + h_max / 2);
}

/* (FASE 8) THE MINI KNOB: A ROUND DIAL WITH A LIT RIM. Three attempts stacked rings and painted the value
 * as single pixels; at r = 9 that left stair steps, a lone pixel sticking out at 3 and 9 o'clock (the row at
 * dy = 0 is one wider than the row above it, which reads as a spike) and an arc that bulged outside the
 * bezel. This one keeps the scanline fill -- one `cv_rect` a row, so a dial is 23 fills -- but computes each
 * row's edge with the exact half-chord and blends the LAST pixel of the row toward the ground by the fraction
 * of it the circle actually covers: the silhouette has no spike and no step. The rim is a vertical ramp (lit
 * at the top, shaded at the foot), the face is a second ramp inside it, the value runs as a continuous arc
 * INSIDE the rim so it never leaves the outline, and the needle stops at the face's edge. Every colour comes
 * from ux_mix() on palette tokens, so GREY and MONO stay neutral. */
static int32_t gw_isqrt(int32_t v)
{
    int32_t t = 0;
    if (v <= 0)
        return 0;
    while ((t + 1) * (t + 1) <= v)
        t++;
    return t;
}
static __attribute__((noinline)) void gw_mini_bakelite_knob(int32_t cx, int32_t cy, int32_t r, int32_t val256, uint16_t arc_col)
{
    int32_t v = clamp(val256, 0, 256), dy, k;
    int32_t ri = r > 5 ? r - 3 : (r > 2 ? r - 2 : 1);      /* the face, inside the rim band */
    int mono = ux.mono || ux.bw;
    uint16_t out = mono ? T_BG : ux_mix(T_BG, T_TEXT, 14);              /* the outline: separates it from the bay */
    uint16_t rim_hi = mono ? T_DIM : ux_mix(ux_mix(T_BG, bg_col(128), 55), arc_col, 30);
    uint16_t rim_lo = mono ? T_BG : ux_mix(T_BG, rim_hi, 62);
    uint16_t face_hi = mono ? T_TEXT : ux_mix(T_BG, bg_col(88), 26);
    uint16_t face_lo = mono ? T_DIM : ux_mix(T_BG, T_LINE, 22);
    for (dy = -r; dy <= r; dy++) {
        int32_t yy = cy + dy, chord = r * r - dy * dy, xo, xi = -1, t2, frac;
        uint16_t rimc, facec, e;
        if (chord <= 0)
            continue;
        xo = gw_isqrt(chord);                             /* the exact half-chord of this row */
        if (xo <= 0)
            continue;
        t2 = (dy + r) * 100 / (2 * r);
        rimc = ux_mix(rim_hi, rim_lo, t2);
        facec = ux_mix(face_hi, face_lo, t2);
        if (ri * ri > dy * dy)
            xi = gw_isqrt(ri * ri - dy * dy);
        if (xi > 0)
            cv_rect(cx - xi, yy, 2 * xi + 1, 1, facec);
        if (xi >= 0) {                                    /* the rim band, both sides */
            cv_rect(cx - xo, yy, xo - xi, 1, rimc);
            cv_rect(cx + xi + 1, yy, xo - xi, 1, rimc);
        } else {
            cv_rect(cx - xo, yy, 2 * xo + 1, 1, rimc);
        }
        /* the row's two outer pixels carry the part of the circle that actually lands on them */
        frac = (chord - xo * xo) * 100 / (2 * xo + 1);
        e = frac < 100 ? ux_mix(T_BG, out, frac < 12 ? 12 : frac) : out;
        cv_pset(cx - xo, yy, e);
        cv_pset(cx + xo, yy, e);
        /* the rim's lit side: the upper third, a shade above its own row, so the dial has depth */
        if (dy <= -r / 3 && xi >= 0) {
            cv_rect(cx - xo + 1, yy, xo - xi - 1, 1, ux_mix(rimc, T_TEXT, 18));
            cv_rect(cx + xi + 1, yy, xo - xi - 1, 1, ux_mix(rimc, T_TEXT, 18));
        }
    }
    /* the value: a continuous arc on the rim, from the bottom left, 270 degrees, drawn only up to the value.
     * Two passes -- the rim's inner row and one outside it -- so it reads as a stroke and not as dots. */
    /* (FASE 4) THE PROGRESS POINTS WHERE THE DIAL POINTS. It was two pixels of arc on the rim, swept from
     * the bottom left: at the bottom of the range that read as a blob on the left of the dial and from the
     * middle up as "the left half lit", while the needle pointed somewhere else. Now the whole band from the
     * sweep's start to the value is filled -- face and rim -- so the lit part of the dial ends exactly at the
     * needle, and turning the knob one way moves the edge of the fill the same way. The steps are 1/256 of
     * the sweep, which is finer than a pixel at this radius: no gaps, and no clip stack. */
    for (k = 0; k <= 256; k++) {
        int32_t a256 = -384 + 768 * k / 256;
        /* (a neutral palette has no accent to spare: the fill takes the ink, which is what the needle uses) */
        uint16_t rim_fill = mono ? T_TEXT : arc_col;
        uint16_t face_fill = ux_mix(face_lo, rim_fill, mono ? 45 : 62);
        uint32_t a;
        int32_t rr;
        if (a256 > -384 + v * 768 / 256)
            break;
        a = (uint32_t)(a256 * 64);
        for (rr = r; rr > 0; rr--) {
            /* inside the dial, never outside the outline: a point the circle rounds out of it is dropped,
             * which is what kept the arc hanging off the left side of the dial */
            int32_t ax = (rr * (int32_t)bg_sin(a)) >> 15, ay = -((rr * (int32_t)bg_sin(a + 0x4000u)) >> 15);
            int32_t d2 = ax * ax + ay * ay;
            if (d2 > r * r)
                continue;
            cv_pset(cx + ax, cy + ay, d2 > ri * ri ? rim_fill : face_fill);
        }
    }
    /* the needle, from the centre to the face's edge, and the centre's dot */
    {
        int32_t nr = ri > 2 ? ri - 1 : 1;
        uint32_t th = (uint32_t)((-384 + v * 768 / 256) * 64);
        int32_t nx = cx + ((nr * (int32_t)bg_sin(th)) >> 15), ny = cy - ((nr * (int32_t)bg_sin(th + 0x4000u)) >> 15);
        cv_line(cx, cy, nx, ny, T_TEXT);
        cv_pset(nx, ny, mono ? T_TEXT : arc_col);
    }
    cv_pset(cx, cy, mono ? T_TEXT : T_ACCENT);
}

#define WF_BANDS 28                                  /* the bay's bands (the count the manual names) */
#define WF_SLICE 4                                   /* samples a band measures: a peak, not one sample */

/* (7.1) THE BANDS DO NOT TRAVEL. `wf_shift()` moved the 28 bands sideways by up to +-14 positions, driven by
 * `bg_knob(0, 0)` -- the picture drifted across the panel with a value the player did not set as a position,
 * and at the edges the bands wrapped around. KNOB 1 shapes the graph through the filter's cutoff and the
 * bars' own ceiling (gw_filter_resp_y), which is where the player expects to see it; nothing here moves the
 * bands along x. */
static uint8_t wf_lvl[WF_BANDS];
static void wf_advance(void)
{
    static uint32_t f0 = 0xFFFFFFFFu;
    uint32_t fr = vz_qframe();
    int32_t i;
    if (fr == f0)
        return;
    f0 = fr;
    for (i = 0; i < WF_BANDS; i++) {
        int32_t band = i, j, s = 0, t, l;             /* (7.1) band i, never i + a shift */
        uint32_t idx;
        idx = (scope_w - 128u + (uint32_t)band * WF_SLICE) & (SCOPE_N - 1u);
        for (j = 0; j < WF_SLICE; j++) {
            int32_t v = scope_buf[(idx + (uint32_t)j) & (SCOPE_N - 1u)];
            if (v < 0)
                v = -v;
            if (v > s)
                s = v;
        }
        t = clamp(s * 255 / 9000, 0, 255);           /* the meters' own scale (255: a uint8_t keeps it) */
        l = wf_lvl[i];
        /* the release lands ON the target: a quarter of the way each frame, and the last few levels at
         * once. Stopping at a quarter left a band up to three levels above its own signal, so the same
         * audio drew a bar a pixel or two taller after the knob had moved the bands around. */
        wf_lvl[i] = (uint8_t)(t >= l ? t : (l - t > 4 ? l - ((l - t + 3) >> 2) : t));
    }
}

static __attribute__((noinline)) void gw_draw_forge_bay(const track_t *t, int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t en)
{
    int32_t gx0 = x0 + 4, gy0 = y0 + 4, gw = w - 28, gh = h - 8;
    int32_t y_zero = gy0 + gh * 2 / 5, cut = gw_trk_cut(t), res = gw_trk_res(t);
    int32_t ftype = clamp(t->p[P_FTYPE], 0, 5);
    /* (7.2) THE CURVE IS THE KNOB'S, NOT THE LFO'S. This drew `cut + P_LD_FLT * lfo * lfo_fade`, and the track's
     * LFO ticks in the audio callback whether or not a note sounds, so the whole picture (curve, node, cyan
     * envelope) swept left and right for ever with no hand on a knob -- at the factory depth of 10, a fifth of
     * the bay's width. That is the movement the player reported. The cutoff drawn is the parameter KNOB 1 (and
     * the FILTER page's CUT) sets; the modulation is still shown, as the cyan envelope's static span (mod_span,
     * below), which is where a depth belongs: a range, not a playhead. */
    int32_t cut_x = 10 + clamp(cut, 0, 127) * (gw - 20) / 127;
    int32_t mod_span = (t->p[P_LD_FLT] < 0 ? -t->p[P_LD_FLT] : t->p[P_LD_FLT]) * (gw - 20) / 256;
    int32_t i, x, py = y_zero, mpy = y_zero, gy;

    gw_oct_panel(x0, y0, w, h, 4, T_BG, T_LINE, 1, T_THEME);

    /* 0. The bay's dust, first: behind the grid, the bars, both curves and the trace. It is the same
     *    gw_embers() HOME's system sits in, thinned by VISUALIZER QUALITY, and it never moves with the
     *    audio -- it is the room the graph is drawn in, not a measurement. */
    gw_embers(x0 + 2, y0 + 2, w - 4, h - 4, ui_viz_q >= 2u ? 7u : ui_viz_q == 1u ? 10u : 16u, 0x2B00u, 70, T_BG);

    /* (7.1) the curve sits on the parameter, not a third of the way to it each frame: a timer must never
     * move the picture on its own, and a step from the knob is what the hand did, not a glitch. */

    /* 1. The scale: three logarithmic divisions and the horizon, dotted every third row so they read as a
     *    grid and never compete with the data. A third of the pixels of a solid line. */
    for (i = 1; i <= 3; i++)
        for (gy = gy0; gy < gy0 + gh; gy += 3)
            cv_rect(gx0 + i * gw / 4, gy, 1, 1, ux_mix(T_SURF, T_GRID, 62));
    cv_rect(gx0, y_zero, gw, 1, ux_mix(T_SURF, T_LINE, 72));
    cv_rect(gx0, gy0 + gh * 3 / 4, gw, 1, ux_mix(T_SURF, T_GRID, 58));

    /* 1b. (3.5) THE BACKGROUND TRACE, UNDER THE DATA. It used to be drawn after the spectrogram, so its line
     *     and its two pixels of glow crossed the bands and the filter curve and read as a rendering fault: a
     *     flat line over the graph in silence, a hair of the wrong colour where the two met. It is the bay's
     *     back layer now -- drawn before the floor, the bands and both curves, so it can only be behind them --
     *     two pixels thick along the theme's own ramp, and it never moves the picture it sits in. */
    /* (FASE 3) THE FILL GOES UNDER THE SIGNAL. The floor below and the spectrogram after it are the bay's
     * ground, so the trace is drawn over both of them (see below, after the bands): HOME's oscilloscope was
     * being covered by its own gradient. */

    /* (7.3) THE FLOOR UNDER THE CURVE. The bay is where the player reads the filter, and the response was a line
     * on a grid: the area between it and the bay's floor is filled with the console's own ramp -- four bands, the
     * accent rising downwards, from the theme's gold to its accent -- so the curve has a body and the picture has
     * depth without one extra state. It goes before the bars and both curves, so nothing it covers is data; the
     * columns follow vz_step(), and every tone is mixed once per column rather than once per pixel. */
    if (ui.home) {   /* (7.3) HOME only: EDIT > FILTER draws this same bay and a filter's graph keeps its own
                      * representation -- a washed floor there would sit under the shape the page is read for. */
        int32_t st = (int32_t)vz_step(), xf;
        uint16_t f0 = ux_mix(T_BG, T_THEME, 8), f1 = ux_mix(T_BG, T_ACCENT, 15);   /* (FASE 4) half the ink: the
                                           * floor keeps its depth and stops reading as a band at rest */
        for (xf = 0; xf < gw; xf += st) {
            int32_t cy2 = clamp(gw_filter_resp_y(ftype, xf, gw, cut_x, res, y_zero, gh), gy0 + 2, gy0 + gh - 2);
            int32_t bot = gy0 + gh - 1, hgt = bot - cy2, k2;
            /* six steps, and every other column starts one pixel lower: the seams then land on different rows
             * from one column to the next, which is the dither a 16-bit panel had -- a blend, not three blocks. */
            for (k2 = 0; k2 < 3 && hgt > 3; k2++) {
                int32_t ya = cy2 + hgt * k2 / 3 + (k2 ? (xf & 1) : 0), yb = cy2 + hgt * (k2 + 1) / 3;
                if (yb <= ya)
                    continue;
                cv_rect(gx0 + xf, ya, st, yb - ya, ux_mix(f0, f1, 8 + k2 * 30));
            }
        }
    }



    /* 2. THE SPECTROGRAM (28 bands). Fixed geometry, a peak measured inside each band's own slice, and a
     *    five-stop thermal body -- deep violet at the foot, red, orange, gold, solar yellow at the crest --
     *    so a bar carries volume instead of three flat thirds. The loud bands keep a 1 px ember either side. */
    wf_advance();
    {
        static const int16_t ST5[5] = {64, 124, 172, 214, 244};    /* violet, red, orange, gold, solar */
        static const uint8_t MIX5[5] = {70, 76, 82, 88, 92};
        int32_t step = (gw - 4) / WF_BANDS;
        for (i = 0; i < WF_BANDS; i++) {
            int32_t bx = gx0 + 2 + i * (gw - 4) / WF_BANDS, bw = step - 1;
            int32_t env_y = gw_filter_resp_y(ftype, i * gw / WF_BANDS, gw, cut_x, res, y_zero, gh);
            int32_t max_h = clamp(gy0 + gh - env_y, 3, gh - 3);
            int32_t bh = clamp(3 + wf_lvl[i] * (max_h - 3) / 255, 3, max_h);
            int32_t top_y = gy0 + gh - bh, hh[5], k, yy = gy0 + gh;
            if (bw < 1)
                bw = 1;
            for (k = 0; k < 5; k++)
                hh[k] = bh / 5;
            hh[4] += bh - 5 * (bh / 5);
            for (k = 0; k < 5; k++) {
                yy -= hh[k];
                if (hh[k] > 0)
                    cv_rect(bx, yy, bw, hh[k], ux_mix(T_BG, bg_col(ST5[k]), MIX5[k]));
            }
            cv_rect(bx, top_y, bw, 1, bg_col(252));               /* the crest: the band's peak, defined */
            /* (FASE 2) THE TIP: a second row above the crest, half its weight, so a quiet band still shows a
             * defined top instead of a single floating pixel -- and, unlike a held peak, it is a function of
             * this frame's level alone: KNOB 1 back at its value is the same picture, which ui_test checks. */
            if (top_y - 1 > gy0 + 1)
                cv_rect(bx, top_y - 1, bw, 1, ux_mix(T_BG, bg_col(220), 86));
            if (wf_lvl[i] > 168) {                                /* a loud band keeps an ember either side */
                uint16_t ec = ux_mix(T_BG, bg_col(238), 42);
                cv_rect(bx - 1, top_y + 1, 1, bh / 3, ec);
                cv_rect(bx + bw, top_y + 1, 1, bh / 3, ec);
            }
        }
    }

    /* 2b. (FASE 3) THE TRACE, ON TOP OF THE GROUND. It is drawn after the floor's gradient and the 28 bands, so
     * nothing paints over the oscilloscope, and it carries a 3 px dark halo of its own: over the floor's light
     * end (GREY, PAPER, MONO) a two-pixel trace of a pale palette colour was the same tone as the fill it sat
     * on. The halo is the bay's own black, so the signal reads as a line in every palette and the fill keeps
     * its depth. */
    gw_live_trace(gx0 + 2, gw - 4, gy0 + gh / 2, gh / 3, 64);

    /* 3. Cyan Modulation Envelope Curve (drawFilterCurve pre-FX/mod overlay) */
    for (x = 0; x < gw; x += (int32_t)vz_step()) {      /* (v1.5) its sampling step: 1 / 2 / 3 px */
        int32_t mod_cut = clamp(cut_x + (mod_span > 4 ? mod_span : 14), 8, gw - 8);
        int32_t my = clamp(gw_filter_resp_y(ftype, x, gw, mod_cut, res / 2, y_zero + 3, gh), gy0 + 2, gy0 + gh - 2);
        if (x)
            cv_line_a(gx0 + x - (int32_t)vz_step(), mpy, gx0 + x, my, T_ACCENT, CV_A_MOD, 1);   /* (1.3.5) lightest */
        mpy = my;
    }

    /* 5. Glowing #f27121 Orange Filter Frequency Response Curve (2px thick) */
    {   /* (v1.5) sampled every vz_step() column instead of every one: the segment runs from the previous sample to
         * this one and the last sample is always gw - 1, so the curve keeps its shape and still reaches the right
         * edge at any step. At step 1 these are exactly the segments this drew before. */
        uint32_t st = vz_step(), xp = 0;
        int32_t yp = clamp(gw_filter_resp_y(ftype, 0, gw, cut_x, res, y_zero, gh), gy0 + 2, gy0 + gh - 2);
        py = yp;
        for (x = (int32_t)st; ; x += (int32_t)st) {
            uint32_t xe = (uint32_t)x < (uint32_t)gw ? (uint32_t)x : (uint32_t)gw - 1u;
            int32_t y = clamp(gw_filter_resp_y(ftype, (int32_t)xe, gw, cut_x, res, y_zero, gh), gy0 + 2, gy0 + gh - 2);
            cv_line_a(gx0 + (int32_t)xp, yp, gx0 + (int32_t)xe, y,
                      bg_col(176 + (gy0 + gh - y) * 76 / (gh ? gh : 1)), CV_A_CURVE, 2);   /* (1.3.5) under the trace */
            yp = y;
            py = y;
            xp = xe;
            if (xe >= (uint32_t)gw - 1u)
                break;
        }
    }
    /* Cutoff Resonance Node Handle (Glowing Orange + White Core) */
    {
        int32_t cy_node = clamp(gw_filter_resp_y(ftype, cut_x, gw, cut_x, res, y_zero, gh), gy0 + 3, gy0 + gh - 4);
        cv_rrect(gx0 + cut_x - 3, cy_node - 3, 7, 7, 3, T_THEME, T_BG);
        cv_rect(gx0 + cut_x - 1, cy_node - 1, 3, 3, T_TEXT);
    }

    /* 5b. THE GRAPH ANSWERS: a TAP on HOME sends a wave across the bay for half a second, and the node that
     *     moved throws three sparks. No pool, no allocation: a sine of decaying amplitude and three pixels, all
     *     inside this bay -- nothing is drawn under the mini-knobs.
     *     (FASE 2/8) THE WAVE IS THE TAP'S, NOT THE KNOBS'. It hung off ui.home_pulse, and every HOME knob turn
     *     sets that too (the system's ring answers a knob, which is right). So turning CUT, RES, ATK or REL --
     *     any of the four -- drew this extra sine sweeping across the graph for half a second: the picture the
     *     player reported as "an oscilloscope that moves when I touch the knobs". The wave has its own stamp now,
     *     set only by go_home()'s tap; the rings keep answering the knobs exactly as they did. */
    if (ui.bay_wave) {
        uint32_t age = ui.frame - ui.bay_wave;
        if (age >= 30u)
            ui.bay_wave = 0;                        /* spent: one wave per tap, never a stale one on return */
        else {
            int32_t amp = (int32_t)(30u - age) * gh / 150;
            uint16_t wc = ux_mix(T_BG, T_THEME, 210 - (int32_t)age * 6);
            for (x = 0; x < gw; x += 2) {
                int32_t wy = clamp(y_zero + (bg_sin((uint32_t)(x * 460 + (int32_t)age * 2600)) * amp >> 15),
                                   gy0 + 1, gy0 + gh - 2);
                cv_pset(gx0 + x, wy, wc);
            }
            for (x = 0; x < 3; x++) {                  /* three sparks off the node, rising and fading */
                int32_t sy = y_zero - (int32_t)age * (2 + x) / 3 - x * 3;
                if (sy > gy0 + 1)
                    cv_rect(gx0 + cut_x - 1 + x * 3, sy, 1, 1,
                            ux_mix(T_BG, bg_col(190 + x * 20), 220 - (int32_t)age * 6));
            }
        }
    }

    /* 6. The two mono meters: the mix's own left and right peaks (fx.c fx_vu[6..7]). These bars used to
     * show the stereo width and the phase correlation -- two parameters of the mix, not signal -- so the
     * right one stood still whatever was played while the left moved with the notes. It was not a lost
     * update: it was measuring a constant. Two channels, two envelopes, neither one copied onto the
     * other, and both read the audio the listener hears. */
    {
        int32_t mx = x0 + w - 22, mh = gh - 6;
        int32_t gl = clamp((int32_t)fx_vu[6] * 256 / 9000, 0, 256);
        int32_t gr = clamp((int32_t)fx_vu[7] * 256 / 9000, 0, 256);
        cv_rect(mx - 3, gy0, 1, gh, T_LINE);
        gw_plasma_bar(mx + 1, gy0 + 2, 7, mh, 8, gl, 60);
        gw_plasma_bar(mx + 10, gy0 + 2, 7, mh, 8, gr, 96);
        cv_rect(mx + 3, gy0 + 3 + mh, 3, 1, T_DIM);        /* L and R, a dot under each bar */
        cv_rect(mx + 12, gy0 + 3 + mh, 3, 1, T_DIM);
    }
}

/* ============================================================================
 * 4. SCREEN 2 (WAVES & WRAITH): WAVE 1 + WAVE 2 + MIXER + WRAITH RIBBON
 *    (Matches planned_2_waves.png & latest GammaWave WAVE 1 / WAVE 2 / MIXER)
 * ============================================================================ */
static __attribute__((noinline)) void graph_being(const track_t *t)
{
    uint32_t c;
    int32_t k[4];
    for (c = 0; c < 4u; c++) {
        const char *lb;
        int32_t v = bg_knob(c, &lb);
        k[c] = v < 0 ? 128 : v;
    }
    /* (3.0) the page used to accumulate an animation clock into `tm` on every frame and never read it;
     * it is gone (the WAVES page's motion is the scope trace and the ribbon playhead, both live) */

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
        /* (v2.3.12) 11 px, not 9: asked twice, the four mini knobs still read small against the cards above.
         * The 26 px pitch leaves 4 px between rims (26 - 2 * 11) and the row keeps 5 rows of air to the rule
         * above (56 against 51) and 5 to the module's foot (77 against 82): bigger, and nothing touches. */
        gw_mini_bakelite_knob(kx, 66, 11, k[c], (c & 1u) ? T_ACCENT : T_THEME);
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

    /* 3. Lower Bay: WRAITH 6-Band Character Ribbon / 5-Band Vowel Ribbon.
     * (2.2) The ribbon is a CONTROL: six bands with their names, their glyphs and a playhead. The note
     * sparks used to be painted straight over it -- gw_note_particles(8, 86, 224, 32), the ribbon's own
     * rect -- which is exactly why they appeared on CHR and on the cells under it, and why HOME's own
     * gesture sparks landed there too (the pool had no owner). Nothing is drawn over the ribbon now: the
     * sparks belong to HOME's bay and to the full-screen visualizer, each inside its own rect. */
    gw_draw_wraith_ribbon(t, 8, 86, 224, 32);
}

/* ============================================================================
 * 5. SCREEN 1 (HOME): SPECTROGRAM FILTER BAY + BLAZING SUN/MOON + HEARTH & 6 TRACKS
 *    (Matches planned_1_home.png & latest GammaWave screenshot)
 * ============================================================================ */
static __attribute__((noinline)) void graph_ecosys(void)
{
    static int32_t en[NTRK];
    static uint32_t ec_frame;                           /* the frame of that draw */
    static uint32_t f0;
    int32_t tot_en = bg_energy();
    uint32_t c, dt = vz_qframe() - f0;
    if (dt > 8u) dt = 8u;
    if (ui_prefs & PREF_ANIM_OFF) dt = 0;
    f0 = vz_qframe();
    /* (3.0) the planets' answer to the notes: one scan of the voices a frame, additive, decaying; off
     * with ANIM OFF so the picture holds still by request */
    gw_planet_excite(dt != 0u);

    /* 1. Upper Bay (y = 2..63): FILTER + Yellow-Orange-Purple Spectrogram Waterfall + Cyan Mod Curve */
    gw_draw_forge_bay(TSEL, 8, 2, 224, 61, tot_en);

    /* 2. Lower-Left Bay (x = 8..98, y = 65..119): HOME'S SYSTEM -- dust, five orbits, five worlds.
     *    The dust goes first (it is the bay's depth, never in front of a planet or a value), then the
     *    system, then -- if the last ~0.7 s had a gesture -- the pulse. Nothing here is drawn under
     *    the mini-knobs of the right bay: those are controls, and the effects belong to this picture. */
    gw_oct_panel(8, 65, 91, 55, 4, T_BG, T_LINE, 1, T_THEME);   /* (3.6) 91 x 55, not 88 x 52: the room the
                                                             * mock-up had left -- 1 px above, 1 to the right
                                                             * (the hearth bay starts at 100), 2 below */
    sys_box(8, 65, 91, 55);                                 /* (3.1) the bay is the clip */
    gw_embers(10, 66, 87, 52, ui_viz_q >= 2u ? 7u : ui_viz_q == 1u ? 11u : 18u, 0x1300u, 96, T_BG);
    gw_draw_system(53, 92, 4 + (tot_en >> 8), tot_en, 210);
    if (ui.home_pulse) {                                    /* a HOME tap or a knob turned on HOME */
        uint32_t age = ui.frame - ui.home_pulse;
        if (age >= 64u) {
            ui.home_pulse = 0;
        } else {
            /* (3.1) the rings used to grow to 46 px around a 6 px star, so the outermost one crossed the bay:
             * their widest radius is now the bay's own (36 x 24 px), and the echo is smaller still */
            int32_t ring = 4 + (int32_t)age;
            int32_t i, ang;
            if (ring < 36)
                gw_orbit_ring(53, 92, ring, ring * 2 / 3, ux_mix(T_BG, T_THEME, 170 - (int32_t)age * 3), 1);
            if (age > 14u && ring < 34)                    /* the echo: narrower, greener, a little later */
                gw_orbit_ring(53, 92, (ring - 14) * 3 / 4 + 2, ((ring - 14) * 3 / 4 + 2) * 2 / 3,
                              ux_mix(T_BG, bg_col(120), 120 - (int32_t)age), 1);
            for (i = 0; i < 2; i++) {                      /* two sparks a frame, spiralling out of the orbits */
                if ((age & 3u) == (uint32_t)i) {
                    uint32_t k = (age / 4u) & 3u;
                    int32_t sr = ring > 30 ? 30 : ring;
                    ang = (int32_t)k * 16384 + (int32_t)age * 620 + i * 2400;
                    vz_np_add(53 + (bg_sin((uint32_t)ang + 0x4000u) * sr >> 15),
                              92 + (bg_sin((uint32_t)ang) * sr * 2 / 3 >> 15),
                              (bg_sin((uint32_t)ang + 0x4000u) * 3 >> 15), -(1 + (int32_t)(k & 1)),
                              bg_col(150 + (int32_t)k * 30 + i * 40), 8, k == 0 ? 1 : 1);
                }
            }
            /* the star itself brightens for the first half of the pulse: the system answers, it does not
             * just gain a ring. One extra disc, so the cost does not grow with the gesture */
            if (age < 24u)
                gw_disc(53, 92, 4 + (tot_en >> 8) + (int32_t)((24u - age) / 10u), bg_col(255));
        }
    }

    /* (2.2) HOME's gesture sparks, inside the bay that owns them and nowhere else. They were emitted here
     * since 2.0 but the only screen that drew the pool was EDIT, so they were painted on its ribbon. */
    vz_np_layer(VZ_OWN_HOME);
    gw_note_particles(9, 66, 88, 52);

    /* 3. Lower-Right Bay (x = 100..232, y = 66..118): HEARTH Physics Knobs (Heat/Sparks/Weight/Force) + 6 Track Pads */
    /* (v1.8.3) THE HEARTH CANNOT CACHE ITSELF. `draw_graph` starts every panel from a cleared canvas
     * (cv_begin fills it with the page's background), so a dial or an ingot that skipped its own draw
     * left a hole in the bay: the flicker test caught it at 375 px, erased for one frame and back. The
     * widgets are drawn whole on every paint, as they were. */
    gw_oct_panel(100, 66, 132, 52, 4, T_BG, T_LINE, 1, T_ACCENT);
    for (c = 0; c < 4u; c++) {                             /* (2.0) 8 px, not 9: a little smaller without
                                                            * losing the value arc or the 31 px pitch */
        int32_t kv = bg_knob(c, 0);
        int32_t kx = 118 + (int32_t)c * 31;
        /* (3.8) 7 px, not 8: the bay's four knobs sat as large as the modules' on top, which flattened the
         * hierarchy (the Hearth controls read as the headline row). One pixel down and the same 31 px pitch
         * keeps the arc, the value and the label untouched. */
        /* (3.9) 9 px, not 8, and not one pixel more: at r 9 the gap ring still lands on 108..128 / 201..221,
         * clear of the bay's left rim (100), its right (231), the header rule (66) and the divider (92) */
        gw_mini_bakelite_knob(kx, 79, 11, kv < 0 ? (110 + (int32_t)c * 28) : kv, (c & 1u) ? T_ACCENT : T_THEME);
    }
    cv_rect(104, 92, 124, 1, T_LINE);

    /* 6 Compact Track Ingot Indicators (T1..T6) at bottom of Hearth Bay */
    for (c = 0; c < NTRK; c++) {
        track_t *t = &trk[c];
        int32_t x = 104 + (int32_t)c * 21, y = 95, w = 19, h = 20;
        int32_t v = t->vu * 256 / 7000;
        int sel = (c == song.sel), muted = (t->p[P_MUTE] || c >= trk_live);
        char num[2] = {(char)('1' + c), 0};
        if (v > 256) v = 256;
        if (dt)
            en[c] = v > en[c] ? v : en[c] + (v - en[c]) / 4;
        cv_rrect(x, y, w, h, 2, sel ? T_RAISE : T_BG, T_BG);
        cv_text_in(x, y + 1, w, &AF_S, num, sel ? T_ACCENT : muted ? T_DIM : T_TEXT, sel ? T_RAISE : T_BG);
        gw_hbar(x + 2, y + 14, w - 4, 4, 3, muted ? 0 : en[c], 72);   /* (v1.9) no floor: silence reads as silence */
    }
}

/* ============================================================================
 * 6. DRUM-X: 8 CHAMFERED OCTAGONAL DRUM FORGE PADS + TRANSIENT WAVEFORMS
 * ============================================================================ */
static __attribute__((noinline)) void gw_drum_wave(uint32_t lane, int32_t x0, int32_t cy, int32_t w, uint16_t col)
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
        if (i) cv_line_a(x0 + i - 1, py, x0 + i, y, col, CV_A_WAVE, 1);   /* (1.3.5) a drawn wave, not a frame */
        py = y;
    }
}

static __attribute__((noinline)) void graph_colony(const track_t *t, int32_t sel)
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

        cv_shadow(px, py, pw, ph, 4, is_sel ? T_ACCENT : muted ? T_DIM : e > 12 ? bg_col(140 + e / 2) : T_THEME);
        gw_oct_panel(px, py, pw, ph, 4, fill, brd, is_sel || e > 16, is_sel ? T_ACCENT : T_THEME);
        cv_text_on(px + 5, py + 2, &AF_S, drum_lane_abbr(t, l),
                   is_sel ? T_ACCENT : muted ? T_DIM : T_TEXT, fill);
        if (!muted && e > 8)
            cv_rect(px + pw - 9, py + 5, 4, 4, bg_col(180 + (e >> 2)));
        gw_drum_wave(l, px + 6, py + 23, pw - 12, muted ? T_DIM : e > 8 ? bg_col(160 + e / 3) : T_THEME);
        gw_hbar(px + 5, py + ph - 8, pw - 10, 5, 6, muted ? 0 : (e > 256 ? 256 : e), 72);
    }
}
