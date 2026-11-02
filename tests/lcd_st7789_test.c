/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The ST7789's frame memory, modelled, against the REAL firmware/src/lcd.c (the target build compiles lcd.c
 * through felucca.c; no host test did, so the startup clear was never executed anywhere).
 *
 * Why this exists: the panel shows 240x240 but the controller's GDRAM is 240x320, and SWRESET / SLPOUT do not
 * clear it. lcd_fill() and lcd_blit() clamp to the panel, so before lcd_gdram_clear() the 80 rows outside the
 * window were left at whatever power-on left them, and the backlight was lit over that.
 *
 * The model implements what the panel actually does with the bytes lcd.c sends: CASET (0x2A) / RASET (0x2B)
 * address windows, RAMWR (0x2C) writing one pixel per two bytes from the cursor with the window's own wrap,
 * DISPON (0x29), and the backlight line. Nothing else is faked: the commands and the data are lcd.c's own.
 *
 * Built and run by tests/run_tests.sh. NO HARDWARE: this verifies the bytes on the wire, not what a bonded
 * panel slice shows -- that needs an FM-1. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GW 240u                       /* the controller's columns */
#define GH 320u                       /* its rows: a 240x240 panel is bonded to one 240-row slice of these */

/* ------------------------------------------------------------ the panel --- */
static uint16_t gdram[GH][GW];
static uint32_t writes[GH][GW];       /* how many times each cell was written */
static uint32_t px_written;           /* pixels accepted since reset */
static uint32_t out_of_range;         /* writes past the controller's memory */
static uint32_t disp_on_px, bl_on_px; /* px_written when DISPON / backlight-on were sent (-1: never) */
static uint32_t cur_cmd, param_i, ramwr;
static uint32_t ax0, ax1, ay0, ay1, cx, cy;
static uint8_t par[8];
static int bl_on;

static void panel_reset(void)
{
    memset(gdram, 0, sizeof gdram);
    memset(writes, 0, sizeof writes);
    px_written = out_of_range = 0;
    disp_on_px = bl_on_px = (uint32_t)-1;
    cur_cmd = param_i = ramwr = 0;
    ax0 = ay0 = 0; ax1 = GW - 1u; ay1 = GH - 1u; cx = cy = 0;
    bl_on = 0;
}

/* ------------------------------------------------- hal/fm1_lcd_hw.h, stubbed */
static void fm1_lcd_hw_init(void) { bl_on = 0; }        /* PA2 high: the backlight off */
static void fm1_lcd_baud(uint32_t b) { (void)b; }
static void fm1_lcd_wait(void) {}
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_backlight(uint32_t on)
{
    if (on) { bl_on = 1; bl_on_px = px_written; }
    else bl_on = 0;
}
static void fm1_lcd_send_cmd(uint8_t c)
{
    cur_cmd = c;
    param_i = 0;
    ramwr = (c == 0x2Cu);
    if (ramwr) { cx = ax0; cy = ay0; }
    if (c == 0x29u)                                   /* DISPON */
        disp_on_px = px_written;
}
static void fm1_lcd_send_data(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t i;
    if (ramwr) {                                       /* the frame memory: two bytes a pixel */
        for (i = 0; i + 1u < n; i += 2u) {
            uint32_t v = ((uint32_t)b[i] << 8) | b[i + 1u];
            if (cx >= GW || cy >= GH)
                out_of_range++;
            else {
                gdram[cy][cx] = (uint16_t)v;
                writes[cy][cx]++;
            }
            px_written++;
            if (++cx > ax1) { cx = ax0; if (++cy > ay1) cy = ay0; }
        }
        return;
    }
    if (cur_cmd == 0x2Au || cur_cmd == 0x2Bu) {        /* CASET / RASET: four bytes, big endian */
        for (i = 0; i < n && param_i < sizeof par; i++)
            par[param_i++] = b[i];
        if (param_i == 4u) {
            uint32_t a = ((uint32_t)par[0] << 8) | par[1], z = ((uint32_t)par[2] << 8) | par[3];
            if (cur_cmd == 0x2Au) { ax0 = a; ax1 = z; }
            else { ay0 = a; ay1 = z; }
        }
    }                                                  /* every other command's parameters: consumed only */
}

#include "../firmware/src/lcd.c"                       /* the real thing: lcd_init, lcd_fill, lcd_blit */

