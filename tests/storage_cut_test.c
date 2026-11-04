/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Power-cut matrix for every persistent object of firmware/src/storage.c.
 *
 * storage_test.c cuts every byte of ONE object's write and checks the older copy
 * survives. This one walks all OBJ_* objects (settings, the four project slots,
 * user presets, songs 2..8, the song index, the TAKES reel, the FM6 voice bank's
 * shared pair, the autosave) and cuts at every erase and every page program of a
 * save, half way through the operation, with no cleanup code afterwards:
 *
 *   - the load must return the previous copy, byte for byte (never nothing,
 *     never a torn payload);
 *   - the bytes outside the object's own two sectors must be untouched;
 *   - the next save must succeed and load back.
 *
 *   cc -o storage_cut_test tests/storage_cut_test.c && ./storage_cut_test
 */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nor[0x100000];
static uint8_t snap[0x100000];
static jmp_buf cut_jb;
static long cut_at = -1, opno;
static int cutting;

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (cutting && opno++ == cut_at) {                 /* half an erase, then the power goes */
        memset(nor + off, 0xFF, 2048);
        longjmp(cut_jb, 1);
    }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (cutting && opno++ == cut_at) {                 /* half a page, then the power goes */
        for (i = 0; i < n / 2u; i++) nor[off + i] &= s[i];
        longjmp(cut_jb, 1);
    }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"

static int check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int outside_pair(uint32_t obj)
{
    uint32_t a = st_sector(obj, 0), b = st_sector(obj, 1), lo = a < b ? a : b, hi = a < b ? b : a;
    return !memcmp(nor, snap, lo) && !memcmp(nor + lo + 4096u, snap + lo + 4096u, hi - lo - 4096u) &&
           !memcmp(nor + hi + 4096u, snap + hi + 4096u, sizeof nor - hi - 4096u);
}

int main(void)
{
    uint8_t a[300], b[300], c[300], got[301];
    uint32_t obj, i;
    long cut, total, cuts = 0, objects = 0;
    int bad = 0, all_ok = 1;

    for (obj = 0; obj < OBJ_COUNT; obj++) {
        for (i = 0; i < 300u; i++) {
            a[i] = (uint8_t)(obj * 7u + i);
            b[i] = (uint8_t)(obj * 11u + i + 1u);
            c[i] = (uint8_t)(obj * 13u + i + 2u);
        }
        memset(nor, 0xFF, sizeof nor);
        if (st_save(obj, a, 300) != 0 || st_save(obj, b, 300) != 0) {
            printf("object %u: cannot write the two copies\n", obj);
            bad++;
            continue;
        }
        memcpy(snap, nor, sizeof nor);

        cutting = 1; cut_at = -1;
        opno = 0;
        if (!setjmp(cut_jb)) st_save(obj, c, 300);
        total = opno;
        cutting = 0;

        for (cut = 0; cut < total; cut++) {
            int ok = 1;
            memcpy(nor, snap, sizeof nor);
            cutting = 1;
            opno = 0;
            cut_at = cut;
            if (!setjmp(cut_jb)) { st_save(obj, c, 300); ok = 0; }   /* finished before the cut */
            cutting = 0;
            cuts++;
            if (!ok) break;
            /* the previous copy survives, exactly */
            memset(got, 0xAA, sizeof got);
            ok &= st_load(obj, got, 300) == 300 && !memcmp(got, b, 300);
            /* and the object is writable again */
            ok &= st_save(obj, c, 300) == 0;
            memset(got, 0xAA, sizeof got);
            ok &= st_load(obj, got, 300) == 300 && !memcmp(got, c, 300);
            ok &= outside_pair(obj);
            if (!ok) {
                printf("object %u (%s), cut %ld of %ld FAILED\n", obj,
                       obj == OBJ_SETTINGS ? "settings" : obj == OBJ_AUTOSAVE ? "autosave"
                       : obj == OBJ_SONGIDX ? "song index" : obj == OBJ_REEL ? "reel"
                       : obj == OBJ_FM6BANK ? "fm6 bank" : obj == OBJ_UPFM6 ? "fm6 patches" : "data",
                       cut, total);
                bad++;
                all_ok = 0;
                break;
            }
        }
        objects++;
    }
    bad += check("every object: a cut at every erase/page program keeps the old copy, then saves again",
                 all_ok && objects == OBJ_COUNT && cuts >= (long)OBJ_COUNT * 3);
    printf("objects: %ld, cuts: %ld\n", objects, cuts);
    printf("%s\n", bad ? "STORAGE CUT TEST FAILED" : "storage cut test passed");
    return bad != 0;
}
