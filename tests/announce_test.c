/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3, issue #105) THE SCREEN READER'S PROTOCOL, measured: firmware/src/announce.c against the
 * fm1-reader PROTOCOL.md (version 1). The wire is a stub here -- the test is the protocol and the USB-MIDI
 * framing (ann_pack), not the USB device, which no host test can be.
 *
 * Class: host x86-64, the real source. No FM-1, no reader page, no browser.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("announce: %-74s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the wire: every complete F0..F7 message the device sends, captured */
#define WIRE 16
static uint8_t wire[WIRE][300];
static uint32_t wire_n[WIRE], wire_c;
static int ann_wire_send(const uint8_t *p, uint32_t n)
{
    if (wire_c >= WIRE || n > sizeof wire[0])
        return 1;
    memcpy(wire[wire_c], p, n);
    wire_n[wire_c++] = n;
    return 0;
}
#define FELUCCA_VERSION "v-test"
#include "../firmware/src/announce.c"

static void reset(void) { wire_c = 0; memset(&ann, 0, sizeof ann); }
static uint32_t last_len(void) { return wire_c ? wire_n[wire_c - 1] : 0; }
static const uint8_t *last(void) { return wire_c ? wire[wire_c - 1] : (const uint8_t *)""; }
static int is_say(uint32_t i, const char *text, uint32_t flags)
{
    const uint8_t *p = wire[i];
    uint32_t n = wire_n[i], k;
    if (n < 8u || p[0] != 0xF0 || p[1] != 0x7D || p[2] != 0x53 || p[3] != 0x52 || p[4] != ANN_SAY ||
        p[5] != flags || p[n - 1] != 0xF7)
        return 0;
    if (n - 7u != strlen(text))
        return 0;
    for (k = 0; k < n - 7u; k++)
        if (p[6 + k] != (uint8_t)text[k])
            return 0;
    return 1;
}
static uint32_t hello(const uint8_t ver, uint32_t now)
{
    uint8_t f[5] = {0x7D, 0x53, 0x52, ANN_HELLO, ver};
    return (uint32_t)ann_frame(f, sizeof f, now);
}

int main(void)
{
    uint8_t f[5];

    /* silent until asked */
    reset();
    ann_say("HOME MAIN", 0, ANN_NOW);
    ann_later("LEVEL 96", ANN_NOW, 120u, 0);
    ann_tick(1000);
    check("silent before HELLO: no reader, no bytes", !wire_c && !ann_on());

    /* HELLO -> the answer (name, version, 0, link), and the screen is due */
    check("HELLO frame accepted", hello(1, 10) == 1);
    check(".. the answer is HELLO version, name, 0, (empty link)",
          wire_c == 1 && wire[0][0] == 0xF0 && wire[0][1] == 0x7D && wire[0][2] == 0x53 && wire[0][3] == 0x52 &&
          wire[0][4] == ANN_HELLO && wire[0][5] == ANN_VER && memcmp(&wire[0][6], ANN_NAME, strlen(ANN_NAME)) == 0 &&
          wire[0][6 + strlen(ANN_NAME)] == 0 && wire[0][7 + strlen(ANN_NAME)] == 0xF7 &&
          last_len() == 8 + strlen(ANN_NAME));
    check("a new reader means WHERE: the screen is due once", ann_where() && !ann_where());

    /* SAY: text, and NOW cuts, KEEP is a warning */
    reset();
    hello(1, 0);
    ann_say("MIXER", 0, ANN_NOW);
    check("SAY NOW: the page's own words, no separator", is_say(1, "MIXER", ANN_NOW));
    ann_say("DISK", " FULL", ANN_KEEP);
    check("SAY KEEP: two parts joined byte for byte", is_say(2, "DISK FULL", ANN_KEEP));

    /* a knob: replaced while it turns, said once it settles (~120 ms), and at least every 4 x quietly */
    reset();
    hello(1, 0);
    ann_later("LEVEL 96", ANN_NOW, 120u, 1000);
    ann_later("LEVEL 97", ANN_NOW, 120u, 1060);
    ann_tick(1100);
    check("a knob still turning is not said yet", wire_c == 1);
    ann_tick(1200);
    check(".. and is said once it has been still for 120 ms", wire_c == 2 && is_say(1, "LEVEL 97", ANN_NOW));
    ann_later("REV 10", ANN_NOW, 120u, 2000);
    ann_later("REV 11", ANN_NOW, 120u, 2400);
    ann_later("REV 12", ANN_NOW, 120u, 2480);
    ann_tick(2481);
    check("a long turn is heard as it goes (every 4 x quietly)", wire_c == 3 && is_say(2, "REV 12", ANN_NOW));

    /* a queued line waits for the one that landed before it */
    reset();
    hello(1, 0);
    ann_later("WAIT", 0, 120u, 0);
    ann_say("PAGE", 0, 0);
    check("what waited goes out before the new line", wire_c == 3 && is_say(1, "WAIT", 0) && is_say(2, "PAGE", 0));

    /* too long: cut at a word, never mid-word */
    reset();
    hello(1, 0);
    {
        char big[ANN_MAX + 40];
        uint32_t i, n;
        for (i = 0; i < sizeof big - 1u; i++)
            big[i] = (i % 7u) == 6u ? ' ' : 'A';
        big[sizeof big - 1u] = 0;
        ann_say(big, 0, ANN_NOW);
        n = last_len();
        check("a line over ANN_MAX is cut at a word", n <= 6u + ANN_MAX + 1u && last()[n - 2] != ' ' && last()[n - 1] == 0xF7);
    }

    /* WHERE, BYE and the 5 s timeout */
    reset();
    hello(1, 0);
    f[0] = 0x7D; f[1] = 0x53; f[2] = 0x52; f[3] = ANN_WHERE; f[4] = 0;
    check("WHERE is the reader's too (and asked once)", ann_frame(f, 5, 100) == 1 && ann_where() && !ann_where());
    ann_tick(4000);
    check("a reader heard 4 s ago is still there", ann_on());
    ann_tick(5001);
    check("5 s without a HELLO and the device stops", !ann_on());
    reset();
    hello(1, 0);
    f[3] = ANN_BYE;
    check("BYE stops it", ann_frame(f, 5, 10) == 1 && !ann_on());

    /* not ours: another manufacturer, another protocol, a truncated frame */
    reset();
    f[0] = 0x7E; f[1] = 0x53; f[2] = 0x52; f[3] = ANN_HELLO; f[4] = 1;
    check("another manufacturer's frame is not the reader's", ann_frame(f, 5, 0) == 0 && !wire_c);
    f[0] = 0x7D; f[1] = 0x53; f[2] = 0x51;
    check("another \"SR\"-looking id is not the reader's", ann_frame(f, 5, 0) == 0 && !wire_c);
    check("a short frame is not the reader's", ann_frame(f, 3, 0) == 0);

    /* USB-MIDI framing: CIN 4 continuing, 5/6/7 the last packet, 1..3 bytes; every byte 7-bit between F0/F7 */
    {
        uint8_t m[6] = {0xF0, 0x7D, 0x53, 0x52, 0x01, 0xF7};
        uint32_t pkt[4], n = ann_pack(m, 6, pkt, 4);
        check("ann_pack: 6 bytes -> 2 USB-MIDI packets", n == 2);
        check(".. the first continues (CIN 4) with 3 bytes", (pkt[0] & 15u) == 4u && ((pkt[0] >> 8) & 0xFFu) == 0xF0 &&
              ((pkt[0] >> 16) & 0xFFu) == 0x7D && ((pkt[0] >> 24) & 0xFFu) == 0x53);
        check(".. the last ends the SysEx (CIN 7)", (pkt[1] & 15u) == 7u && ((pkt[1] >> 8) & 0xFFu) == 0x52 &&
              ((pkt[1] >> 16) & 0xFFu) == 0x01 && ((pkt[1] >> 24) & 0xFFu) == 0xF7);
    }
    {
        uint8_t m[4] = {0xF0, 0x7D, 0xF7, 0};
        uint32_t pkt[2], n = ann_pack(m, 3, pkt, 2);
        check("ann_pack: a 3-byte message is one CIN 7 packet", n == 1 && (pkt[0] & 15u) == 7u);
        n = ann_pack(m, 1, pkt, 2);
        check("ann_pack: a 1-byte message is CIN 5", n == 1 && (pkt[0] & 15u) == 5u && ((pkt[0] >> 8) & 0xFFu) == 0xF0);
    }

    printf("announce_test: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
