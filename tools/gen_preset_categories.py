#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Primary musical category for every factory sound, from the firmware tables.

Names and order come from eng_*.c. The category is a documented rule over the engine and the name's
keywords (BASS, LEAD, PAD, ...); the engine default is used only when no keyword is present. This does not
change names, ids, parameters or order, and it is the single source for docs/PRESET_CATEGORIES.md,
web/preset_categories.json and the firmware's own table (build/gen/preset_cats.h).

  gen_preset_categories.py [build/gen/preset_cats.h]

With a path the C header is written too: the categories of every factory sound, two nibbles a byte, indexed
by the engine and by the sound's own index in its engine's table (the same index the projects and the editor
store), plus each engine's first nibble and the category names. The firmware's browser filters with it
(SELECT on the PRESETS page); nothing is stored twice: a name lives in the firmware's preset table, its
category is four bits here.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# (engine, file, array, default category, engine number in firmware/src/engines.c ENGINES[]). The numbers are
# append-only: the stores and the protocol hold them.
ENGINES = [
    ("ANALOG", "eng_analog.c", "ANALOG_PRESETS", "KEYS", 0),
    ("PHASE", "eng_phase.c", "PHASE_PRESETS", "TEXTURES", 2),
    ("LOFI", "eng_lofi.c", "LOFI_PRESETS", "TEXTURES", 3),
    ("FORMANT", "eng_formant.c", "FORMANT_PRESETS", "PADS", 5),
    ("WHEEL", "eng_wheel.c", "WHEEL_PRESETS", "ORGANS", 7),
    ("DRUM", "eng_drum.c", "DRUM_PRESETS", "DRUMS", 10),
    ("NOISE", "eng_noise.c", "NOISE_PRESETS", "TEXTURES", 11),
    ("FM6", "eng_fm6.c", "FM6_PRESETS", "KEYS", 12),
    ("WTABLE", "eng_wavetable.c", "WT_PRESETS", "KEYS", 14),   # (FASE 5) was missing: 20 factory sounds
                                                                # shipped and the browser never listed them
    ("ADDTV", "eng_additive.c", "AD_PRESETS", "KEYS", 15),      # (1.2.6) the eighth partial additive engine
]
NENGINES = 16   # core.h: the table's engines 0..15, reserved numbers included (they have no sounds)
RULES = [
    ("DRUMS", ("DRUM", "KIT", "909", "808", "LINN", "TRAP", "BEAT", "PERC")),
    ("BELLS", ("BELL", "CHIME", "GONG", "CELEST", "MALLET")),
    ("BASS", ("BASS", "SUB", "REESE", "303", "GROWL")),
    ("LEADS", ("LEAD", "SOLO", "SAW", "SQUELCH", "ACID")),
    ("PADS", ("PAD", "DRONE", "CHOIR", "STRING", "ORCH", "ATMOS", "CLOUD", "AIR")),
    ("PLUCKS", ("PLUCK", "HARP", "KALIMBA", "GUITAR", "KOTO", "SITAR")),
    ("KEYS", ("KEY", "EP", "PIANO", "CLAV", "RHODE", "WURLI")),
    ("ORGANS", ("ORGAN", "REED", "HARMON", "ACCORD")),
    ("BRASS", ("BRASS", "HORN", "TRUMP", "SAX")),
    ("FX", ("FX", "NOISE", "GLITCH", "WIND", "METAL", "RADIO", "TAPE")),
]


# (FASE 1/10) THE TABLES HOLD OFFSETS NOW, NOT POINTERS. Each preset_t starts with the 16-bit offset of its name in
# PRESET_NAMES (params.c) instead of a const char *: 306 pointers of 4 B were 1 224 B of XIP for 291 distinct
# strings. The names did not change and neither did their order, so this tool reads the blob once and resolves the
# offset exactly as the firmware's PR_NAME() does -- the categories below are the same table as before the change.
_NAMES = None


def preset_names():
    global _NAMES
    if _NAMES is None:
        s = open(os.path.join(ROOT, "firmware/src/params.c"), encoding="utf-8").read()
        m = re.search(r"const char PRESET_NAMES\[\] =\n(.*?);\n", s, re.S)
        if not m:
            raise SystemExit("no PRESET_NAMES in firmware/src/params.c")
        _NAMES = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))).replace("\\0", "\x00")
    return _NAMES


def preset_name(off):
    b = preset_names()
    return b[off:b.index("\x00", off)]


def entries(path, array):
    """the preset table as the firmware has it: (name, legacy) for every entry, in table order"""
    s = open(path, encoding="utf-8").read()
    m = re.search(r"static const preset_t " + array + r"\[\] = \{", s)
    if not m:
        raise SystemExit("no %s in %s" % (array, path))
    body = s[m.end():]
    out, depth, quote, esc, start = [], 0, False, False, None
    for j, c in enumerate(body):
        if quote:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                quote = False
            continue
        if c == '"':
            quote = True
        elif c == "{":
            depth += 1
            if depth == 1:
                start = j
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                entry = body[start:j + 1]
                nm = re.match(r'\{\s*"([^"]+)"\s*,', entry)
                om = None if nm else re.match(r'\{\s*(\d+)\s*,', entry)
                if nm:
                    out.append((nm.group(1), bool(re.search(r"\.legacy\s*=\s*1", entry))))
                elif om:
                    out.append((preset_name(int(om.group(1))), bool(re.search(r"\.legacy\s*=\s*1", entry))))
                start = None
            if depth < 0:
                break
    return out


