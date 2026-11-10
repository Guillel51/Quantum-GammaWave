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
    /* (3.0) the grey axis is decided by the palette being DRAWN (ux.mono), not only by the stored
     * settings.palette: a screen rendered as GREY while the setting still named another palette drew
     * tinted pixels into a gray page (54 of them, one at HOME's star). MONO keeps its black/white. */
    return ux.mono || ux.bw ? ux_gray(ux_luma(c) >> 3) : c;
}
static int32_t bg_sin(uint32_t ang) { return sine_i(ang << 16); }   /* Q15 */

/* (1.8.6) Note-reactive sparks for the visualizers. They are driven by the voices the
 * audio path is actually playing (gate, note, velocity), not by a random animation:
 * a note-on emits a small fan, a new note on a reused voice releases the old one, and
 * note-off emits a short outward burst. Bounded storage, no allocation, UI-side only.
 * The host's deterministic pixel tests set UI_NO_DRIFT, which also freezes these. */
/* (2.2) the pool's owner, outside UI_NO_DRIFT: the constant and the setter exist in both builds */
enum { VZ_OWN_NONE = 0, VZ_OWN_HOME, VZ_OWN_VIZ };
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
static void gw_note_particles(int32_t x0, int32_t y0, int32_t w, int32_t h)
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
static void gw_note_particles(int32_t x0, int32_t y0, int32_t w, int32_t h)
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

/* Smooth anti-aliased filled circular disc for THE HEARTH solar core, knobs & satellite moons */
static void gw_disc(int32_t cx, int32_t cy, int32_t r, uint16_t col)
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
    /* HOME's four mini-knobs are the four HOME encoders, not the last page's columns. Using page_desc()
     * here made the indicators follow a stale page and desynchronise from KNOB 1..4. */
    const param_desc_t *d = ui.home ? home_param(c, &vp) : page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    if (lab)
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
 *    - SIZE. Planets were 2 / 3 / 4 px discs in an 88 x 52 px bay, with the star at most 6 px.
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
/* the same ring gw_orbit_ring paints (24 segments from cx + rx, clockwise, every other one when
 * dotted), from the table: identical geometry, no interpolation */
static void gw_ring_tab(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int dotted)
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
            cv_line(px, py, mx, my, col);
        if (!dotted || !(k & 1u))
            cv_line(mx, my, x, y, col);
        px = x;
        py = y;
    }
}

/* A shaded planet: the dark side, the lit crescent turned toward the star, a terminator pixel
 * row between them, an atmosphere ring, a specular and two surface bands. Row spans come from a
 * monotone walk of the circle's half width (no sqrt, no table): the widest row is r px.
 * lx/ly: the unit-ish direction of the star from the planet, -1 / 0 / 1 each. */
