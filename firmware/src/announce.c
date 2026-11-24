/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (FASE 3, issue #105) THE SCREEN, IN WORDS: the FM-1 Screen Reader protocol (version 1) of
 * charlesvestal/fm1-reader, implemented here so that the page — or the computer's own screen reader —
 * can say what Quantum GammaWave is showing: the page and its section, a knob's value as it turns, the
 * sound a track loaded and every message the instrument puts on its top bar.
 *
 * Protocol (fm1-reader/PROTOCOL.md, "FM-1 screen reader protocol (version 1)"): SysEx over USB-MIDI,
 * F0 7D 53 52 <cmd> <args..> F7. 7D is the non-commercial manufacturer id, 53 52 is "SR"; every byte
 * between F0 and F7 is 7-bit, so a device that does not implement it ignores the frames.
 *
 *   reader -> device   1 HELLO ver   listening (again every ~2 s; 5 s without one and the device stops)
 *                      2 BYE          stop
 *                      3 WHERE        say everything on the screen now
 *   device -> reader   1 HELLO ver, name, 0, words link   (the answer to each HELLO)
 *                      16 SAY flags, text   (flags 1 NOW: cut off what is being said; 2 KEEP: a warning)
 *
 * SILENT, STANDING STILL. Nothing is built, formatted or sent while no reader has said HELLO: `ann_on()`
 * is 0 and every entry point returns before touching a string, so a DAW on the same port never sees a
 * byte and the audio path pays one load. This layer owns no buffer for the radio, no allocation and no
 * flash access; its RAM is the struct below (a queued line and two timers). The firmware says what its
 * screen literally says ("ANALOG BRASS", "LEVEL 96"), and the reader speaks it: the words file is
 * optional and this build publishes none yet (ANN_WORDS_URL is empty, which the protocol allows: the
 * reader then applies its own common rules). What is announced, and what is not yet, is in
 * docs/ACCESSIBILITY.md.
 *
 * The includer supplies ann_wire_send(p, n): one complete F0..F7 message, 0 = sent, non-zero = there was
 * no room in the USB queue (the message is dropped, as the protocol asks: the device never waits).
 * usb.c owns it (the SysEx ring, sent before any MIDI). usb.c also feeds ann_frame() with the bytes a
 * received F0..F7 frame carried, and the UI calls ann_tick()/ann_page()/ann_value()/ann_message().
 */
#ifndef FELUCCA_ANNOUNCE
#define FELUCCA_ANNOUNCE 1
#endif
#ifndef ANN_MAX
#define ANN_MAX 120u                    /* text in one SAY (the protocol allows 240; our longest line is ~40) */
#endif
#ifndef ANN_NAME
#ifdef FELUCCA_VERSION
#define ANN_NAME "Quantum GammaWave " FELUCCA_VERSION
#else
#define ANN_NAME "Quantum GammaWave"
#endif
#endif
#ifndef ANN_WORDS_URL
/* (FASE 8) the words this build was made with: firmware/words.tsv, turned into this file by
 * tools/gen_words.py and published with the site. The name is the fingerprint of its content, so it never
 * moves and a reader that kept it keeps being right; `gen_words.py --check` fails if the two disagree. */
#ifndef ANN_WORDS_URL
#define ANN_WORDS_URL "https://guillel51.github.io/Quantum-GammaWave/sr/w/2ab0d08f.json"
#endif
#endif
#define ANN_VER 1u
#define ANN_H0 0x7Du                    /* the non-commercial manufacturer id */
#define ANN_H1 0x53u                    /* 'S' */
#define ANN_H2 0x52u                    /* 'R' */
#define ANN_NOW 1u
#define ANN_KEEP 2u
#define ANN_GONE_MS 5000u
enum { ANN_HELLO = 1, ANN_BYE = 2, ANN_WHERE = 3, ANN_SAY = 16 };

static int ann_wire_send(const uint8_t *p, uint32_t n);      /* usb.c */

static struct {
    uint8_t on;                         /* a reader is listening (heard a HELLO in the last 5 s) */
    uint8_t where;                      /* the reader asked for the whole screen */
    uint8_t later_set, later_flags;
    uint32_t heard;                     /* the last HELLO, ms */
    uint32_t later_t, later_first, later_quiet;
    char later[ANN_MAX + 1];            /* the newest of a run (a knob turning), said when it settles */
} ann;

static int ann_on(void) { return ann.on; }
/* one frame out of its parts: a, then (HELLO only) a 0 byte, then b. Too long = cut at the last space */
static void ann_send(uint32_t cmd, uint32_t flags, const char *a, const char *b)
{
    uint8_t f[8 + ANN_MAX];
    char w[ANN_MAX + 1];
    uint32_t k = 0, i = 0, two = cmd == ANN_HELLO, n = 0;
    for (; a; a = b, b = 0) {
        for (i = 0; a[i] && k < ANN_MAX; i++)
            w[k++] = a[i];
        if (a[i]) {                     /* too long: back to the last space */
            while (k && w[k - 1] != ' ')
                k--;
            while (k && w[k - 1] == ' ')
                k--;
            break;
        }
        if (two && b && k < ANN_MAX)
            w[k++] = 1;                 /* the separator, written as 0 below */
    }
    if (two && b) {                     /* the parts were cut: no link, no dangling separator */
        while (k && w[k - 1] == 1)
            k--;
    }
    w[k] = 0;
    f[n++] = 0xF0u;
    f[n++] = ANN_H0;
    f[n++] = ANN_H1;
    f[n++] = ANN_H2;
    f[n++] = (uint8_t)cmd;
    f[n++] = (uint8_t)(flags & 0x7Fu);
    for (i = 0; w[i]; i++)
        f[n++] = w[i] == 1 ? 0u : (uint8_t)w[i] & 0x7Fu;
    f[n++] = 0xF7u;
    ann_wire_send(f, n);
}

/* what is on the screen, in words, now (a page, an answer). b may be 0 */
static void ann_say(const char *a, const char *b, uint32_t flags)
{
    if (!ann.on || !a || !*a)
        return;
    if (ann.later_set && !(flags & ANN_NOW))     /* in order: what was waiting first */
        ann_send(ANN_SAY, ann.later_flags, ann.later, 0);
    ann.later_set = 0;
    ann_send(ANN_SAY, flags, a, b);
}

/* say s once it has stayed the newest for quiet_ms (each call replaces what waits; a run that keeps
 * changing is said at least every 4 x quiet_ms, so a knob turned for long is heard as it goes) */
static void ann_later(const char *s, uint32_t flags, uint32_t quiet_ms, uint32_t now)
{
    uint32_t i;
    if (!ann.on || !s)
        return;
    for (i = 0; s[i] && i < ANN_MAX; i++)
        ann.later[i] = s[i];
    ann.later[i] = 0;
    if (!ann.later_set)
        ann.later_first = now;
    ann.later_set = 1;
    ann.later_flags = (uint8_t)flags;
    ann.later_t = now;
    ann.later_quiet = quiet_ms;
}

/* the reader asked for the whole screen: 1 once */
static int ann_where(void)
{
    int w = ann.where;
    ann.where = 0;
    return w && ann.on;
}

static void ann_tick(uint32_t now)
{
    if (ann.on && now - ann.heard > ANN_GONE_MS) {
        ann.on = 0;
        ann.later_set = 0;
    }
    if (ann.later_set && (now - ann.later_t >= ann.later_quiet || now - ann.later_first >= 4u * ann.later_quiet)) {
        ann.later_set = 0;
        ann_send(ANN_SAY, ann.later_flags, ann.later, 0);
    }
}

/* a received SysEx (the bytes between F0 and F7): 1 if it was the reader's */
static int ann_frame(const uint8_t *p, uint32_t n, uint32_t now)
{
    if (n < 4u || !p || p[0] != ANN_H0 || p[1] != ANN_H1 || p[2] != ANN_H2)
        return 0;
    switch (p[3]) {
    case ANN_HELLO:
        if (!ann.on)
            ann.where = 1;              /* a new reader: what is on the screen first */
        ann.on = 1;
        ann.heard = now;
        ann_send(ANN_HELLO, ANN_VER, ANN_NAME, ANN_WORDS_URL);
        break;
    case ANN_BYE:
        ann.on = 0;
        ann.later_set = 0;
        break;
    case ANN_WHERE:
        ann.where = 1;
        break;
    default:
        break;
    }
    return 1;
}

#if FELUCCA_ANNOUNCE
/* one message as USB-MIDI SysEx event packets (CIN 4 continuing, 5/6/7 the last one, 1..3 bytes).
 * Shared with usb.c's ann_wire_send, so the framing is the tested part (tests/announce_test.c). */
static uint32_t ann_pack(const uint8_t *p, uint32_t n, uint32_t *out, uint32_t max)
{
    uint32_t i, k = 0;
    for (i = 0; i < n && k < max; i += 3u) {
        uint32_t m = n - i >= 3u ? 3u : n - i;
        uint32_t cin = m == 3u ? (i + 3u < n ? 4u : 7u) : 4u + m;
        out[k++] = cin | (uint32_t)p[i] << 8 | (m > 1u ? (uint32_t)p[i + 1] << 16 : 0u) |
                   (m > 2u ? (uint32_t)p[i + 2] << 24 : 0u);
    }
    return k;
}
#endif