def names(path, array):
    return [n for n, _ in entries(path, array)]


def classify(engine, name, default):
    up = name.upper()
    for cat, words in RULES:
        if any(w in up for w in words):
            return cat
    return default


def table_all():
    """every entry of every table with its own index: what the firmware's browser can resolve by id"""
    rows = []
    for eng, f, arr, default, num in ENGINES:
        for i, (n, leg) in enumerate(entries(os.path.join(ROOT, "firmware/src", f), arr)):
            rows.append((eng, num, i, n, classify(eng, n, default), leg))
    return rows


def table():
    """the browsable library: the legacy aliases left out (core.h engine_library skips them anywhere, not just
    at the end: DRUM's two pre-1.6 kits sit at ids 10/11), indexed by the sound's place in that list"""
    rows, seen = [], {}
    for eng, _num, _i, n, c, leg in table_all():
        if leg:
            continue
        rows.append((eng, seen.get(eng, 0), n, c))
        seen[eng] = seen.get(eng, 0) + 1
    return rows


def header(path):
    """the firmware's table: PCAT_BASE[NENGINES] (first nibble of the engine's sounds, PCAT_GONE: none) and
    PCAT_NIB, two four-bit categories a byte, in engine order then table order"""
    all_rows = table_all()
    cats = sorted({c for *_, c, _l in all_rows})          # (the editor's picker lists them sorted too)
    cid = {c: i + 1 for i, c in enumerate(cats)}          # (0 is ALL)
    nib, base = [], [0xFFFF] * NENGINES
    for num in range(NENGINES):
        rows = [r for r in all_rows if r[1] == num]
        if not rows:
            continue
        base[num] = len(nib)
        nib += [cid[r[4]] for r in rows]
    packed = [nib[i] | (nib[i + 1] << 4 if i + 1 < len(nib) else 0) for i in range(0, len(nib), 2)]
    lines = [
        "/* SPDX-License-Identifier: GPL-3.0-only",
        " * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments",
        " * Generated by tools/gen_preset_categories.py from firmware/src/eng_*.c: do not edit.",
        " * The primary category of every factory sound, four bits each, indexed by the engine (core.h NENGINES)",
        " * and by the sound's own index in that engine's preset table (the index a project stores). PCAT_BASE is",
        " * the engine's first nibble, PCAT_GONE for an engine with no sounds here; PCAT_NAME[0] is ALL.",
        " */",
        "#pragma once",
        "#include <stdint.h>",
        "#define PCAT_GONE 0xFFFFu",
        "#define PCAT_N %du" % (len(cats) + 1),
        "static const char PCAT_NAME[PCAT_N][%d] = {\"ALL\", " % (max(len(c) for c in cats + ["ALL"]) + 1) +
        ", ".join('"%s"' % c for c in cats) + "};",
        "static const uint16_t PCAT_BASE[NENGINES] = {%s};" % ", ".join(
            "PCAT_GONE" if b == 0xFFFF else "%du" % b for b in base),
        "static const uint8_t PCAT_NIB[%du] = {%s};" % (len(packed), ", ".join("%du" % b for b in packed)),
        "/* engine e's sound k: its category (0..PCAT_N-1), ALL for an engine that has none here */",
        "static inline uint32_t pcat_of(uint32_t e, uint32_t k)",
        "{",
        "    uint32_t b = e < NENGINES ? PCAT_BASE[e] : PCAT_GONE;",
        "    if (b == PCAT_GONE)",
        "        return 0u;",
        "    b += k;",
        "    return (PCAT_NIB[b >> 1] >> ((b & 1u) * 4u)) & 15u;",
        "}",
        "",
    ]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    return len(cats) + 1, len(packed), sum(1 for b in base if b != 0xFFFF)


