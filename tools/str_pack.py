#!/usr/bin/env python3
"""What the UI strings cost, and what a dictionary could really give back.

The XIP audit says `.rodata.str1.1` is 25 562 B with 4 293 references: the largest category of constants after
the fonts and the curves. This tool measures, on the linked image (class TARGET AC791N, the linker's own bytes),
what three different schemes would actually save -- and what each costs in RAM, which is the budget that is
really short (RAMTEXT keeps 4 128 B free).

Why the numbers matter more than the idea
-----------------------------------------
GNU ld already merges string literals in a SHF_MERGE|SHF_STRINGS section, **including suffixes**: when both
"CHORD" and "MORPH CHORD" are literals, the section keeps one copy of the tail and the second literal is placed
to overlap it. So "suffix sharing" is not a saving waiting to be taken -- it is already taken -- and a tool that
claims it would be measuring a redundancy the linker removed. This tool therefore measures what is *left*:

  tail    suffix pairs still present in the linked section (bytes the linker did not fold, if any);
  prefix  the bytes a radix tree over the strings would keep, against the flat store (the upper bound of prefix
          compression);
  front   a prefix dictionary of the K best prefixes (front coding: each string = id + tail).

and, for each, the RAM it demands at run time: a `const char *` is a 4-byte link-time constant inside `.text`
(flash, read-only), so a string a boot-time expansion builds cannot be reached by those words at all. Every
string rebuilt at run time needs a pointer slot in RAM (4 B) or a call-site change to pass an index instead of
an address -- and the strings themselves must live somewhere too.

Usage: python3 tools/str_pack.py [--md docs/STR_PACK.md] [--top 40]
Exit status 0 always: this measures, it does not gate.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(ROOT, "build", "felucca.elf")

SHF_MERGE, SHF_STRINGS = 0x10, 0x20
SHT_NOBITS, SHT_PROGBITS = 8, 1


def sections(img):
    """[(name, addr, offset, size, flags, type)] from the section header table, by its own bytes."""
    e_shoff = struct.unpack_from("<I", img, 0x20)[0]
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", img, 0x2E)
    raw = []
    for i in range(e_shnum):
        o = e_shoff + i * e_shentsize
        name, typ, flags, addr, off, size = struct.unpack_from("<IIIIII", img, o)
        raw.append((name, typ, flags, addr, off, size))
    strtab = raw[e_shstrndx]
    out = []
    for name, typ, flags, addr, off, size in raw:
        end = img.index(b"\0", strtab[4] + name)
        out.append((img[strtab[4] + name:end].decode("ascii", "replace"), addr, off, size, flags, typ))
    return out


def addr_to_file(img, addr):
    """The file offset of a virtual address, through the program headers (the image is one load segment)."""
    e_phoff = struct.unpack_from("<I", img, 0x1C)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", img, 0x2A)
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from("<IIIIII", img, o)
        if p_type == 1 and p_vaddr <= addr < p_vaddr + p_filesz:
            return p_offset + (addr - p_vaddr)
    raise SystemExit("str_pack: address 0x%x is not in a loadable segment" % addr)


def map_section(path, want):
    """(address, size) of one input section, from the map -- the image itself keeps only the merged .text."""
    import re
    txt = open(path, encoding="utf-8", errors="replace").read()
    pat = re.compile(r"^\s*(" + re.escape(want) + r")\s*\n?\s*0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)", re.M)
    m = pat.search(txt)
    if not m:
        raise SystemExit("str_pack: %s is not in the map" % want)
    return int(m.group(2), 16), int(m.group(3), 16)


def strings_of(img, sec):
    """The literals of a string-merge section, in place: NUL-separated, as the linker laid them out."""
    name, addr, off, size, flags, typ = sec
    blob = img[off:off + size]
    out, cur = [], bytearray()
    for b in blob:
        if b == 0:
            if cur:
                out.append(bytes(cur))
            cur = bytearray()
        else:
            cur.append(b)
    if cur:
        out.append(bytes(cur))
    return blob, out


def tail_savings(strs):
    """Bytes the section would lose if every string that ends another were folded into it (ld does this already)."""
    have = set(strs)
    saved, pairs = 0, []
    for s in strs:
        # the longest proper suffix of s that is itself a stored literal
        for k in range(1, len(s)):
            if s[k:] in have:
                saved += k + 1          # the shared tail plus its NUL
                pairs.append((s, s[k:], k + 1))
                break
    return saved, pairs


def radix_bytes(strs):
    """Bytes a radix tree keeps for these strings (one node per branch char, NUL-only leaves counted once)."""
    trie = {}
    for s in strs:
        node = trie
        for ch in s:
            node = node.setdefault(ch, {})
    n = 0

    def walk(node):
        nonlocal n
        for k, child in node.items():
            n += 1
            walk(child)
        if not node:
            n += 1                      # the terminator of a word
    walk(trie)
    return n


def refs_where(img, lo, hi):
    """(references inside the XIP flash, references in RAM) -- a pointer word in flash cannot be repointed."""
    e_phoff = struct.unpack_from("<I", img, 0x1C)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", img, 0x2A)
    segs = []
    for i in range(e_phnum):
        o = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz = struct.unpack_from("<IIIIII", img, o)
        if p_type == 1 and p_filesz:
            segs.append((p_offset, p_offset + p_filesz, p_vaddr - p_offset))
    words = struct.unpack_from("<%dI" % (len(img) // 4), img)
    in_flash = in_ram = 0
    for i, w in enumerate(words):
        if lo <= w < hi:
            f = i * 4
            va = None
            for a, b, delta in segs:
                if a <= f < b:
                    va = f + delta
            if va is not None and 0x02000000 <= va < 0x03000000:
                in_flash += 1
            else:
                in_ram += 1
    return in_flash, in_ram


def main():
    if not os.path.exists(ELF):
        print("str_pack: build/felucca.elf is needed (./build.sh)", file=sys.stderr)
        return 1
    img = open(ELF, "rb").read()
    secs = sections(img)
    maddr, msize = map_section(os.path.join(ROOT, "build", "felucca.map"), ".rodata.str1.1")
    sec = (".rodata.str1.1", maddr, addr_to_file(img, maddr), msize, SHF_MERGE | SHF_STRINGS, SHT_PROGBITS)
    name, addr, off, size, flags, typ = sec
    blob, strs = strings_of(img, sec)
    total = sum(len(s) + 1 for s in strs)
    merge = "merge+strings" if (flags & (SHF_MERGE | SHF_STRINGS)) == (SHF_MERGE | SHF_STRINGS) else hex(flags)

    print("str_pack: .rodata.str1.1 %d B, %d literals, %d B of text (%.1f B each), flags %s"
          % (size, len(strs), total, total / max(1, len(strs)), merge))
    dupes = len(strs) - len(set(strs))
    print("str_pack: %d duplicate literals still in the section (ld merges identical ones: %s)"
          % (dupes, "yes, none left" if not dupes else "no -- look here first"))

    saved, pairs = tail_savings(strs)
    print("str_pack: tail  %6d B  (%d strings end another one)" % (saved, len(pairs)))
    if pairs:
        for a, b, n in pairs[:8]:
            print("           %-32s ends %-24s %d B" % ("'" + a.decode('latin1') + "'", "'" + b.decode('latin1') + "'", n))

    rad = radix_bytes(strs)
    print("str_pack: prefix %6d B  a radix tree keeps %d B (string bytes only, no indexes)"
          % (total - rad, rad))

    # The one variant that needs no RAM: every literal becomes an index into the radix blob and the draw code
    # expands the (prefix, tail) pair into a small buffer as it goes. It costs 2 B of index per literal -- in the
    # image -- and an edit at every place a literal is used.
    idx = 2 * len(strs)
    print("str_pack: accessor  %6d B  = %d B of text + %d B of 2-byte indexes (no RAM at run time)"
          % (total - rad - idx, rad, idx))
    flash, ram = refs_where(img, addr, addr + size)
    print("str_pack: refs   %d words point into the section: %d of them live in the XIP flash, %d in RAM"
          % (flash + ram, flash, ram))

    # What each scheme costs in RAM, which is the budget that is short.
    ptrs = 4 * len(strs)
    print("str_pack: RAM   a rebuilt string needs a 4 B pointer slot (or an index at every call site): %d B for %d"
          % (ptrs, len(strs)))
    print("str_pack: RAM   a full boot expansion holds the text too: %d B, against 4 128 B free in RAMTEXT" % total)

    md = sys.argv[sys.argv.index("--md") + 1] if "--md" in sys.argv else None
    if md:
        with open(os.path.join(ROOT, md), "w", encoding="utf-8") as f:
            f.write("# The UI strings: what a dictionary could give (AC791N build)\n\n")
            f.write("Generated by `tools/str_pack.py` from `build/felucca.elf`: class **TARGET**, the linker's own\n"
                    "bytes, not an estimate.\n\n")
            f.write("| | bytes |\n|---|---|\n")
            f.write("| `.rodata.str1.1` | %d |\n| text of the literals | %d |\n| literals | %d |\n"
                    "| duplicate literals left | %d |\n" % (size, total, len(strs), dupes))
            f.write("| tail sharing still available | %d |\n| prefix (radix tree) | %d |\n"
                    "| accessor variant (radix + 2 B index per literal) | %d |\n"
                    % (saved, total - rad, total - rad - idx))
            f.write("\nThe strings are addressed by 4-byte link-time constants inside `.text`, which is the XIP flash:\n"
                    "a string built at boot cannot be reached by those words. Each rebuilt string therefore costs a\n"
                    "4-byte pointer slot in RAM (%d B for these %d literals) or an index passed at every call site --\n"
                    "and the expanded text itself needs somewhere to live (%d B against the 4 128 B free in RAMTEXT).\n"
                    % (ptrs, len(strs), total))
        print("str_pack: wrote %s" % md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
