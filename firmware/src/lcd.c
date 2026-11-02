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

static uint16_t lcd_fillbuf[240];

static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, n, k, sw = (uint16_t)((c >> 8) | (c << 8));
    if (!w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    lcd_sync();
    k = 240u / w * w;                       /* whole rows per transfer (narrow fills: one DMA) */
    for (i = 0; i < k; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    for (n = w * h; n; n -= k) {
        if (k > n)
            k = n;
        lcd_data(lcd_fillbuf, k * 2u);
    }
}

static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (!w || !h)
        return;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
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

/* ST7789V init: reset, then 16-bit colour, scan direction and IPS inversion;
 * voltage and gamma stay at the defaults. Format: cmd, n, n data bytes;
 * cmd 0x00 = wait (data byte: ~ms). */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};

static void lcd_init(void)
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
    lcd_gdram_clear(0);                            /* every row of the 240x320, not just the 240x240 the UI uses */
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