def main():
    rows = table()
    os.makedirs(os.path.join(ROOT, "docs"), exist_ok=True)
    with open(os.path.join(ROOT, "docs/PRESET_CATEGORIES.md"), "w", encoding="utf-8") as f:
        f.write("# Factory preset categories (1.7)\n\n")
        f.write("Generated from `firmware/src/eng_*.c` by `tools/gen_preset_categories.py`. Names, ids and\n")
        f.write("order are untouched. Primary category rules: engine default plus the name keywords below.\n\n")
        f.write("| # | Engine | Preset | Category |\n|---:|---|---|---|\n")
        for k, (eng, i, n, c) in enumerate(rows, 1):
            f.write("| %d | %s | %s | %s |\n" % (k, eng, n, c))
        f.write("\nRules: " + ", ".join("%s=%s" % (c, "/".join(w)) for c, w in RULES) + ".\n")
        f.write("""
## En el FM-1 (SELECT y PRESETS)

La página **SAVE > PRESETS** lleva la misma tabla, comprimida: `build/gen/preset_cats.h` guarda cuatro bits por
sonido (`PCAT_NIB`, 138 B para los 275 sonidos de las tablas), el primer nibble de cada motor
(`PCAT_BASE[15]`, `PCAT_GONE` donde no hay sonidos) y los nombres en un array fijo de 7 bytes (`PCAT_NAME[12]`).
Ningún nombre se repite: la categoría se busca por **motor e índice de tabla** —el índice que guardan los
proyectos y el editor—, así que los dos kits legacy de DRUM en los ids 10/11 no desplazan a los demás, y cada
motor entra por su número de `ENGINES[]` (WTABLE es el 14), no por su posición en la lista que se muestra.

- **SELECT** en la página PRESETS recorre las categorías (`ALL`, `BASS`, `BELLS`, … `TEXTURES`) y lo dice en un
  mensaje; en cualquier otra página SELECT sigue pasando las páginas de la sección.
- **PRESETS** navega dentro de la categoría elegida y se compone con el filtro de favoritos de KNOB 3
  (`BASS` + `FAV` = los favoritos de BASS).
- La tarjeta **LIST** (KNOB 4) dice las dos cosas: la categoría en el valor y `FAV` cuando el filtro de
  favoritos está puesto. KNOB 4 sigue conmutando ese filtro.
- Los presets de usuario no tienen categoría (no nacen de una regla sobre su nombre): salen en `ALL`.
- La categoría es estado de sesión, como el filtro de favoritos: no se guarda en el proyecto.
- La biblioteca son 286 sonidos: `engine_library()` salta los alias legacy **en cualquier posición** (no solo
  al final), así que DRUM muestra 16 y no los 18 de su tabla, como el editor y este documento. Los ids no se
  mueven: un proyecto que guardó `HEARTH KIT` (id 10) sigue cargando ese sonido.

## En el Web Studio (PRESETS del dispositivo)

El panel **DEVICE PRESETS** de `web/editor.html` lee `web/preset_categories.json` y ofrece las tres cosas: la
lista (`All presets` / `Favorites only`, que es el filtro del propio FM-1), la **búsqueda** por nombre y la
**navegación por categoría**.

- **La categoría va por motor e índice, no por nombre** (`presetCategoryOf`). Ocho nombres de fábrica se
  repiten entre motores —`SINE KEY`, `SUB BASS`, `PLUCK`, `BRASS`, `WIND`, `ORGAN`, `BELL`, `GLASS PAD`— y un
  mapa por nombre solo podía guardar una de las dos filas. Hoy las dos copias comparten categoría, así que el
  fallo no se veía; queda atado a `motor:índice` para que no aparezca al añadir un preset que repita nombre con
  otra categoría.
- **Se busca lo que la lista enseña**: un preset de usuario responde a su etiqueta `U07` además de a su nombre.
  La búsqueda y la categoría se componen entre sí y con el filtro de favoritos (`LEADS` + `saw` → `SAW LEAD`,
  `HYPER SAW`).
- **La lista vacía lo dice por lo que está puesto**: «No sounds match the search and category.» cuando hay
  búsqueda o categoría, y el texto de favoritos solo cuando el filtro es `Favorites only`.
- **Los textos están traducidos** (`deviceSearch`, `deviceSearchPh`, `deviceCategory`, `deviceCatAll`,
  `deviceNoMatch`, en japonés y en inglés) y la lista de categorías se reconstruye al cambiar de idioma sin
  perder la que esté elegida.
- La **estrella** de cada fila es el favorito del dispositivo (`FAV_SET`), visible en la lista y en el filtro.

Verificado en el navegador (Chromium, simulador QG-1 del propio Web Studio, `http://127.0.0.1:3000/editor.html`
servido desde `web/`): 279 filas con `All presets`, 12 opciones de categoría (`ALL` + 11), `BASS` 18 filas,
`BASS` + `sub` 4 filas (incluye las dos `SUB BASS`, ANALOG y WTABLE), `LEADS` + `saw` 2 filas, estrella que pasa
de `☆` a `★` con `aria-pressed` y el filtro `Favorites only` mostrando esa única fila, mensaje correcto con una
búsqueda sin resultados. Esto es navegador con el simulador: no es el FM-1 físico.

**En el propio FM-1** la página PRESETS tiene las mismas categorías (SELECT las recorre; ver arriba), además de
la lista, la estrella en cada fila y el filtro `ALL`/`FAV` en KNOB 4. Editor y firmware leen la misma tabla, que
genera `tools/gen_preset_categories.py`.
""")
    with open(os.path.join(ROOT, "web/preset_categories.json"), "w", encoding="utf-8") as f:
        json.dump([{"engine": e, "index": i, "name": n, "category": c} for e, i, n, c in rows],
                  f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("preset categories: %d" % len(rows))
    if len(sys.argv) > 1:
        n, nb, ne = header(sys.argv[1])
        print("  firmware table: %d categories, %d B of nibbles, %d engines -> %s" % (n, nb, ne, sys.argv[1]))


if __name__ == "__main__":
    main()
