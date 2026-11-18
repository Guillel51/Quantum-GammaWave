/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 LCD wiring: SPI1 on PC9 CLK, PC10 DO, PC7 CS,
 * PC8 D/C; backlight PA2, active low. The panel protocol is src/lcd.c.
 *
 *   fm1_lcd_hw_init()          pins, backlight OFF, SPI1 master at BAUD 4 (lcd.c lights it after the first frame)
 *   fm1_lcd_baud(b)            SPI1 clock = lsb / (b + 1)
 *   fm1_lcd_send_cmd(c)        D/C low, CS low, one byte (then fm1_lcd_wait)
 *   fm1_lcd_send_data(p, n)    D/C high, CS low, DMA n bytes from RAM p
 *   fm1_lcd_wait()             SPI done (or timeout, counted), pending cleared
 *   fm1_lcd_deselect()         CS high
 *   fm1_lcd_backlight(on)      PA2 low (on) / high (off); the TIMER5 scan writes port A too: IRQs off around it
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_LCD_PC_OUT (*(volatile uint32_t *)0x50080u)
#define FM1_LCD_PC_DIR (*(volatile uint32_t *)0x50088u)
#define FM1_LCD_IOMAP_CON1 (*(volatile uint32_t *)0x51020u)
#define FM1_LCD_SPI_CON (*(volatile uint32_t *)0x11D00u)
#define FM1_LCD_SPI_BAUD (*(volatile uint32_t *)0x11D04u)
#define FM1_LCD_SPI_BUF (*(volatile uint32_t *)0x11D08u)
#define FM1_LCD_SPI_ADR (*(volatile uint32_t *)0x11D0Cu)
#define FM1_LCD_SPI_CNT (*(volatile uint32_t *)0x11D10u)
#define FM1_LCD_PA_OUT (*(volatile uint32_t *)0x50000u)
#define FM1_LCD_PA_DIR (*(volatile uint32_t *)0x50008u)
#define FM1_LCD_BL (1u << 2)
#define FM1_LCD_CS (1u << 7)
#define FM1_LCD_DC (1u << 8)
#define FM1_LCD_CLK (1u << 9)
#define FM1_LCD_DO (1u << 10)

static uint32_t fm1_lcd_timeouts;
/* (1.4.11) the frame-memory row the UI's row 0 lands on: 0 or 80, the 240-row slice of the controller's 240x320 the
 * panel is bonded to. Defined in gfx.c (every translation unit builds it; the host harness and the emulator stub
 * lcd.c); set from MENU > EXTRA > PANEL Y and used by lcd_window and the full-screen clear. */
extern uint32_t lcd_yoff;
static uint32_t fm1_lcd_bytes;      /* (1.4.7) the size of the transfer fm1_lcd_wait is about to wait for */

FM1_INLINE void fm1_lcd_hw_init(void)
{
    FM1_LCD_PA_OUT |= FM1_LCD_BL;   /* backlight off while the panel is reset and its RAM written: with it lit from
                                     * the first microsecond, the GDRAM no one has written yet shows as black lines */
    FM1_LCD_PA_DIR &= ~FM1_LCD_BL;
    FM1_LCD_IOMAP_CON1 |= 0x10u;
    FM1_LCD_PC_OUT |= FM1_LCD_CS;
    FM1_LCD_PC_OUT &= ~(FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_PC_DIR &= ~(FM1_LCD_CS | FM1_LCD_DC | FM1_LCD_CLK | FM1_LCD_DO);
    FM1_LCD_SPI_CON = 0x4021u;
    FM1_LCD_SPI_BAUD = 4u;
}

FM1_INLINE void fm1_lcd_backlight(uint32_t on)
{
    if (on)
        FM1_LCD_PA_OUT &= ~FM1_LCD_BL;
    else
        FM1_LCD_PA_OUT |= FM1_LCD_BL;
}

FM1_INLINE void fm1_lcd_baud(uint32_t b) { FM1_LCD_SPI_BAUD = b; }

/* (1.4.7) THE BOUND IS THE TRANSFER'S, NOT A FIXED NUMBER. It was 4 000 000 iterations for every
 * transfer. A byte at 12 MHz takes 8/12e6 s = 666 ns, and one iteration of this loop is a peripheral
 * read (~16 ns on this core), so a full-frame blit -- 240x240x2 = 115 200 B, ~77 ms -- needs about
 * 4.8 M of them: the fixed bound ran out first, cleared the pending bit and deselected CS in the
 * middle of a live DMA, and the panel kept whatever part of the frame had arrived. That is what a
 * screen showing "only some details" is: the blits that fit, plus truncated ones. 160 iterations
 * per byte is ~2.5 us/byte, three times the time the wire needs, and the wait only lasts that long
 * when the transfer really is not finishing. */
static void fm1_lcd_wait(void)
{
    uint32_t n, bound = 200u + fm1_lcd_bytes * 160u;
    fm1_lcd_bytes = 0;
    for (n = 0; n < bound && !(FM1_LCD_SPI_CON & 0x8000u); n++)
        ;
    if (n == bound)
        fm1_lcd_timeouts++;
    FM1_LCD_SPI_CON |= 0x4000u;
}

FM1_INLINE void fm1_lcd_deselect(void) { FM1_LCD_PC_OUT |= FM1_LCD_CS; }

FM1_INLINE void fm1_lcd_send_cmd(uint8_t c)
{
    FM1_LCD_PC_OUT &= ~FM1_LCD_DC;
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_BUF = c;
}

FM1_INLINE void fm1_lcd_send_data(const void *p, uint32_t n)
{
    FM1_LCD_PC_OUT |= FM1_LCD_DC;
    FM1_LCD_PC_OUT &= ~FM1_LCD_CS;
    FM1_LCD_SPI_CON |= 0x4000u;
    FM1_LCD_SPI_ADR = (uint32_t)(uintptr_t)p;
    FM1_LCD_SPI_CNT = n;
    fm1_lcd_bytes = n;              /* fm1_lcd_wait: how long this one is allowed to take */
}
