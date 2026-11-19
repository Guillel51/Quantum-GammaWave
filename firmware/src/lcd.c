/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ST7789-class 240x240 panel on SPI1:
 * PC9 CLK, PC10 DO, PC7 CS, PC8 D/C, backlight PA2 active low. Polled DMA
 * transfers; every source buffer must be in RAM.
 * A pixel transfer is left running (lcd_busy): the next LCD access, or a write
 * to its source buffer (lcd_sync), waits for it, so the main loop works
 * while the last strip of a frame goes out. */
/* pins and SPI1: hal/fm1_lcd_hw.h */
#ifndef LCD_BAUD
#define LCD_BAUD 4u                /* lsb/(BAUD+1): 4 = 12 MHz */
#endif

static uint8_t lcd_small[64];
static uint8_t lcd_busy;           /* a lcd_data DMA may still run; CS is low */

static void lcd_spin(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++)
        ;
}

static void lcd_sync(void)          /* finish the running transfer (before touching its buffer) */
{
    if (!lcd_busy)
        return;
    fm1_lcd_wait();
    fm1_lcd_deselect();
    lcd_busy = 0;
}

static void lcd_cmd(uint8_t c)
{
    lcd_sync();
    fm1_lcd_send_cmd(c);
    fm1_lcd_wait();
    fm1_lcd_deselect();
}

static void lcd_data(const void *p, uint32_t n)
{
    if (!n)
        return;                    /* SPI_CNT = 0 never completes */
    lcd_sync();
    fm1_lcd_send_data(p, n);
    lcd_busy = 1;                  /* completed by lcd_sync */
}

/* (1.4.8) WHICH SLICE OF THE CONTROLLER'S 240x320 THE PANEL SCANS. The UI draws in 240x240 and everything below
 * went to frame-memory rows 0..239, which assumes the module is bonded to the top slice. A 240x240 panel is bonded to
 * one 240-row slice of a 240x320 memory and the common ST7789 modules use 80 (Adafruit, Arduino_GFX, TFT_eSPI): with
 * 80 the picture sits 80 rows below where it is written, so the top third of every screen (the header and the page
 * titles) is off the glass and the bottom third shows memory nobody draws in. `off N` in the console moves the window
 * and forces a full redraw, so which slice this unit has is a thing the owner can see, not a thing to guess.
 * (1.4.11) It is a saved setting now -- MENU > EXTRA > PANEL Y, panel.c settings_yoff -- and its storage lives in
 * gfx.c, declared in hal/fm1_lcd_hw.h, so the host harness and the emulator (which stub lcd_fill / lcd_blit) have
 * it too. */

/* the raw window command: frame-memory coordinates. The callers that take the UI's 240x240 coordinates add
 * lcd_yoff themselves (lcd_fill, lcd_blit); the ones that address the memory directly (lcd_gdram_clear, and the
 * full-screen fill's other slice) do not. */
static void lcd_window(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    lcd_sync();
    lcd_small[0] = (uint8_t)(x0 >> 8);
    lcd_small[1] = (uint8_t)x0;
    lcd_small[2] = (uint8_t)(x1 >> 8);
    lcd_small[3] = (uint8_t)x1;
    lcd_cmd(0x2A);
    lcd_data(lcd_small, 4);
    lcd_small[4] = (uint8_t)(y0 >> 8);       /* own bytes: the x transfer may still read [0..3] */
    lcd_small[5] = (uint8_t)y0;
    lcd_small[6] = (uint8_t)(y1 >> 8);
    lcd_small[7] = (uint8_t)y1;
    lcd_cmd(0x2B);
    lcd_data(lcd_small + 4, 4);
    lcd_cmd(0x2C);
}

/* (v1.5.6) EVERY FILL MAKES THE CANVAS CACHE STALE, and this is the one place that knows it: gfx.c skips a blit whose
 * pixels the panel already has, and a fill under that window changes them. A counter instead of a call keeps lcd.c
 * free of gfx: the tests that link this file alone still build. */
uint32_t lcd_gen;
static uint16_t lcd_fillbuf[240];

