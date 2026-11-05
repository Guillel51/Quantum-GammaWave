/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of firmware/src/ota.c: a fake M-UPGRADE serves a real .fwsc over
 * the SysEx protocol (pack7, cmd 0x30 reads, 0xE0000000 -> "success") and a
 * simulated NOR plays the flash.
 *   cc -o ota_test tests/ota_test.c && ./ota_test FILE.fwsc
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000];
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024];                 /* one pending host frame (7-bit bytes) */
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, ident_replies, mute_after = 0xFFFFFFFFu, bad_erase;
static int committed;
static uint8_t committed_parm[112];
static int32_t last_code;
static uint8_t *corrupt_at;              /* flip a served byte (loader body) */
static uint32_t fake_jedec = 0x856014u;  /* what the fake NOR answers to 0x9F */
static uint32_t fake_fail_at = 0xFFFFFFFFu;   /* raw read starting here fails */
static uint8_t qry_body[320];
static uint32_t qry_cmd, qry_len;        /* last 0x12/0x13 reply from the device */

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

static void host_reply(const uint8_t *msg, uint32_t n)       /* queue msg as a device-bound frame */
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

/* ---- hooks ---- */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[320] = {0}, m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) { ident_replies++; return 0; }
    if (u[2] == 0x12 || u[2] == 0x13) {          /* identification / read replies: record them */
        uint32_t blen = u[3] | u[4] << 8 | (uint32_t)u[5] << 16;
        if (d != 7u + blen) { printf("bad query reply length %u vs %u\n", d, blen); exit(1); }
        qry_cmd = u[2];
        qry_len = blen;
        memcpy(qry_body, u + 6, blen);
        return 0;
    }
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg cmd %02x len %u\n", u[2], d); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (++requests > mute_after)
        return 0;                                         /* host gone */
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
        if (corrupt_at && corrupt_at >= logical + addr && corrupt_at < logical + addr + len)
            m[14 + (corrupt_at - (logical + addr))] ^= 0x01;
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
    if (off < 0xE0000u || off >= 0xE5000u || (off & 0xFFFu)) { bad_erase++; return -8; }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < 0xE0000u || off + n > 0xE5000u) { bad_erase++; return -8; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static uint32_t ota_jedec(void) { return fake_jedec; }
static int ota_rawread(uint32_t off, void *p, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;                               /* past the 1 MiB part */
    if (fake_fail_at != 0xFFFFFFFFu && fake_fail_at >= off && fake_fail_at - off < n)
        return -5;
    memcpy(p, nor + off, n);
    return 0;
}
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm) { committed = 1; memcpy(committed_parm, parm, 112); }

#include "../firmware/src/ota.c"

static uint32_t ota_body_off(void)                  /* ota.bin data + 0x20 in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, logical, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 100) return ota_rd32(e + 8) + 0x20u;
    }
    return 0;
}

static int check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = 0xE0000; i < 0xE5000; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + 0x400, 0x4000);                 /* device flash head = package flash.bin head */
    rx_full = 0; requests = 0; ident_replies = 0; committed = 0; last_code = 0;
    mute_after = 0xFFFFFFFFu; corrupt_at = NULL; bad_erase = 0;
    fake_jedec = 0x856014u; fake_fail_at = 0xFFFFFFFFu; qry_len = 0; ota_sess = 0; ota_sess_part = 0;
}

/* queue one request frame and let the device answer it */
static void qry(uint32_t cmd, const uint8_t *body, uint32_t n)
{
    uint8_t m[64];
    uint32_t i, s = 0;
    m[0] = 0; m[1] = 0x59; m[2] = (uint8_t)cmd; m[3] = (uint8_t)n; m[4] = 0; m[5] = 0;
    for (i = 0; i < n; i++) { m[6 + i] = body[i]; s += body[i]; }
    m[6 + n] = (uint8_t)~s;
    qry_len = 0; qry_cmd = 0;
    host_reply(m, 7 + n);
    ota_service();
}