static void sys_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);   /* the bay's clip (below) */
static void sys_pset(int32_t x, int32_t y, uint16_t c);
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
static void gw_sphere(int32_t x, int32_t y, int32_t r, int32_t heat, int32_t lx, int32_t ly, int kind,
                      uint32_t rot, int32_t flash)
{
    int32_t dy, span, atm, lo, hi, t, tz, lift;
    uint16_t edge, lit_top, lit_low;
    /* (3.0) a phase, not a crescent patch: the light comes from the star, so the terminator is the line
     * where the surface normal turns away from it, and each row is lit from that line to the lit limb.
     * (3.1) every row goes through sys_rect/sys_pset: the body, its atmosphere and its ring are clipped by
     * the bay itself, and the radii are small (2..5 px + effects) so the shading reads at 240 x 240. */
    uint16_t base = bg_col(heat - 70);                         /* the planet's own tone on the thermal ramp */
    uint16_t litc = ux_mix(base, T_TEXT, 46);                 /* the sunlit surface */
    uint16_t mid = ux_mix(T_BG, base, 66);                    /* the surface away from it: dark, not a hole */
    uint16_t dark = ux_mix(T_BG, base, 30);                   /* the far limb, one pixel of it */
    uint16_t term = ux_mix(litc, mid, 55), atmc = ux_mix(T_BG, bg_col(heat + 30), 38);
    /* (3.3) a note is a strike of light, not only a bigger body: the lit face and its limb brighten for the
     * first frames of pl_flash and fall back as it decays. Both tones are computed once, not per pixel. */
    lift = flash > 0 ? (flash >> 4) : 0;
    if (lift > 70) lift = 70;
    edge = ux_mix(litc, T_TEXT, 55 + lift);
    lit_top = ux_mix(litc, T_TEXT, 18 + lift);
    lit_low = ux_mix(litc, mid, 30 - (lift >> 1));
    if (r < 2) r = 2;
    if (r > 6) r = 6;
    if (kind == 3)                                                /* the ring first: the body hides its middle, so it
                                                                * reads as one ring passing behind the planet. (2.2)
                                                                * the ring answers a note too: the flash lifts its
                                                                * edge, so the giant is not the one body that never
                                                                * reacts */
        gw_ring_tab(x, y, r + 3, 2, ux_mix(T_BG, bg_col(heat + 40), 72 + (lift >> 1)), 0);
    for (dy = -(r + 1); dy <= r + 1; dy++) {                   /* the atmosphere: the 1 px ring the body leaves */
        atm = gw_span(r + 1, dy);
        span = gw_span(r, dy);
        if (atm > span) {
            sys_pset(x - atm, y + dy, atmc);
            sys_pset(x + atm, y + dy, atmc);
        }
    }
    for (dy = -r; dy <= r; dy++) {
        span = gw_span(r, dy);
        if (!span && dy)
            continue;
        sys_rect(x - span, y + dy, 2 * span + 1, 1, mid);
        if (span)                                              /* the far limb, one pixel */
            sys_pset(x + (lx > 0 ? -span : span), y + dy, dark);
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
        if (hi - lo > 0) {                                    /* the lit surface, tone by row: a sphere
                                                                * falling away from the light */
            tz = dy * 3 / (2 * r + 1);
            sys_rect(x + lo, y + dy, hi - lo + 1, 1, tz < 0 ? lit_top : tz > 0 ? lit_low : litc);
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
    if (kind == 0 && r >= 3) {                                 /* rocky: two craters, a pixel each, on the lit side */
        int32_t cx0 = x + lx * 1, cy0 = y - 1, cx1 = x + (lx > 0 ? -1 : 2), cy1 = y + 2;
        sys_pset(cx0, cy0, ux_mix(mid, T_BG, 55));
        sys_pset(cx0 + 1, cy0, ux_mix(mid, T_BG, 30));
        if (r >= 4) {
            sys_pset(cx1, cy1, ux_mix(mid, T_BG, 55));
            sys_pset(cx1, cy1 + 1, ux_mix(mid, T_BG, 30));
        }
    } else if (kind == 1 && r >= 3) {                           /* gas: two bands, one bright and one dark */
        sys_rect(x - gw_span(r, 0) + 1, y - 1, 2 * gw_span(r, 0) - 1, 1, ux_mix(mid, litc, 42));
        sys_rect(x - gw_span(r, 1) + 1, y + 1, 2 * gw_span(r, 1) - 1, 1, ux_mix(mid, T_BG, 60));
    } else if (kind == 2 && r >= 3) {                          /* ocean: a lit crescent and one horizon line */
        sys_rect(x - gw_span(r, 0) + 1, y + r / 2, 2 * gw_span(r, r / 2) - 1, 1, ux_mix(litc, T_TEXT, 30));
    } else if (kind == 4 && r >= 3) {                          /* (2.1) frozen: two caps and a crevasse. A body
                                                                * that reads at once as ice -- bright at both poles,
                                                                * one dark line across the middle -- so the fifth
                                                                * planet is not another grey rock */
        int32_t cw = gw_span(r, 1);
        sys_rect(x - cw + 1, y - r + 1, 2 * cw - 1, 1, ux_mix(litc, T_TEXT, 55));
        sys_rect(x - cw + 1, y + r - 1, 2 * cw - 1, 1, ux_mix(litc, T_TEXT, 40));
        sys_rect(x - gw_span(r, 0) + 2, y, 2 * gw_span(r, 0) - 3, 1, ux_mix(mid, T_BG, 55));
    }
    if (r >= 3) {                                              /* the specular, where the light lands */
        int32_t sx = x + lx * (r - 2) * 2 / 3, sy = y + ly * (r - 2) * 2 / 3;
        sys_pset(sx, sy, ux_mix(bg_col(240), T_TEXT, 75));
        sys_pset(sx - lx, sy - ly, ux_mix(litc, T_TEXT, 45));
    }
}

/* Which planet a track feeds, and how hard: 1/4 -> 0, 2/5 -> 1, 3/6 -> 2. A note-on is a voice
 * whose (note, gate) differs from the last scan's. Additive and clamped, so a chord or a fast run
 * raises the excitation instead of restarting it; the release is a fixed fraction a frame. */
#if !UI_NO_DRIFT
static void gw_planet_excite(int on)
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
static void gw_planet_excite(int on)
{
    (void)on;
    { uint32_t k; for (k = 0; k < NPL; k++) pl_ex[k] = 0; }
    pl_flash[0] = pl_flash[1] = pl_flash[2] = pl_flash[3] = 0;
}
#endif

/* (3.1) HOME's bay is a real clip, not a suggestion. Every primitive the system draws goes through these
 * row/pixel helpers, so a body, a ring, a corona, a trail or a gesture spark can never paint outside the
 * 88 x 52 px square the panel gives it. The check is four compares per call -- no clip stack, no state to
 * save and restore in gfx.c's hot path. */
static int16_t sys_x0 = 9, sys_y0 = 67, sys_x1 = 94, sys_y1 = 116;
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
static void gw_ring_semi(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t col, int back)
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
static void gw_draw_system(int32_t cx, int32_t cy, int32_t r, int32_t en, int32_t heat)
{
    /* three orbits: radius, tilt, rate (16-bit units a millisecond) and planet size. (3.1) SIZES are derived
     * from the bay, not chosen by eye: for orbit i the room left is min(half_w - ORX[i], half_h - ORY[i]);
     * the outer body is 4 px + 1 px atmosphere + a 3 px ring = 8 px, against 10 px of room, so it keeps a
     * 2 px margin at its widest point on every side. The old 4 / 6 / 7 px bodies (with a 4 px ring on the
     * last) needed 11 px and crossed the edge. */
    static const int8_t ORX[5] = {11, 18, 26, 33, 38};         /* (3.2) a fourth orbit, 11 px inside the first:
                                                                * a moon, so the system is not three rings and a sun;
                                                                * (2.1) a fifth outside it, 38 + 3 + 1 = 42 of the
                                                                * bay's 44, so it keeps its margin */
    static const uint8_t ORMS[5] = {9, 7, 6, 5, 4};            /* 7.3 s .. 16.4 s a turn (was 9-16 s) */
    /* (3.3) the body's own spin, 16-bit turn units a millisecond: 3.1 s, 4.7 s, 3.9 s and 2.5 s a turn. Faster
     * than the orbit on purpose -- a moon that spins in step with its orbit reads as a cut-out -- and each one
     * its own rate, so no two bodies turn together. */
    static const uint8_t ROTMS[5] = {21, 14, 17, 26, 31};
    /* (2.2) the starting angles, spread evenly round the ellipse (72 degrees apart) instead of the old
     * near-misses: four of them began within a third of a turn, so the bodies bunched into one blob beside
     * the star for most of the first second. They still drift apart on their own rates. */
    static const uint16_t ORPHS[5] = {0x0000, 0x1999, 0x3333, 0x4CCC, 0x6666};
    static const uint8_t ORR[5] = {4, 5, 5, 6, 3};             /* (2.1) each body one pixel larger than 3.3 --
                                                                * the outer one 6 px + ring, still 1 px of margin */
    static const uint8_t ORYS[5] = {70, 60, 52, 44, 36};       /* tilt: ry = 8 / 11 / 14 / 15 / 13 px */
    static const uint8_t ORK[5] = {0, 1, 2, 3, 4};             /* rocky, banded, ocean, ringed, frozen */
    /* fixed stars: x, y in the bay, size (1..3), luminance, twinkle phase */
    static const uint8_t ST[14][5] = {
        {4, 4, 2, 150, 11}, {13, 2, 1, 110, 47}, {31, 5, 1, 90, 83}, {46, 2, 2, 170, 29},
        {66, 6, 1, 120, 61}, {79, 13, 2, 190, 7}, {7, 19, 1, 80, 97}, {21, 15, 1, 140, 53},
        {38, 10, 1, 100, 23}, {57, 19, 2, 160, 71}, {82, 34, 1, 95, 37}, {5, 37, 1, 115, 89},
        {27, 43, 2, 175, 19}, {69, 41, 1, 105, 67}
    };
    /* the nebula: two dithered bands, positions from a table (the old per-pixel sine cost 44 lookups a frame) */
    static const uint8_t NEB[2][12] = {
        {6, 22, 13, 21, 21, 22, 29, 21, 38, 22, 47, 21},
        {30, 30, 40, 29, 49, 30, 58, 29, 67, 30, 76, 29}
    };
    int32_t i, j, orb_x[NPL], orb_y[NPL], orb_rot[NPL], order[NPL], a, big = 0;
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
        orb_ry[i] = ORX[i] * ORYS[i] / 100 + tilt_wob * (i + 1) / 24;
        if (orb_ry[i] < ORX[i] * 30 / 100)
            orb_ry[i] = ORX[i] * 30 / 100;             /* the ellipse never closes up into a line */
    }
    uint16_t star_ink = ux_mix(T_BG, T_THEME, 26), star_dim = ux_mix(T_BG, T_ACCENT, 18);
    if (r < 4) r = 4;
    if (r > 6) r = 6;                                 /* a star, not a sun: the planets carry the bay */
    for (i = 0; i < 2; i++) {
        uint16_t nc = i ? star_dim : star_ink;
        for (j = 0; j < 12; j += 2)
            sys_pset(sys_x0 + NEB[i][j], sys_y0 + NEB[i][j + 1], nc);
    }
    for (i = 0; i < 14; i++) {                         /* a slow twinkle, one sine a star, a quarter rate */
        int32_t tw = (int32_t)((bg_sin((twf * (2u + (uint32_t)(i & 3u)) + ST[i][4]) << 8) + 32768) >> 11);
        int32_t x = sys_x0 + 1 + ST[i][0], y = sys_y0 + 2 + ST[i][1];
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
    /* the far half of each path first: it passes behind the star and behind the front planets */
    for (i = 0; i < NPL; i++) {
        uint16_t oc = pl_ex[i] > 24 ? ux_mix(T_BG, bg_col(heat + i * 18), 34 + pl_ex[i] / 5)
                                    : ux_mix(T_BG, T_LINE, 26 + i * 10);   /* (2.2) fainter at rest: five
                                                                          * ellipses in one 88 px bay were
                                                                          * reading as a wire frame */
        gw_ring_semi(cx, cy, ORX[i], orb_ry[i], oc, 1);
    }
    /* each planet's place on its own ellipse: a pure function of the clock, so no frame rate and no audio
     * level can bend it; the excitation only lifts its radius and its glow */
    for (i = 0; i < NPL; i++) {
        a = (int32_t)((uint16_t)(tm * (uint32_t)ORMS[i] + ORPHS[i] + (uint32_t)(wob >> 6) * (uint32_t)(i + 1)));
        orb_x[i] = cx + (ORX[i] * bg_sin((uint32_t)a + 0x4000u) >> 15);
        orb_y[i] = cy + (orb_ry[i] * bg_sin((uint32_t)a) >> 15);
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
    for (i = 0; i < NPL; i++) {
        int32_t pi = order[i];
        if (orb_y[pi] >= cy)
            continue;
        if (!UI_NO_DRIFT && pl_ex[pi] > 32) {          /* a short afterglow arc under the body, not a second orbit */
            uint16_t gc = ux_mix(T_BG, bg_col(heat + pi * 18 + pl_ex[pi] / 3), 60);
            sys_line(orb_x[pi] - 2, orb_y[pi] + 2, orb_x[pi] + 2, orb_y[pi] + 2, gc);
        }
        gw_sphere(orb_x[pi], orb_y[pi], ORR[pi] + pl_ex[pi] / 96, heat + pi * 18 + pl_ex[pi] / 2,
                  orb_x[pi] < cx ? 1 : -1, orb_y[pi] < cy ? -1 : 1, ORK[pi],
                  (uint32_t)orb_rot[pi], pl_flash[pi]);
        if (pl_ex[pi] > big) big = pl_ex[pi];
    }
    /* the star: two corona rings, three discs of its own and a sparkle while the patch sounds; the biggest
     * planet's note lifts it a pixel, so the system answers as a whole */
    gw_disc(cx, cy, r + 2, ux_mix(T_BG, bg_col(70 + heat / 3), 30 + big / 14));
    gw_disc(cx, cy, r + 1, ux_mix(T_BG, bg_col(70 + heat / 3), 54 + big / 11));
    gw_disc(cx, cy, r, bg_col(170 + en / 5));
    gw_disc(cx, cy, r * 62 / 100, bg_col(228 + en / 9));
    gw_disc(cx, cy, r * 30 / 100, bg_col(252));
    if (en > 40 || big > 48) {                          /* a four-point sparkle while it sounds */
        int32_t sp = 2 + en / 70 + big / 110, s;
        for (s = 1; s <= sp; s++) {
            uint16_t sc = ux_mix(T_BG, bg_col(246), 170 - s * 24);
            sys_pset(cx + r + 1 + s, cy, sc);
            sys_pset(cx - r - 1 - s, cy, sc);
            sys_pset(cx, cy + r + 1 + s, sc);
            sys_pset(cx, cy - r - 1 - s, sc);
        }
    }
    /* the near half of each path, over the star, then the planets in front from the top down */
    for (i = 0; i < NPL; i++) {
        uint16_t oc = pl_ex[i] > 24 ? ux_mix(T_BG, bg_col(heat + i * 18 + pl_ex[i] / 3), 46 + pl_ex[i] / 4)
                                    : ux_mix(T_BG, T_LINE, 52 + i * 14);
        gw_ring_semi(cx, cy, ORX[i], orb_ry[i], oc, 0);
        if (!UI_NO_DRIFT && pl_ex[i] > 16) {           /* the note's trail on its own orbit, 3 px, short */
            int32_t k, aa0 = (int32_t)((uint16_t)(tm * (uint32_t)ORMS[i] + ORPHS[i] +
                                                   (uint32_t)(wob >> 6) * (uint32_t)(i + 1)));
            for (k = 1; k <= 3; k++) {
                int32_t aa = aa0 - k * (400 + pl_ex[i] * 2);
                int32_t nx = cx + (ORX[i] * bg_sin((uint32_t)aa + 0x4000u) >> 15);
                int32_t ny = cy + (orb_ry[i] * bg_sin((uint32_t)aa) >> 15);
                sys_pset(nx, ny, ux_mix(T_BG, bg_col(heat + i * 18 + pl_ex[i] / 2), pl_ex[i] * (4 - k) / 14));
            }
        }
    }
    for (i = 0; i < NPL; i++) {
        int32_t pi = order[i], ex = pl_ex[pi];
        if (orb_y[pi] < cy)
            continue;
        gw_sphere(orb_x[pi], orb_y[pi], ORR[pi] + ex / 96, heat + pi * 18 + ex / 2,
                  orb_x[pi] < cx ? 1 : -1, orb_y[pi] < cy ? -1 : 1, ORK[pi],
                  (uint32_t)orb_rot[pi], pl_flash[pi]);
        if (!UI_NO_DRIFT && ex > 40) {                 /* (3.2) the note's ring: the body answers at once,
                                                        * two pixels out, and fades with its own excitation */
            int32_t rr = ORR[pi] + 2 + ex / 80;
            gw_ring_tab(orb_x[pi], orb_y[pi], rr, rr * 2 / 3,
                        ux_mix(T_BG, bg_col(heat + pi * 18 + ex / 2), clamp(ex, 30, 200)), 1);
        }
        if (UI_NO_DRIFT || ex <= 40)
            continue;
        {   /* the note's sparks: the same bounded pool the other visualizers draw */
            uint16_t pc = bg_col(heat + pi * 18 + ex / 3);
            vz_np_add(orb_x[pi], orb_y[pi], (orb_x[pi] < cx ? -2 : 2), -2, pc, 9, 1);
        }
    }
}

/* the ambient dust of a bay: deterministic, no allocation, one sine per particle per frame. It is
 * the same field the full-screen visualizer draws (graph_home_viz), moved here so there is one
 * implementation: n embers drifting in a rect, dimmed to `bg` so they read as depth and never as
 * data. MENU > VISUALIZER QUALITY thins them; PREF_ANIM_OFF stops the motion (not the field). */
static void gw_embers(int32_t x0, int32_t y0, int32_t w, int32_t h, uint32_t n, uint32_t seed,
                      int32_t heat0, uint16_t bg)
{
    uint32_t i;
    int mono = ux.mono || ux.bw;
    if (!w || !h || !n)
        return;
    for (i = 0; i < n; i++) {
        int32_t px = x0 + 1 + (int32_t)(((i * 11u + (i % 5u) * 7u + seed) * 37u) % (uint32_t)(w - 2));
        int32_t py = y0 + h / 2 + (int32_t)((bg_sin(ui.frame * (55u + (i % 7u) * 9u) + i * 4096u + seed) * (h / 3 + (i % 6u) * 2)) >> 15);
        int32_t lvl = (int32_t)((bg_sin(ui.frame * (39u + (i % 5u) * 8u) + i * 3072u + seed) + 32768) >> 9);
        if (py >= y0 && py < y0 + h && px >= x0 && px < x0 + w) {
            uint16_t pc = mono ? (lvl > 90 ? T_TEXT : T_DIM) : ux_mix(bg, bg_col(heat0 + (int32_t)i * 7), 34 + lvl / 5);
            cv_rect(px, py, (i % 5u) == 0u ? 2 : 1, 1, pc);
        }
    }
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
    int is_drum = str_eq(ENGINES[eng]->name, "DRUM");
    int is_vowel = is_voice || (t->p[P_FTYPE] >= FT_VOWEL);
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
    default: /* LP: low-pass roll-off above cutoff */
        if (d > 0) att = (d * d) * h_max / (w * 18 + 1);
        break;
    }
    return clamp(y_zero - res_pk + att, y_zero - h_max / 2, y_zero + h_max / 2);
}

static void gw_mini_bakelite_knob(int32_t cx, int32_t cy, int32_t r, int32_t val256, uint16_t arc_col)
{
    uint32_t th = (uint32_t)((-384 + clamp(val256, 0, 256) * 768 / 256) * 64);
    int32_t px1 = cx + ((r - 2) * bg_sin(th) >> 15), py1 = cy - ((r - 2) * bg_sin(th + 0x4000u) >> 15);
    uint16_t bake = (ux.mono || ux.bw) ? T_RAISE : ux_mix(T_BG, bg_col(128), 42);
    uint16_t rim = ux.mono ? T_DIM : ux_mix(T_BG, arc_col, 38);
    /* A true radial control: a clean outer rim, a recessed body and a 270-degree needle. The old
     * two discs plus the dotted orbit read as a gear; every layer here is concentric and the
     * indicator is drawn from the body's edge, so the value stays legible at a glance. */
    gw_disc(cx, cy, r + 3, T_LINE);
    gw_disc(cx, cy, r + 2, rim);
    gw_disc(cx, cy, r, bake);
    gw_disc(cx, cy, r - 2, ux.mono ? bake : ux_mix(bake, T_BG, 48));
    gw_orbit_ring(cx, cy, r + 3, r + 3, arc_col, 0);
    cv_line_t(cx, cy, px1, py1, T_TEXT, 1);
    cv_pset(cx, cy, ux.mono ? T_TEXT : T_ACCENT);
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

    /* 0. The bay's dust, first: behind the grid, the bars, both curves and the trace. It is the same
     *    gw_embers() HOME's system sits in, thinned by VISUALIZER QUALITY, and it never moves with the
     *    audio -- it is the room the graph is drawn in, not a measurement. */
    gw_embers(x0 + 2, y0 + 2, w - 4, h - 4, ui_viz_q >= 2u ? 4u : ui_viz_q == 1u ? 6u : 10u, 0x2B00u, 70, T_BG);

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
    for (x = 0; x < gw; x += (int32_t)vz_step()) {      /* (v1.5) its sampling step: 1 / 2 / 3 px */
        int32_t mod_cut = clamp(cut_x + (mod_span > 4 ? mod_span : 14), 8, gw - 8);
        int32_t my = clamp(gw_filter_resp_y(ftype, x, gw, mod_cut, res / 2, y_zero + 3, gh), gy0 + 2, gy0 + gh - 2);
        if (x)
            cv_line(gx0 + x - (int32_t)vz_step(), mpy, gx0 + x, my, T_ACCENT);
        mpy = my;
    }

    /* 4. Live Triggered Pre-FX Waveform Trace */
    gw_live_trace(gx0 + 2, gw - 4, gy0 + gh / 2, gh / 3, 64);

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
            cv_line_t(gx0 + (int32_t)xp, yp, gx0 + (int32_t)xe, y,
                      bg_col(176 + (gy0 + gh - y) * 76 / (gh ? gh : 1)), 2);
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

    /* 5b. THE GRAPH ANSWERS: a knob turned on HOME (or HOME tapped) sends a wave across the bay for half a
     *     second, and the node that moved throws three sparks. No pool, no allocation: a sine of decaying
     *     amplitude and three pixels, all inside this bay -- nothing is drawn under the mini-knobs. */
    if (ui.home_pulse) {
        uint32_t age = ui.frame - ui.home_pulse;
        if (age < 30u) {
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
static void graph_being(const track_t *t)
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
static void graph_ecosys(void)
{
    static int32_t en[NTRK];
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

    /* 2. Lower-Left Bay (x = 8..96, y = 66..118): HOME'S SYSTEM -- dust, three orbits, four planets.
     *    The dust goes first (it is the bay's depth, never in front of a planet or a value), then the
     *    system, then -- if the last ~0.7 s had a gesture -- the pulse. Nothing here is drawn under
     *    the mini-knobs of the right bay: those are controls, and the effects belong to this picture. */
    gw_oct_panel(8, 66, 88, 52, 4, T_BG, T_LINE, 1, T_THEME);
    sys_box(8, 66, 88, 52);                                 /* (3.1) the bay is the clip */
    gw_embers(10, 67, 84, 50, ui_viz_q >= 2u ? 4u : ui_viz_q == 1u ? 7u : 11u, 0x1300u, 96, T_BG);
    gw_draw_system(52, 92, 4 + (tot_en >> 8), tot_en, 210);
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
                gw_orbit_ring(52, 92, ring, ring * 2 / 3, ux_mix(T_BG, T_THEME, 170 - (int32_t)age * 3), 1);
            if (age > 14u && ring < 34)                    /* the echo: narrower, greener, a little later */
                gw_orbit_ring(52, 92, (ring - 14) * 3 / 4 + 2, ((ring - 14) * 3 / 4 + 2) * 2 / 3,
                              ux_mix(T_BG, bg_col(120), 120 - (int32_t)age), 1);
            for (i = 0; i < 2; i++) {                      /* two sparks a frame, spiralling out of the orbits */
                if ((age & 3u) == (uint32_t)i) {
                    uint32_t k = (age / 4u) & 3u;
                    int32_t sr = ring > 30 ? 30 : ring;
                    ang = (int32_t)k * 16384 + (int32_t)age * 620 + i * 2400;
                    vz_np_add(52 + (bg_sin((uint32_t)ang + 0x4000u) * sr >> 15),
                              92 + (bg_sin((uint32_t)ang) * sr * 2 / 3 >> 15),
                              (bg_sin((uint32_t)ang + 0x4000u) * 3 >> 15), -(1 + (int32_t)(k & 1)),
                              bg_col(150 + (int32_t)k * 30 + i * 40), 8, k == 0 ? 1 : 1);
                }
            }
            /* the star itself brightens for the first half of the pulse: the system answers, it does not
             * just gain a ring. One extra disc, so the cost does not grow with the gesture */
            if (age < 24u)
                gw_disc(52, 92, 4 + (tot_en >> 8) + (int32_t)((24u - age) / 10u), bg_col(255));
        }
    }

    /* (2.2) HOME's gesture sparks, inside the bay that owns them and nowhere else. They were emitted here
     * since 2.0 but the only screen that drew the pool was EDIT, so they were painted on its ribbon. */
    vz_np_layer(VZ_OWN_HOME);
    gw_note_particles(9, 67, 86, 50);

    /* 3. Lower-Right Bay (x = 100..232, y = 66..118): HEARTH Physics Knobs (Heat/Sparks/Weight/Force) + 6 Track Pads */
    gw_oct_panel(100, 66, 132, 52, 4, T_BG, T_LINE, 1, T_ACCENT);
    for (c = 0; c < 4u; c++) {                             /* (2.0) 8 px, not 9: a little smaller without
                                                            * losing the value arc or the 31 px pitch */
        int32_t kv = bg_knob(c, 0);
        int32_t kx = 118 + (int32_t)c * 31;
        gw_mini_bakelite_knob(kx, 78, 8, kv < 0 ? (110 + (int32_t)c * 28) : kv, (c & 1u) ? T_ACCENT : T_THEME);
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