/* ------------------------------------------------------------------ checks */
static int check(const char *what, int ok)
{
    printf("lcd: %-78s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static uint32_t unwritten_cells(void)
{
    uint32_t y, x, n = 0;
    for (y = 0; y < GH; y++)
        for (x = 0; x < GW; x++)
            if (!writes[y][x]) n++;
    return n;
}
static uint32_t unwritten_rows(void)
{
    uint32_t y, x, n = 0;
    for (y = 0; y < GH; y++) {
        for (x = 0; x < GW; x++)
            if (!writes[y][x]) { n++; break; }
    }
    return n;
}
static uint32_t rows_of_colour(uint16_t c)
{
    uint32_t y, x, n = 0;
    for (y = 0; y < GH; y++) {
        for (x = 0; x < GW; x++)
            if (gdram[y][x] != c) break;
        if (x == GW) n++;
    }
    return n;
}

/* What lcd_init did before lcd_gdram_clear() existed: one window over the panel's slice and 240x240 pixels,
 * i.e. lcd_fill() after its clamp, with no write for the rows outside it. */
static void old_panel_clear(uint16_t c)
{
    uint32_t i, n, sw = (uint16_t)((c >> 8) | (c << 8));
    for (i = 0; i < 240u; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_sync();
    lcd_window(0, 0, 239, 239);
    for (n = 240u * 240u; n; n -= 240u)
        lcd_data(lcd_fillbuf, 240u * 2u);
}

int main(void)
{
    int bad = 0;

    /* (1) The defect, reproduced: exactly what lcd_init did before lcd_gdram_clear() existed. */
    panel_reset();
    old_panel_clear(0);
    {
        uint32_t u = unwritten_cells(), r = unwritten_rows();
        printf("lcd: a 240x240 fill leaves %u of %u cells and %u of %u rows of the frame memory untouched\n",
               u, GW * GH, r, GH);
        bad += check("reproduced: the panel's window leaves 19200 cells / 80 rows never written", u == 19200u && r == 80u);
        bad += check("reproduced: the 80 untouched rows are exactly rows 240..319", r == 80u && unwritten_rows() == 80u);
    }

    /* (2) The fix: the real lcd_init(). */
    panel_reset();
    lcd_init(0x0B1Fu);
    {
        uint32_t u = unwritten_cells();
        printf("lcd: lcd_init wrote %u pixels, %u cells untouched, window %u..%u x %u..%u\n",
               px_written, u, ax0, ax1, ay0, ay1);
        bad += check("lcd_init writes every one of the 76800 cells of the 240x320 frame memory",
                     u == 0u && px_written == GW * GH);
        bad += check("lcd_init's window really is the whole 240x320 (not the panel's 240x240)",
                     ax0 == 0u && ax1 == GW - 1u && ay0 == 0u && ay1 == GH - 1u);
        bad += check("DISPON is sent only after the whole frame memory was written",
                     disp_on_px == GW * GH);
        bad += check("the backlight is lit only after DISPON, and after the whole write",
                     bl_on && bl_on_px == GW * GH && bl_on_px == disp_on_px);
        bad += check("no write ever landed outside the controller's memory", out_of_range == 0u);
    }

    /* (3) The rows the panel does not show hold the same colour as the rest, so no bonded slice can keep
     *     power-on garbage or a background the UI never draws. */
    {
        uint32_t tail = rows_of_colour(0x0B1Fu);
        printf("lcd: %u of %u rows hold the clear's colour 0x%04X\n", tail, GH, gdram[0][0]);
        bad += check("every row, including 240..319, holds the background lcd_init was given", tail == GH);
    }
    /* (3b) The same for a whole-screen background fill: the palette can change after boot, and the tail has
     *      no way to be addressed from the UI. */
    panel_reset();
    lcd_fill(0, 0, 240, 240, 0x5A5Au);
    {
        uint32_t tail = rows_of_colour(0x5A5Au);
        printf("lcd: a whole-screen fill leaves %u of %u rows at its colour\n", tail, GH);
        bad += check("a whole-screen fill repaints rows 240..319 too", tail == GH);
    }
    panel_reset();
    lcd_fill(0, 0, 240, 120, 0x5A5Au);
    bad += check("a fill that is not the whole screen still leaves the tail alone",
                 unwritten_rows() == 200u);

    /* (4) The drawing primitives still cannot address past the controller, and a blit still stops at the
     *     panel: the UI's coordinates mean 240x240. */
    panel_reset();
    lcd_fill(0, 0, 240, 240, 0x1234u);
    bad += check("a whole-screen fill writes the whole controller and nothing outside it",
                 unwritten_cells() == 0u && px_written == GW * GH && out_of_range == 0u);
    panel_reset();
    lcd_blit(0, 0, 240, 240, (const uint16_t *)gdram);
    bad += check("lcd_blit still writes only the panel's 240x240 window",
                 unwritten_cells() == 19200u && px_written == 240u * 240u && out_of_range == 0u);
    panel_reset();
    lcd_fill(0, 0, 0, 0, 0);
    lcd_blit(0, 0, 0, 0, (const uint16_t *)gdram);
    bad += check("a zero-sized fill or blit writes nothing at all", px_written == 0u && out_of_range == 0u);

    /* (5) A fill that starts past the panel's edge writes nothing rather than wrapping into it. */
    panel_reset();
    lcd_fill(240, 0, 10, 10, 0);
    lcd_fill(0, 240, 10, 10, 0);
    bad += check("a fill outside the panel writes nothing", px_written == 0u && out_of_range == 0u);

    printf(bad ? "lcd: %d FAILED\n" : "lcd: all passed\n", bad);
    return !!bad;
}
