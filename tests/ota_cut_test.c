/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Power-cut matrix for firmware/src/ota.c (M-UPGRADE staging).
 *
 * ota_test.c covers a host that disappears or never confirms. This one covers the
 * other half of a worst case: the power goes in the middle of the staging writes,
 * so ota_session() never returns and its own ota_unstage() never runs. The state
 * left in NOR is then what a real unit would boot with. For every erase and every
 * page program of a real .fwsc:
 *
 *   - the cut is applied half way through that operation, then execution stops
 *     (longjmp: no cleanup code runs, as with a real power loss);
 *   - the device reboots and runs ota_boot_cleanup() as main.c does;
 *   - nothing may have been committed, and the staging area must be erased;
 *   - a fresh session on the same NOR must still install and commit;
 *   - no byte outside 0xE0000..0xE4FFF may have changed.
 *
 * A second pass cuts the boot cleanup itself (a power loss while recovering from
 * a power loss) and requires the next boot to finish the job.
 *
 *   cc -o ota_cut_test tests/ota_cut_test.c && ./ota_cut_test FILE.fwsc
 */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "FM-1_965"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000];
static uint8_t clean[0x100000];          /* the state before a cut, compared outside the staging area */
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, bad_write;
static int committed;
static jmp_buf cut_jb;
static long cut_at = -1, opno;
static int cutting;

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}

static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static void host_reply(const uint8_t *msg, uint32_t n)
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

/* ---- hooks (the same contract ota_test.c uses) ---- */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64] = {0}, m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg cmd %02x len %u\n", u[2], d); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    requests++;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0;
    m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    host_reply(m, 15 + len);
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len;
    return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms += 1; }
static int ota_erase(uint32_t off)
{
    if (off < 0xE0000u || off >= 0xE5000u || (off & 0xFFFu)) { bad_write++; return -8; }
    if (cutting && opno++ == cut_at) {                  /* the power goes in the erase: half a sector */
        memset(nor + off, 0xFF, 2048);
        longjmp(cut_jb, 1);
    }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < 0xE0000u || off + n > 0xE5000u) { bad_write++; return -8; }
    if (cutting && opno++ == cut_at) {                  /* half the page programmed */
        for (i = 0; i < n / 2u; i++) nor[off + i] &= s[i];
        longjmp(cut_jb, 1);
    }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; committed = 1; }

#include "../firmware/src/ota.c"

static int check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + 0x400, 0x4000);                 /* the head the installer never rewrites */
    rx_full = 0; requests = 0; committed = 0; bad_write = 0;
    now_ms = 0;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = 0xE0000; i < 0xE5000; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

/* what the SPL would actually run: a valid LOADER.BIN head, or a valid UPDATA_PARM record.
 * Leftover body bytes without the head are inert (the next update erases the whole area first),
 * so a cut before the head is not a failure: the invariant is that neither can be launched. */
static int head_valid(void)
{
    const uint8_t *h = nor + 0xE0000;
    return h[0] != 0xFFu && ota_crc16(h + 2, 30, 0) == ota_rd16(h) && !memcmp(h + 16, "LOADER.BIN", 10);
}
static int record_valid(void)
{
    const uint8_t *p = nor + 0xE4F00;
    return p[0] != 0xFFu && ota_crc16(p + 2, 78, 0) == ota_rd16(p) && (p[2] | p[3] << 8) == 0x5A0Du;
}
static int staging_inert(void) { return !head_valid() && !record_valid(); }

static int outside_untouched(void)
{
    return !memcmp(nor, clean, 0xE0000u) && !memcmp(nor + 0xE5000u, clean + 0xE5000u, 0x1B000u);
}

static int run_session(void)                  /* 1 = the power was cut, 0 = ota_session() returned */
{
    int cut;
    opno = 0;
    cut = 0;
    if (!setjmp(cut_jb))
        ota_session();
    else
        cut = 1;
    return cut;
}

