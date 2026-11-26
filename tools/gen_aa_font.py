#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Pre-rasterise TTF fonts into anti-aliased 4-bit alpha glyph tables for the UI (run by
tools/build.py generate() with --preset inter-tight).

  gen_aa_font.py OUT.h --preset standin|inter-tight|chakra-petch [--tracking EM] [--gamma G] [--kern-min N]
  gen_aa_font.py OUT.h --face NAME=FONT[#index]:PX:FIRST-LAST[+0xNN,...] ...   (explicit faces)

Faces of the presets (S labels, M values/headers, L big numerals / titles):
  standin     an installed Inter (~/Library/Fonts/Inter.ttc, else Inter in fonts/ next to tools/) with
              -2.5 % tracking: a stand-in for Inter Tight, for previews only.
  inter-tight assets/fonts/InterTight[wght].ttf (SIL OFL 1.1, Google Fonts) at weights 400 / 500 / 600.
  chakra-petch assets/fonts/ChakraPetch-Medium / SemiBold / Bold.ttf (SIL OFL 1.1): JIANT's face since 0.5.

Output (a C header; the glyph / font types live in the renderer, firmware/src/gfx.c):
  AF_<N>_DATA   4-bit alpha, trimmed to the ink box, rows back to back, each glyph a byte-aligned bit stream
                (MSB first) of canonical Huffman codes (AF_<N>_HC); a symbol is a value and a repeat count:
                a run of 1..16 zeros or 15s, or one value 1..14 (huff_pack; about 28 % less than 2 px per byte)
  AF_<N>_HC     the code: the number of codes of each length 1..15, then the symbols in code order
                ((count - 1) << 4 | value)
  AF_<N>_G      aag_t {off, adv (1/16 px), bx, by, bw, bh} per glyph and phase (PHASES of S M, PHASES_L of L:
                the glyph drawn at a pen of +0, +1/phases .. px, next to each other), ASCII 32..126, then the
                extras (Latin-1 by their code; the ellipsis U+2026 as code 0x85, as in Windows-1252)
  AF_<N>_KERN / _KD  kerning pairs (key = a<<8|b, sorted) and their deltas in 1/16 px (GPOS of the weight)
  AF_<N>        aafont_t (its last field: log2 of the phases)
Prints the byte cost of every face.
"""
import argparse
import heapq
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import aa_raster as ar  # noqa: E402

FONTS = Path(__file__).resolve().parents[1] / "fonts"
LOCAL_INTER = os.path.expanduser("~/Library/Fonts/Inter.ttc")   # 4.001: 0 Regular, 10 Medium, 12 SemiBold
# (v1.5.14) ONLY WHAT THE FIRMWARE DRAWS. The face used to carry (c) deg . A: O: U: a: o: u: (Latin-1) as well, and
# nothing in firmware/src or in the generated tables ever puts one of those in a string: nine glyphs x four phases of
# ink and their table entries, in every face, for characters no screen can show (tests/font_charset_test.py checks it
# now, so a string that needs one has to add it back here). The ellipsis stays: ui_draw.c draws it.
EXTRAS = [0x2026]
CODE = {0x2026: 0x85}                  # characters outside Latin-1 -> their one-byte code in the firmware
# (3.0) DEEP: the glyphs of a face that get twice the phases. A thin stem or a bowl can sit a whole step (1/4 px)
# off in a pair -- 'Gl' read 0.258 against the 0.25 limit -- and the fix is a finer grid only where the face needs
# it: the union of the letters of every S pair at 0.15 px or more. The face rasterises at twice the phases and
# keeps, per glyph, the even ones for the rest (the same 0, 1/4, 1/2, 3/4 grid, so their bitmaps do not change) and
# all of them for these; AF_<N>_GI is each glyph's first entry and AF_<N>_X its log2 phases.
DEEP = {"S": "GRacdehilmnopqrstvxy"}
PHASES, PHASES_M, PHASES_L = 4, 2, 2  # horizontal phases per glyph (aa_raster.raster_font): S, M, L. (4.2) M went to
                                       # 2: it is the monospace face, every advance is 9 px, and its fourth-phase cells
                                       # were 8 KB of flash for a quarter of a pixel.
# faces stored Huffman-coded (--huff): M, L and S. S was left raw until now -- 2 px per byte, so the labels
# drew at full speed -- because it is the most drawn face and the least compressible (-15 %); with the flash
# slot down to 8 B free, 1.4 KiB of constants that decode to the same pixels is the only headroom the firmware
# can still get without giving up a feature, and the text path is signature-gated (a label redraws when it
# changes, not once a frame), so the decode sits in a path with margin
HUFF = [("M", "L", "S")]
# L draws only "JIANT" (ABOUT), the UPDATE MODE countdown digit, the calibration's control names (panel.c
# B_NAME / E_NAME) and MENU > LARGE's card values and page titles (ui_draw.c draw_column_tall, ui_graph.c
# graph_title: # . / J W too; a value with another character is set in M): a sparse face of those glyphs, the
# space as its range and the rest as extras
# (ui_test.c checks that every L string is covered)
# (JIANT 0.6.56) a face's ink raised by n px in its line: IBM Plex Mono's taller ascent put its values 1 px under where
# Chakra Petch's sat (tests/ui_render.c alignment: a value centred in its window, its unit on its baseline)
RISE = {"IBMPlexMono-SemiBold.ttf": 1}
UI_PRESET = "jiant-lab"            # the firmware's faces (tools/build.py; tests/text_spacing_test.py)
L_CHARS = " #+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXY"


def preset(name):
    if name == "chakra-petch":                 # JIANT 0.5: a tech face (SIL OFL 1.1, Google Fonts)
        v = str(FONTS.parent / "assets" / "fonts" / "ChakraPetch-")
        return [("S", v + "Medium.ttf", 12, (32, 125), EXTRAS, -0.055, PHASES),
                ("M", v + "SemiBold.ttf", 15, (32, 125), EXTRAS, -0.05, PHASES),
                ("L", v + "Bold.ttf", 28, (32, 32), [ord(c) for c in L_CHARS[1:]], 0.0, PHASES_L)]
    if name == "jiant-lab":                    # JIANT 0.6.56: the landing's faces (SIL OFL 1.1, Google Fonts): IBM Plex
        f = FONTS.parent / "assets" / "fonts"  # Mono for labels and values (its digits on the grid Chakra Petch had: 7 / 9
        return [("S", str(f / "ChakraPetch-Medium.ttf"), 12, (32, 125), EXTRAS, -0.055, PHASES),     # px), Michroma for the
                ("M", str(f / "IBMPlexMono-SemiBold.ttf"), 15, (32, 125), EXTRAS, 0.0, PHASES_M), # big titles
                ("L", str(f / "Michroma-Regular.ttf"), 18, (32, 32), [ord(c) for c in L_CHARS[1:]], 0.0, PHASES_L)]
    if name == "inter-tight":
        v = str(FONTS.parent / "assets" / "fonts" / "InterTight[wght].ttf")   # SIL OFL 1.1, Google Fonts
        return [("S", v + "@400", 12, (32, 125), EXTRAS, 0.0, PHASES),
                ("M", v + "@500", 15, (32, 125), EXTRAS, 0.0, PHASES),
                ("L", v + "@600", 28, (32, 32), [ord(c) for c in L_CHARS[1:]], 0.0, PHASES_L)]
    if name == "standin":
        if os.path.exists(LOCAL_INTER):
            reg, med, semi = LOCAL_INTER + "#0", LOCAL_INTER + "#10", LOCAL_INTER + "#12"
        else:                                  # the repo's Inter 3.019 (only Regular + Light)
            reg = med = semi = str(FONTS / "Inter-Regular.ttf")
        t = -0.025                             # Inter Tight is Inter with tighter spacing
        return [("S", reg, 12, (32, 125), EXTRAS, t, PHASES), ("M", med, 15, (32, 125), EXTRAS, t, PHASES),
                ("L", semi, 28, (32, 32), [ord(c) for c in L_CHARS[1:]], t, PHASES_L)]
    raise SystemExit(f"unknown preset {name}")


def parse_face(s):
    name, _, rest = s.partition("=")
    font, px, rng = rest.rsplit(":", 2)
    extras = []
    if "+" in rng:
        rng, ex = rng.split("+", 1)
        extras = [int(x, 0) for x in ex.split(",") if x]
    a, b = rng.split("-")
    return (name, font, int(px), (int(a), int(b)), extras, None, PHASES)


HC_MAXLEN = 15


# (FASE 4) the values a face gets RUNS of: the fonts' (0, 15), and a caller may name its own -- the keycaps
# spend most of their pixels on the fill (5) and on what is outside the pill (0), so those two runs halve
# their table (tools/gen_aa_keycaps.py)
RUNS = (0, 15)


def huff_symbols(v, runs=None):
    """a glyph's alpha values -> symbols (count - 1) << 4 | value: runs of 1..16 of a RUNS value, singles else"""
    runs = RUNS if runs is None else runs
    out, i = [], 0
    while i < len(v):
        j = i + 1
        if v[i] in runs:
            while j < len(v) and v[j] == v[i] and j - i < 16:
                j += 1
        out.append((j - i - 1) << 4 | v[i])
        i = j
    return out


def huff_lengths(count):
    """symbol -> code length (Huffman), at most HC_MAXLEN"""
    heap = [(c, k, (s,)) for k, (s, c) in enumerate(sorted(count.items()))]
    heapq.heapify(heap)
    length = {s: 0 for s in count}
    if len(heap) == 1:
        return {s: 1 for s in count}
    k = len(heap)
    while len(heap) > 1:
        a, b = heapq.heappop(heap), heapq.heappop(heap)
        for s in a[2] + b[2]:
            length[s] += 1
        k += 1
        heapq.heappush(heap, (a[0] + b[0], k, a[2] + b[2]))
    assert max(length.values()) <= HC_MAXLEN, "code too long: limit the lengths"
    return length


def huff_pack(glyph_values, runs=None):
    """[alpha values per glyph] -> (hc table bytes, [bytes per glyph]): canonical Huffman, MSB first"""
    syms = [huff_symbols(v, runs) for v in glyph_values]
    count = {}
    for g in syms:
        for s in g:
            count[s] = count.get(s, 0) + 1
    length = huff_lengths(count)
    order = sorted(count, key=lambda s: (length[s], s))
    code, c, prev = {}, 0, length[order[0]]
    for s in order:                                    # canonical: by length, then symbol
        c <<= length[s] - prev
        prev = length[s]
        code[s] = c
        c += 1
    hc = [sum(1 for s in order if length[s] == n) for n in range(1, HC_MAXLEN + 1)] + order
    out = []
    for g in syms:
        acc, nb, b = 0, 0, bytearray()
        for s in g:
            acc, nb = (acc << length[s]) | code[s], nb + length[s]
            while nb >= 8:
                nb -= 8
                b.append((acc >> nb) & 255)
        if nb:
            b.append((acc << (8 - nb)) & 255)
        out.append(bytes(b))
    return hc, out


def huff_unpack(hc, data, n):
    """the decoder (gfx.c cv_alpha_hc), for the self-check: n values from the bit stream data"""
    out, pos = [], 0
    while len(out) < n:
        code = first = idx = 0
        for ln in range(HC_MAXLEN):
            code |= (data[pos >> 3] >> (7 - (pos & 7))) & 1
            pos += 1
            if code - first < hc[ln]:
                break
            idx += hc[ln]
            first = (first + hc[ln]) << 1
            code <<= 1
        s = hc[HC_MAXLEN + idx + code - first]
        out += [s & 15] * ((s >> 4) + 1)
    assert len(out) == n
    return out


def emit(face, tracking, gamma, kern_min):
    name, font, px, (first, last), extras, ftrack, phases = face
    deep = DEEP.get(name, "")
    nph = phases * 2 if deep else phases          # the raster's phases (the slot count of a deep glyph)
    chars = [chr(c) for c in range(first, last + 1)] + [chr(c) for c in extras]
    r = ar.raster_font(font, px, chars, tracking if ftrack is None else ftrack, gamma, True, kern_min, nph)
    rise = RISE.get(os.path.basename(font.split("#")[0].split("@")[0]), 0)
    if rise:                                       # the ink up by rise px in its line (the line shorter by as much)
        r["asc"] -= rise
        r["h"] -= rise
        r["glyphs"] = {ch: (a, [(bx, by - rise if bw else by, bw, bh, b) for bx, by, bw, bh, b in ph])
                       for ch, (a, ph) in r["glyphs"].items()}
    data, table, lines, xf, gi = bytearray(), [], [], [], []
    psh0 = phases.bit_length() - 1
    for ch in chars:
        adv16, ph = r["glyphs"][ch]
        if nph == phases:                         # a face of one grid: phases entries a glyph, as it always had
            keep = ph if len(ph) == phases else ph + [ph[0]] * (phases - len(ph))
        else:                                     # the mixed face: every other phase for the rest of it -- 0,
            keep = ph if (ch in deep or len(ph) <= 1) else ph[0::2]   # 1/4, 1/2, 3/4, the bitmaps it always had
        gi.append(len(table))                     # (3.0) the glyph's first entry (AF_<N>_GI)
        for bx, by, bw, bh, b in keep:
            table.append((len(data), adv16, bx, by, bw, bh))
            data += b
        xf.append(psh0 if nph == phases else len(keep).bit_length() - 1)   # (3.0) its phases (AF_<N>_X)
    raw, hc = bytes(data), []
    if name in HUFF[0]:
        # Huffman-code every distinct bitmap (phases may share one), byte-aligned per glyph
        offs = sorted({e[0] for e in table if e[4]})
        vals = []
        for off in offs:
            e = next(t for t in table if t[0] == off and t[4])
            v = []
            for byte in raw[off:off + (e[4] * e[5] + 1) // 2]:
                v += [byte >> 4, byte & 15]
            vals.append(v[:e[4] * e[5]])
        hc, packed = huff_pack(vals)
        data, newoff = bytearray(), {}
        for off, v, b in zip(offs, vals, packed):
            assert huff_unpack(hc, b, len(v)) == v
            newoff[off] = len(data)
            data += b
        table = [(newoff.get(off, 0),) + t[1:] for t in table for off in (t[0],)]
        data += bytes(2)                               # gfx.c cv_alpha_hc reads up to 2 bytes ahead
    assert len(data) < 65536, "glyph data over 64 KiB: split the face"
    kern = sorted(r["kern"].items())
    keys = [(ord(a) << 8) | ord(b) for (a, b), _ in kern if ord(a) < 256 and ord(b) < 256]
    deltas = [d for (a, b), d in kern if ord(a) < 256 and ord(b) < 256]
    order = sorted(range(len(keys)), key=lambda i: keys[i])
    keys, deltas = [keys[i] for i in order], [deltas[i] for i in order]
    psh = nph.bit_length() - 1
    if any(x != psh for x in xf):                     # a face that mixes its phases: the runs and their log2
        lines.append(ar.c_array(f"AF_{name}_GI", "uint16_t", gi, 12))
        # 2 bits a glyph (they are 0..3): 95 B of table become 24
        lines.append(ar.c_array(f"AF_{name}_X", "uint8_t",
                                [sum(xf[i * 4 + j] << (2 * j) for j in range(4) if i * 4 + j < len(xf))
                                 for i in range((len(xf) + 3) // 4)], 24))
        giname, xname = f"AF_{name}_GI", f"AF_{name}_X"
    else:
        giname = xname = "0"
    lines.append(f"/* {name}: {os.path.basename(font.split('#')[0])} {px}px, h {r['h']}, baseline {r['asc']}, "
                 f"{len(chars)} glyphs x {nph} phases, {len(data)} B data"
                 + (f" (Huffman; {len(raw)} B as nibbles) */" if hc else " */"))
    lines.append(ar.c_array(f"AF_{name}_DATA", "uint8_t", list(data), 24, "0x{:02x}"))
    if hc:
        lines.append(ar.c_array(f"AF_{name}_HC", "uint8_t", hc, 24))
    # (FASE 1, 2026-11-16) the advance is the same in every phase (checked here, not assumed): 2 B a glyph
    # instead of 2 B a phase, and aag_t drops to 6 B (gfx.c). 840 B of XIP across the three faces; the glyphs,
    # their bitmaps and the drawn pixels are untouched.
    advs = [table[gi[k]][1] for k in range(len(chars))]
    assert all(len({table[p][1] for p in range(gi[k], gi[k] + (1 << xf[k]))}) == 1 for k in range(len(chars))), \
        f"{name}: a glyph's advance differs between its phases"
    # the advances are 1/16 px: a byte each when the face's widest fits (S 156, M 144: 210 B less XIP)
    advt = "uint8_t" if max(advs) <= 0xFF else "uint16_t"
    lines.append(ar.c_array(f"AF_{name}_ADV", advt, advs, 16))
    # (FASE 2/5, 2026-11-25) one word a glyph and phase instead of six bytes: off, bx + 1 and by / bw / bh five bits
    # each. The faces' ranges are checked here, so a face that outgrows the word fails the build instead of being
    # truncated. 4 B a phase: 1 304 B less XIP across S, M and L; the bitmaps and the drawn pixels do not move.
    # (3.0) off is 14 bits (gfx.c aag_t): the word was 31, so the extra bit was the spare and costs nothing; S at 8
    # phases needed 8 223 B of bitmaps and the 13-bit record refused the whole header.
    lines.append(f"static const aag_t AF_{name}_G[{len(table)}] = {{")
    for off, adv, bx, by, bw, bh in table:
        assert 0 <= off <= 0x3FFF and -1 <= bx <= 5 and 0 <= by <= 31 and 0 <= bw <= 31 and 0 <= bh <= 31, \
            f"{name}: glyph record out of range: {off} {bx} {by} {bw} {bh}"
        w = off | ((bx + 1) & 7) << 14 | by << 17 | bw << 22 | bh << 27
        lines.append(f"    {{0x{w:08x}u}},")
    lines.append("};")
    if keys:
        lines.append(ar.c_array(f"AF_{name}_KERN", "uint16_t", keys, 12))
        lines.append(ar.c_array(f"AF_{name}_KD", "int8_t", deltas, 24))
    ex = ", ".join(str(CODE.get(c, c)) for c in extras) or "0"
    lines.append(f"static const uint8_t AF_{name}_EX[{max(1, len(extras))}] = {{{ex}}};")
    lines.append(f"static const aafont_t AF_{name} = {{{r['h']}, {r['asc']}, {first}, {last}, {len(extras)}, {len(keys)}, "
                 f"AF_{name}_G, AF_{name}_ADV, AF_{name}_DATA, {'AF_%s_KERN' % name if keys else '0'}, "
                 f"{'AF_%s_KD' % name if keys else '0'}, AF_{name}_EX, {psh}, {giname}, {xname}, "
                 f"{1 if advt == 'uint8_t' else 0}, {'AF_%s_HC' % name if hc else '0'}}};");
    if "H" in chars:                                  # the capitals' band (gfx.c CAP_IN: centring text by it)
        cap = table[gi[chars.index("H")]]
        lines.append(f"#define AF_{name}_CAP_Y {cap[3]}   /* the ink of H: rows {cap[3]} .. {cap[3] + cap[5] - 1} of the line */")
        lines.append(f"#define AF_{name}_CAP_H {cap[5]}")
    cost = {"data": len(data) + len(hc), "glyph_table": 6 * len(table) + 2 * len(advs), "kern": 3 * len(keys), "extras": len(extras),
            "glyphs": len(chars), "phases": phases, "pairs": len(keys), "h": r["h"], "asc": r["asc"], "px": px,
            "font": os.path.basename(font.split("#")[0]), "adv_digit": r["glyphs"]["0"][0] / 16}
    cost["total"] = cost["data"] + cost["glyph_table"] + cost["kern"] + cost["extras"]
    return "\n".join(lines) + "\n", cost


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("--preset", choices=["standin", "inter-tight", "chakra-petch", "jiant-lab"])
    ap.add_argument("--face", action="append", default=[])
    ap.add_argument("--tracking", type=float, default=0.0, help="extra advance per glyph in em")
    ap.add_argument("--gamma", type=float, default=1.0, help="coverage curve; <1 thickens light-on-dark text")
    ap.add_argument("--kern-min", type=int, default=1, help="smallest kerning pair kept, in 1/16 px (0 pairs = no kerning)")
    ap.add_argument("--huff", default=",".join(HUFF[0]), help="faces stored Huffman-coded (comma list, '' = none)")
    a = ap.parse_args()
    HUFF[0] = tuple(x for x in a.huff.split(",") if x)
    faces = ([parse_face(s) for s in a.face] if a.face else []) or (preset(a.preset) if a.preset else None)
    if not faces:
        ap.error("--preset or --face is required")
    out = ["/* generated by tools/gen_aa_font.py: 4-bit alpha glyphs (Inter Tight, SIL OFL 1.1, "
           "Copyright 2022 The Inter Project Authors; see assets/fonts/OFL.txt) */",
           "#pragma once", "#include <stdint.h>", ""]
    total = 0
    for f in faces:
        text, c = emit(f, a.tracking, a.gamma, a.kern_min)
        out.append(text)
        total += c["total"]
        print(f"face {f[0]}: {c['font']} {c['px']}px h{c['h']} base{c['asc']}  {c['glyphs']} glyphs x {c['phases']}  "
              f"data {c['data']} B + table {c['glyph_table']} B + kern {c['kern']} B ({c['pairs']} pairs) "
              f"= {c['total']} B   digit adv {c['adv_digit']:.2f}px")
    Path(a.out).write_text("\n".join(out))
    print(f"fonts total {total} B  -> {a.out}")


if __name__ == "__main__":
    main()