static uint32_t qry_ident(uint32_t nonce)      /* -> status of the 0x12 reply */
{
    uint8_t b[7];
    b[0] = 1;
    b[1] = (uint8_t)nonce; b[2] = (uint8_t)(nonce >> 8);
    b[3] = (uint8_t)(nonce >> 16); b[4] = (uint8_t)(nonce >> 24);
    ota_wr16(b + 5, ota_crc16(b, 5, 0));
    qry(0x12, b, 7);
    return qry_body[1];
}

static uint32_t qry_sess(void) { return ota_rd32(qry_body + 17); }

static uint32_t qry_read(uint32_t addr, uint32_t len, uint32_t sess, uint32_t *got)
{
    uint8_t b[13];
    b[0] = 1;
    ota_wr32(b + 1, addr);
    ota_wr16(b + 5, len);
    ota_wr32(b + 7, sess);
    ota_wr16(b + 11, ota_crc16(b, 11, 0));
    qry(0x13, b, 13);
    *got = ota_rd16(qry_body + 2);
    return qry_body[1];
}

int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw;
    size_t n, i;
    int bad = 0, rc;
    if (argc < 2) { printf("usage: ota_test FILE.fwsc\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x4400) { printf("%s: too short for a package\n", argv[1]); return 2; }
    logical = malloc(n);
    for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);   /* drop the marker bytes */
    memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
    logical_len = n - 20;

    /* handshake outside a session */
    reset_dev();
    {
        uint8_t m[7] = {0, 0x59, 0x11, 0, 0, 0, 0xFF};
        host_reply(m, 7);
        ota_service();
        bad += check("handshake answered", ident_replies == 1);
        bad += check("identity frame is " FELUCCA_ID,
                     ota_msg[2] == 0x11 && !memcmp(ota_msg + 6, FELUCCA_ID, sizeof FELUCCA_ID - 1));
    }

    /* full session */
    reset_dev();
    rc = ota_session();
    printf("  %u requests\n", requests);
    bad += check("session commits", rc == 0 && committed);
    bad += check("headers + loader + confirm, no head reads", requests >= 10 && requests <= 30);
    {
        uint8_t *h = nor + 0xE0000, *p = committed_parm;
        bad += check("staged body == the package's loader", !memcmp(nor + 0xE0020, logical + ota_body_off(), 64));
        bad += check("LOADER.BIN head CRC",
                     (h[0] | h[1] << 8) == ota_crc16(h + 2, 30, 0) && !memcmp(h + 16, "LOADER.BIN", 10));
        bad += check("record at 0xE4F00 == RAM record", !memcmp(nor + 0xE4F00, p, 112));
        bad += check("record CRC / type / magic / ota_addr",
                     (p[0] | p[1] << 8) == ota_crc16(p + 2, 78, 0) && (p[2] | p[3] << 8) == 0x5A0D &&
                     (p[6] | p[7] << 8) == 0x5441 && ota_rd32(p + 72) == 0xE0000);
        bad += check("nothing written outside 0xE0000..0xE4FFF", bad_erase == 0);
    }
    /* boot cleanup erases it */
    bad += check("boot cleanup erases the area", ota_boot_cleanup() == 1 && area_erased());
    bad += check("second boot: nothing to do", ota_boot_cleanup() == 0);

    /* host disappears in the middle */
    reset_dev();
    mute_after = 4;
    rc = ota_session();
    bad += check("host gone -> no commit, area clean", rc < 0 && !committed && area_erased());

    /* host never confirms */
    reset_dev();
    mute_after = 0xFFFFFFFFu;
    {
        uint32_t total;
        rc = ota_session();
        total = requests;                                  /* requests of a full run incl. confirm */
        reset_dev();
        mute_after = total - 1;                            /* answer everything but the confirmation */
        rc = ota_session();
        bad += check("no 'success' -> -13, no commit, area clean", rc == -13 && !committed && area_erased());
    }

    /* ---- 0x12: physical identification ---- */
    reset_dev();
    fake_jedec = 0;                                        /* the NOR does not answer */
    bad += check("0x12 no NOR answer -> status 1, no session", qry_ident(1) == 1 && qry_len == 24u && qry_sess() == 0);
    reset_dev();
    fake_jedec = 0x123456u;                                /* not a part we know */
    bad += check("0x12 unknown part -> status 2, no session", qry_ident(2) == 2 && qry_sess() == 0);
    bad += check("0x12 unknown part echoes the id, no geometry",
                 qry_body[2] == 0x12 && qry_body[3] == 0x34 && qry_body[4] == 0x56 &&
                 qry_body[5] == 0 && qry_body[9] == 0 && qry_body[11] == 0);
    reset_dev();
    bad += check("0x12 known part -> status 0", qry_ident(3) == 0 && qry_cmd == 0x12u && qry_len == 24u);
    bad += check("0x12 reports the id it read (0x856014)",
                 qry_body[2] == 0x85 && qry_body[3] == 0x60 && qry_body[4] == 0x14);
    bad += check("0x12 geometry: 1 MiB / 256 / 4K / 64K",
                 ota_rd32(qry_body + 5) == 0x100000u && ota_rd16(qry_body + 9) == 256u &&
                 ota_rd16(qry_body + 11) == 0x1000u && ota_rd32(qry_body + 13) == 0x10000u);
    bad += check("0x12 reply crc + session", ota_crc16(qry_body, 21, 0) == ota_rd16(qry_body + 21) && qry_sess() != 0);
    {
        uint32_t s1 = qry_sess(), s2;
        bad += check("0x12 second read: same part and geometry", qry_ident(3) == 0 && ota_rd32(qry_body + 5) == 0x100000u &&
                     qry_body[2] == 0x85 && qry_body[4] == 0x14);
        s2 = qry_sess();
        bad += check("0x12 second read: fresh session token", s2 != 0 && s2 != s1);
        bad += check("0x13 reads what the NOR holds",
                     qry_read(0x400, 256, s2, &qry_len) == 0 && qry_len == 256u && !memcmp(qry_body + 14, nor + 0x400, 256));
        bad += check("0x13 echoes address and session",
                     ota_rd32(qry_body + 4) == 0x400u && ota_rd32(qry_body + 8) == s2 &&
                     ota_crc16(qry_body, 12, 0) == ota_rd16(qry_body + 12));
        bad += check("0x13 old session refused", qry_read(0x400, 16, s1, &qry_len) == 4 && qry_len == 0);
        bad += check("0x13 past the end refused", qry_read(0xFFFF0u, 32, s2, &qry_len) == 5 && qry_len == 0);
        bad += check("0x13 zero length refused", qry_read(0x400, 0, s2, &qry_len) == 6);
        bad += check("0x13 over the 512-byte cap refused", qry_read(0x400, 513, s2, &qry_len) == 6);
        bad += check("0x13 more than one chunk refused, no surprise short read", qry_read(0x400, 257, s2, &qry_len) == 6);
        fake_fail_at = 0x750u;                             /* fails in the middle of the chunk */
        bad += check("0x13 read error reported, no payload", qry_read(0x700, 256, s2, &qry_len) == 6 && qry_len == 0);
        fake_fail_at = 0xFFFFFFFFu;
        bad += check("0x13 last chunk at the very end works", qry_read(0xFFF00u, 256, s2, &qry_len) == 0 && qry_len == 256);
    }
    {
        uint8_t b[7] = {1, 9, 0, 0, 0, 0, 0};
        reset_dev();
        qry(0x12, b, 7);                                   /* bad request crc */
        bad += check("0x12 bad request crc -> status 7", qry_body[1] == 7 && qry_sess() == 0);
        b[5] = (uint8_t)ota_crc16(b, 5, 0); b[6] = (uint8_t)(ota_crc16(b, 5, 0) >> 8);
        b[0] = 2;                                          /* wrong protocol version */
        qry(0x12, b, 7);
        bad += check("0x12 wrong version -> status 7", qry_body[1] == 7 && qry_sess() == 0);
        reset_dev();
        bad += check("0x13 without a session refused", qry_read(0, 16, 0x1234u, &qry_len) == 4);
        bad += check("identification and reads write nothing",
                     bad_erase == 0 && !committed && nor[0xE0000] == 0xFF && nor[0xE4F00] == 0xFF);
    }

    printf("%s\n", bad ? "OTA TEST FAILED" : "ota test passed");
    return bad != 0;
}
