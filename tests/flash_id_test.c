/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host check of the documented FM-1 flash-id allowlist. This is a pure helper
 * test: it does not claim that either part was read from hardware. */
#include <stdio.h>
#include "../firmware/hal/fm1_flash_id.h"

int main(void)
{
    int bad = 0;
    if (!fl_jedec_supported(FL_JEDEC_P25Q80H)) {
        puts("P25Q80H id rejected");
        bad++;
    }
    if (!fl_jedec_supported(FL_JEDEC_B36014)) {
        puts("B36014 id rejected");
        bad++;
    }
    if (fl_jedec_supported(0x123456u) || fl_jedec_supported(0u) || fl_jedec_supported(0xFFFFFFFFu)) {
        puts("unknown id accepted");
        bad++;
    }
    puts(bad ? "flash id test FAILED" : "flash id test passed");
    return bad != 0;
}
