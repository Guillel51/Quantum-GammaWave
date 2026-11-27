/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * RAMTEXT (JIANT 0.6.78: at the top of the pool, was 0x01C00000): .ram_text (code that must run while the flash is
 * busy: the flash driver), .c1_text (the second core's loop), .hot_text (core 0's hottest audio code). A call reaches
 * +-4 MiB (pi32v2 imm23 x 2, no long calls in this compiler): RAM code there reaches the whole XIP image (build.py
 * checks the margin); at 0x01C00000 it reached none of it.
 * LOWBUF 0x01C00000..0x01C08000 (0.6.78; 0.6.82: 4 KiB more, the steps grew; 1.4.5: the last page, which nothing
 * used -- it was kept "for RAM-run debug" -- joins it): main-loop buffers (undo, the project scratch, the song
 * section's stage, the flash driver's staging buffer and the generator's backup), zeroed at boot. Every one of them
 * is touched from the main loop only: the audio ISR never reads across a flash write. 8 KiB of .data+.bss back.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8E7BC
    LOWBUF (rw)  : ORIGIN = 0x01C00000, LENGTH = 0x8000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x50000
    /* (FASE 1/6) up 1 KiB (0x01C71000 -> 0x01C70C00 origin), length down the same KiB so the guard band at
     * 0x01C76800 does not move: the XIP code grew 500 B with the MORPH graphs and the call window from the RAM
     * code had 8 032 B left of the required 8 192.
     * (FASE 2/6) up 2.5 KiB more, 0x01C70C00 -> 0x01C71600, length 0x5C00 -> 0x5200: turning the window cache
     * on grows the XIP code by 3 288 B and the call window from the RAM code fell to 5 856 B of the 8 192
     * required. The window keeps the RAM code (3 940 B) and the cold `.ramdata` tail (16 656 B) in 20 992 B, and
     * the guard band still starts at 0x01C76800. */
    /* (FASE 5e 2026-11-27) up 2 KiB, length down the same 2 KiB (0x4C00 -> 0x4400) so the guard band at
     * 0x01C76800 does not move: the XIP code grew with the FX page's own layout, M-WIDE's field, FX-MASTER's cards
     * and the models' emblems, and the call window from the RAM code fell to 7 036 B of the 8 192 required. The
     * window keeps the RAM code (3 940 B) and the cold .ramdata tail (9 168 B) in 17 408 B, 4 492 B to spare. */
    RAMTEXT (rwx): ORIGIN = 0x01C72400, LENGTH = 0x4400   /* (FASE 5c 2026-11-27) up 512 B, length down the same 512
                                                           * so the guard band at 0x01C76800 does not move: the LZ4
                                                           * decoder grew the XIP code by 176 B and the call window
                                                           * from the RAM code fell to 8 056 B of the 8 192 required
                                                           * (the RAM code itself is 3 940 B of the window). */   /* (0.6.97, 0.7.0, 1.0-GW: up 2 KiB so calls from RAMTEXT reach
                                                           * >16 KiB past the XIP image's end; user stack is 13.75 KiB)
                                                           * (FASE 3/8 2026-11-27) up 1 KiB more, 0x01C71600 -> 0x01C71A00,
                                                           * length 0x5200 -> 0x4E00, the guard band still at 0x01C76800: the
                                                           * knee's second copy (rev_room_soft, CLOUD's own network) grew the XIP
                                                           * code by 748 B and the call window from the RAM code fell to 7 956 B
                                                           * of the 8 192 required. The window holds 19 572 B of its 19 968. */
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    /* (1.0-GW) _ecode: the end of the code. All the code comes first, the constants after it (fonts, icons, tables,
     * presets, strings): only code calls or is called by the RAM code, so the call reach (build.py) is measured to
     * _ecode, not to the image's end */
    .text : { KEEP(*(.text.crt0)) *(.text*) . = ALIGN(4); _ecode = .; *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    /* (JIANT 0.6.71) the second core's waiting loop (hal/fm1_multicore.h): in RAM, after the flash driver, inside the
     * guard's RAM code window (_rt_start .. _c1t_end); its own section, as it calls the jobs (the build's check that
     * .ram_text calls nothing still holds) */
    .c1_text : { _c1t_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1t_end = .; } > RAMTEXT AT > XIP
    _c1t_load = LOADADDR(.c1_text);
    /* (JIANT 0.6.78) core 0's hottest audio code (tools/build.py HOT_FNS): in RAM after the second core's loop, inside the
     * same RAM code window (_rt_start .. _hot_end) */
    .hot_text : {
        _hot_start = .;
        KEEP(*(.hot_text*))   /* (tools/build.py HOT_FNS: their sections renamed in the optimised IR) */
        . = ALIGN(4);
        _hot_end = .;
    } > RAMTEXT AT > XIP
    _hot_load = LOADADDR(.hot_text);
    /* (2.5.0) THE WINDOW'S TAIL, FOR COLD DATA. RAMTEXT is 24 KiB; the RAM code that has to keep running while the
     * flash is busy uses about 4 KiB of it, and the rest was idle while the 96 KiB RAM window was down to 4.7 KiB
     * of headroom. The buffers here are main-loop only -- none of them is read from the audio ISR -- so they cost
     * nothing in latency where they are now, and the RAM window gets their bytes back. NOLOAD: the boot copy of
     * the code above cannot write them, and the ASSERT fails the link instead of the unit. */
    .ramdata (NOLOAD) : { _rd_start = .; *(.bss.ramdata*) *(.ramdata*) . = ALIGN(4); _rd_end = .; } > RAMTEXT
    ASSERT(_rd_end <= ORIGIN(RAMTEXT) + LENGTH(RAMTEXT), "RAMTEXT is full: the RAM code and its cold data outgrew the window")
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    /* (JIANT 0.6.71) the second core's stacks (hal/fm1_multicore.h): 6 KiB for the render, 1 KiB for an exception */
    .c1_stacks (NOLOAD) : { . = ALIGN(32); _c1_stack_lo = .; . += 7168; _c1_stack_hi = .; } > POOL
    _c1_ustack_top = _c1_stack_lo + 6144;
    _c1_sstack_top = _c1_stack_hi;
    .lowbuf (NOLOAD) : { _low_start = .; *(.lowbuf*) . = ALIGN(4); _low_end = .; } > LOWBUF
    /* (1.4.5) the window is a budget of its own: a buffer that outgrows it must fail here, not at run time */
    ASSERT(_low_end <= ORIGIN(LOWBUF) + LENGTH(LOWBUF), "LOWBUF is full: a main-loop buffer outgrew its window")
    .noinit (NOLOAD) : { _ni_start = .; *(.noinit*) . = ALIGN(4); _ni_end = .; } > NOINIT
    /* (v1.5.12) THE SAME RULE LOWBUF HAS, FOR THE WINDOW THAT WAS CLOSEST TO FULL: 15 572 of 15 696 B used,
     * 124 B to spare. A no-init buffer that grows past it linked fine and then shared memory with the guard
     * words and the stacks at run time. It fails here now, with the unit untouched. */
    ASSERT(_ni_end <= ORIGIN(NOINIT) + LENGTH(NOINIT), "NOINIT is full: a no-init buffer outgrew its window")
    _guard0 = 0x01C76800; _ustack_lo = 0x01C76900; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