int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw;
    size_t n, i;
    int bad = 0, rc;
    long total, cut, cuts = 0, cleanup_cuts = 0;
    uint8_t *logical_tmp;

    if (argc < 2) { printf("usage: ota_cut_test FILE.fwsc\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x4400) { printf("%s: too short for a package\n", argv[1]); return 2; }
    logical_tmp = malloc(n);
    for (i = 0; i < 20; i++) memcpy(logical_tmp + i * 47, raw + i * 48, 47);
    memcpy(logical_tmp + 20 * 47, raw + 20 * 48, n - 20 * 48);
    logical = logical_tmp;
    logical_len = n - 20;

    /* how many flash operations a clean session performs */
    reset_dev();
    memcpy(clean, nor, sizeof nor);
    cutting = 1; cut_at = -1;
    rc = run_session();
    total = opno;
    cutting = 0;
    bad += check("a clean session commits", rc == 0 && committed);
    bad += check("a clean session stages in 20..80 flash operations", total >= 20 && total <= 80);
    printf("  %ld flash operations, %u requests\n", total, requests);

    for (cut = 0; cut < total; cut++) {
        reset_dev();
        memcpy(clean, nor, sizeof nor);
        cutting = 1; cut_at = cut;
        rc = run_session();
        cutting = 0;
        cuts++;
        if (!rc) { printf("  cut %ld: the session finished before it (operation count moved)\n", cut); break; }
        if (committed) { printf("  cut %ld: COMMITTED after a power cut\n", cut); bad++; break; }
        if (!outside_untouched()) { printf("  cut %ld: wrote outside the staging area\n", cut); bad++; break; }
        /* reboot: main.c's ota_boot_cleanup, then nothing may be launchable */
        ota_boot_cleanup();
        if (!staging_inert()) { printf("  cut %ld: a loader head or update record survived the boot\n", cut); bad++; break; }
        committed = 0;
        rx_full = 0;
        now_ms = 0;
        rc = run_session();
        if (rc != 0 || !committed) { printf("  cut %ld: a fresh session cannot recover\n", cut); bad++; break; }
        ota_boot_cleanup();
        if (!staging_inert()) { printf("  cut %ld: recovery left something launchable\n", cut); bad++; break; }
    }
    bad += check("a power cut at every staging erase/program: no commit, nothing launchable, next update works",
                 cuts == total && cuts >= 20);

    /* a cut while the boot cleanup itself is erasing (a cut during recovery) */
    reset_dev();
    memcpy(clean, nor, sizeof nor);
    cutting = 1; cut_at = total - 1;                   /* body and LOADER.BIN head written, no record */
    run_session();
    cutting = 0;
    bad += check("the cleanup-cut setup leaves a launchable head behind", head_valid() && !record_valid());
    {
        int ok = 1, k;
        for (k = 0; k < 3; k++) {
            cutting = 1; opno = 0; cut_at = k;         /* the power goes in the k-th erase of the cleanup */
            if (!setjmp(cut_jb)) {
                ota_boot_cleanup();
                cutting = 0;
                break;                                 /* finished before the cut */
            }
            cutting = 0;
            cleanup_cuts++;
            ok &= !committed && staging_inert();
        }
        ota_boot_cleanup();                            /* the next boot: finish or confirm inert */
        ok &= staging_inert();
        committed = 0;
        rx_full = 0;
        now_ms = 0;
        ok &= run_session() == 0 && committed;         /* and an update still goes through */
        ota_boot_cleanup();
        ok &= staging_inert() && area_erased();
        bad += check("a power cut while the boot cleanup erases: nothing launchable, updates still work", ok);
    }
    bad += check("the cleanup cut actually landed in an erase", cleanup_cuts >= 1);

    printf("cuts tested: %ld staging + %ld cleanup\n", cuts, cleanup_cuts);
    printf("%s\n", bad ? "OTA CUT TEST FAILED" : "ota cut test passed");
    return bad != 0;
}