static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, n, k, sw = (uint16_t)((c >> 8) | (c << 8));
    if (!w || !h || x >= 240u || y >= 240u)
        return;
    lcd_gen++;                              /* (v1.5.6) the panel changed outside gfx's blit */
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    lcd_sync();
    k = 240u / w * w;                       /* whole rows per transfer (narrow fills: one DMA) */
    for (i = 0; i < k; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_window(x, y + lcd_yoff, x + w - 1u, y + h - 1u + lcd_yoff);   /* (1.4.11) onto the panel's slice */
    for (n = w * h; n; n -= k) {
        if (k > n)
            k = n;
        lcd_data(lcd_fillbuf, k * 2u);
    }
    /* (QG-1 v1.5) A whole-screen background fill also repaints the controller's rows the panel is not bonded
     * to (240..319). They cannot be addressed from the UI's 240x240 coordinates, so without this they keep
     * whatever colour they had at boot: on a module bonded to the upper slice that reads as a band of the
     * startup background across the bottom of the screen for the rest of the session, and it changes colour
     * with the palette without them. The clamp above still keeps every other fill inside the panel. */
    if (!x && !y && w == 240u && h == 240u) {
        /* (1.4.8) whichever rows the UI's slice leaves out: 240..319 at offset 0, 0..(off-1) at offset 80 */
        uint32_t g0 = lcd_yoff ? 0u : 240u, g1 = lcd_yoff ? lcd_yoff - 1u : 319u;
        lcd_window(0, g0, 239u, g1);
        for (n = (g1 - g0 + 1u) * 240u; n; n -= 240u)  /* 240 at a time (lcd_fillbuf's size) */
            lcd_data(lcd_fillbuf, 240u * 2u);
    }
}

static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (!w || !h)
        return;
    lcd_window(x, y + lcd_yoff, x + w - 1u, y + h - 1u + lcd_yoff);   /* (1.4.11) onto the panel's slice */
    lcd_data(px, w * h * 2u);
}

/* The controller's frame memory is 240x320, not the 240x240 the panel shows: a 240x240 panel is bonded to one
 * 240-row slice of it and which slice depends on the module (Adafruit/Arduino_GFX/Zephyr all use 320 - 240 = 80 for
 * the parts they ship). lcd_fill / lcd_blit clamp to the panel, so before this they left the other rows at whatever
 * power-on left there -- and frame memory is not cleared by SWRESET or SLPOUT, so it reads back as noise. With the
 * backlight last, those untouched rows are exactly what showed as the black lines at startup: the clear covered the
 * window the UI draws in, not the memory the panel scans. Writing all 320 rows closes it either way, with no offset
 * to guess and nothing the UI can reach changing. */
static void lcd_gdram_clear(uint16_t c)
{
    uint32_t i, n, sw = (uint16_t)((c >> 8) | (c << 8));
    for (i = 0; i < 240u; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_sync();
    lcd_window(0, 0, 239, 319);                    /* the whole 240x320, past lcd_fill's 240 clamp */
    for (n = 240u * 320u; n; n -= 240u)            /* 76800 pixels, 240 at a time (lcd_fillbuf's size) */
        lcd_data(lcd_fillbuf, 240u * 2u);
}

/* ST7789V init: reset, then the panel's own power, VCOM and gamma block, then 16-bit colour, scan direction and
 * IPS inversion. Format: cmd, n, n data bytes; cmd 0x00 = wait (data byte: ~ms).
 *
 * (1.4.10) THE TABLE IS THE FULL SEQUENCE AGAIN, AND THE VALUES ARE NOT MINE. 1.4 wrote a 2.2 curve and the
 * brightness register from a comparison against the emulator, never from a panel, and on the owner's FM-1 it crushed
 * the low half of the range in every screen; 1.4.2.1 answered by removing everything after MADCTL, which leaves the
 * panel on its power-on defaults for gate voltage, VCOM, power and gamma -- a state nobody chose and nobody measured,
 * and the report from the unit (everything almost black, some details, whole words missing) is what a panel looks
 * like when that block is not the module's.
 *
 * So the sequence below is the standard 240x240 ST7789 set as published by the driver these modules are used with:
 * Adafruit_ST7789's init_240x240 (PORCTRL 0x0C/0x0C/0x00/0x33/0x33, GCTRL 0x35, VCOMS 0x19, LCMCTRL 0x2C, VDVVRHEN
 * 0x01, VRHS 0x12, VDVS 0x20, FRCTRL2 0x0F, PWCTRL1 0xA4/0xA1 and its two 14-byte gamma tables), byte for byte, and
 * the same block TFT_eSPI and Pimoroni ship for 240x240. What 1.4 did wrong was inventing numbers against the
 * emulator; this is the one set the module's own driver carries. Two exemptions stay out in the open: brightness
 * (WRCTRLD/WRDISBV, 0x53/0x51) because the FM-1's backlight is a GPIO and the register changed nothing when 1.4
 * wrote it, and CABC (0xC7) because content-adaptive dimming on a synth's screen is a bug, not a feature.
 * tests/panel_table_test.py fails the build if this table drifts from that reference, if a command goes missing, or
 * if brightness or CABC come back. Class: the driver's published table; the effect on this panel is the owner's to
 * see, and nothing here has been measured on the unit. */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,   /* PORCTRL: the porches (VFP/VBP/HFP/HBP) */
    0xB7, 1, 0x35,               /* GCTRL: VGH / VGL */
    0xBB, 1, 0x19,               /* VCOMS: the common voltage */
    0xC0, 1, 0x2C,               /* LCMCTRL */
    0xC2, 1, 0x01,               /* VDVVRHEN: VDV and VRH set by command */
    0xC3, 1, 0x12,               /* VRHS */
    0xC4, 1, 0x20,               /* VDVS */
    0xC6, 1, 0x0F,               /* FRCTRL2: frame rate in normal mode */
    0xD0, 2, 0xA4, 0xA1,         /* PWCTRL1 */
    0xE0, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,  /* PVGAMCTRL */
    0xE1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,  /* NVGAMCTRL */
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};
/* bg: the theme's background (main.c passes T_BG, valid because settings_init() applies the palette before
 * this runs). The whole 240x320 is written in it, not in black, so no bonded slice of the panel can show a
 * colour the splash and the UI never draw. */
