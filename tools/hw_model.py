#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""FM-1 budget model: deadlines, buffers and SRAM — arithmetic, not a measurement.

Every constant is either from the project's own headers/docs (sample rate, block, half buffer, SRAM,
flash, the documented 160–240 MHz clock range) or derived from them. The model does NOT know the
AC791N's IPC, cache, DMA arbitration or interrupt latency, so it never prints a cycle count for the
firmware: it prints the cycle *ceiling* per block and labels host instruction counts as host data.
"""
import argparse

SR = 44100
BLOCK = 32
HALF_FRAMES = 128
SRAM_KIB = 96
FLASH_MIB = 1                                  # Puya P25Q80H, JEDEC 0x856014 (8 Mbit = 1 MiB)
LCD_W = LCD_H = 240
RGB565 = 2
CLOCKS = (160_000_000, 240_000_000)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--voices", type=int, nargs="+", default=[8, 10, 12])
    a = ap.parse_args()
    block_us = BLOCK / SR * 1e6
    blocks_s = SR / BLOCK
    dma_bytes = BLOCK * 2 * 2                      # stereo, 16-bit
    print("FM-1 budget model — arithmetic from documented constants, NOT an AC791N measurement")
    print("sample rate %d Hz, block %d samples = %.1f us, %.1f blocks/s" % (SR, BLOCK, block_us, blocks_s))
    print("I2S half buffer %d frames = %.2f ms; DMA per block %d B (stereo 16-bit)" %
          (HALF_FRAMES, HALF_FRAMES / SR * 1e3, dma_bytes))
    print("LCD %dx%d RGB565 = %d B/frame; at 30 fps %.1f MB/s" %
          (LCD_W, LCD_H, LCD_W * LCD_H * RGB565, LCD_W * LCD_H * RGB565 * 30 / 1e6))
    for mhz in (c // 1_000_000 for c in CLOCKS):
        print("  at %3d MHz: %.0f cycles per block ceiling (time x clock; IPC unknown)" %
              (mhz, block_us * 1e-6 * mhz * 1e6))
    print("voices  voice RAM (6 tracks)  per-voice share of %d KiB" % SRAM_KIB)
    for v in a.voices:
        ram = 88 * 6 * v                            # host ABI; target pointers are 32-bit, so <= this
        print("%6d  %18d B  %5.2f %% (upper bound, host ABI)" % (v, ram, 100.0 * ram / (SRAM_KIB * 1024)))
    print("flash %d MiB: not modelled (sector size, wear and erase time come from the JieLi SDK)" % FLASH_MIB)
    print("underruns/latency: NOT PREDICTABLE here — needs target cycles, DMA arbitration and IRQ latency")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
