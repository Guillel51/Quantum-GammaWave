/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Pure allowlist for the documented FM-1 1 MiB flash parts. Kept separate from
 * fm1_flash.h so a host test can compile it without the pi32v2 SFC primitives. */
#pragma once
#include <stdint.h>

#define FL_JEDEC_P25Q80H 0x856014u
#define FL_JEDEC_B36014  0xB36014u

static inline int fl_jedec_supported(uint32_t id)
{
    return id == FL_JEDEC_P25Q80H || id == FL_JEDEC_B36014;
}