static void lcd_init(uint16_t bg)
{
    uint32_t r, x;
    fm1_lcd_hw_init();
    lcd_spin(2000000u);
    for (r = 0; r < sizeof LCD_SEQ; r += 2u + LCD_SEQ[r + 1u]) {
        if (LCD_SEQ[r] == 0x00u) {                       /* pseudo command: wait */
            lcd_spin(LCD_SEQ[r + 2u] * 25000u);          /* ~1 ms per unit (lcd_spin(3000000) ~ 120 ms) */
            continue;
        }
        lcd_cmd(LCD_SEQ[r]);
        for (x = 0; x < LCD_SEQ[r + 1u]; x++)
            lcd_small[x] = LCD_SEQ[r + 2u + x];
        if (LCD_SEQ[r + 1u])
            lcd_data(lcd_small, LCD_SEQ[r + 1u]);
    }
    fm1_lcd_baud(LCD_BAUD);
    lcd_gdram_clear(bg);                           /* every row of the 240x320, not just the 240x240 the UI uses */
    lcd_cmd(0x29);
    /* The backlight only now (fm1_lcd_hw_init left it off): the panel has had SWRESET, SLPOUT, COLMOD, MADCTL and a
     * write of its whole 240x320 frame memory, so there is no row the panel can scan that nobody has written. PA2 is
     * also the key matrix 595's /OE, so it has to be low before fm1_input_init / panel_init (both after this call). */
    fm1_lcd_backlight(1);
}

/* MENU > SCREEN OFF (ui.c scr_frame): the panel dark while the sound goes on. s 0: DISPOFF, SLPIN; 1: SLPOUT (the
 * panel wants 120 ms after it, and 120 ms after SLPIN before it: ui.c waits, a frame at a time, never here); 2: DISPON
 * (after the frame that redrew it all). The panel keeps its RAM asleep, but the UI redraws everything before it shows
 * (ui.force). The backlight stays on: PA2 is also the key matrix 595's /OE, so with it
 * high no button, key or knob reads and nothing could wake the screen (#157 follow-up, found on 1.1.5) */
static void lcd_power(uint32_t s)
{
    if (s == 1u)
        lcd_cmd(0x11);                             /* SLPOUT */
    else if (s == 2u)
        lcd_cmd(0x29);                             /* DISPON */
    else {
        lcd_cmd(0x28);                             /* DISPOFF */
        lcd_cmd(0x10);                             /* SLPIN */
    }
}
/* the crash screen, UPDATE, UBOOT: the panel on at once if SCREEN OFF had it asleep (blocking, ~240 ms; the IRQs
 * left as they are: the crash handler calls it) */
static void lcd_wake_now(void)
{
    lcd_spin(3000000u);                            /* (120 ms after a SLPIN) */
    lcd_cmd(0x11);
    lcd_spin(3000000u);
    lcd_cmd(0x29);
    fm1_lcd_backlight(1);
}
