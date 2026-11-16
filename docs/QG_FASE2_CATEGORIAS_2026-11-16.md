# FASE 2 — categorías de presets en el firmware, y el espacio XIP que dejan

Fecha: 2026-11-16. Continúa `docs/QG_FASE1_OPTIMIZACION_2026-11-16.md` (`3f6cb02`).

## Qué se pedía

- Liberar definitivamente el espacio XIP necesario para las funciones nuevas, con margen real, no solo que enlace.
- Implementar las categorías de presets **en el firmware**, en SELECT y PRESETS, con la clasificación y el
  generador que ya existían.
- Comprimir la tabla, sin información redundante, con los índices y desplazamientos de cada motor correctos,
  WTABLE y las excepciones de DRUM incluidas.
- Conservar el orden, los identificadores, los parámetros y los sonidos originales, y sincronizar firmware,
  Web Studio y documentación.

## La tabla (generada, no escrita a mano)

`tools/gen_preset_categories.py` ya clasificaba los sonidos de fábrica a partir de las propias tablas de los
motores (`eng_*.c`) y escribía `docs/PRESET_CATEGORIES.md` y `web/preset_categories.json`. Ahora emite además
`build/gen/preset_cats.h`, que entra en la imagen (lo genera `tools/build.py generate()`, como el resto de
cabeceras):

| Dato | Tamaño | Qué es |
|------|--------|--------|
| `PCAT_NAME[12][9]` | 108 B | `ALL` y las 11 categorías, en un array fijo (sin punteros, sin nombres repetidos) |
| `PCAT_BASE[NENGINES]` | 30 B | el primer nibble de cada motor; `PCAT_GONE` (0xFFFF) si no tiene sonidos aquí |
| `PCAT_NIB` | 138 B | cuatro bits por sonido, dos sonidos por byte, para las 275 entradas de las nueve tablas |

`pcat_of(e, k)` indexa por **número de motor** (`ENGINES[]`: ANALOG 0 … DRUM 10, NOISE 11, FM6 12,
WTABLE 14) y por el **índice de tabla** del sonido —el mismo que guardan los proyectos, los presets de usuario y
el protocolo del editor—, así que los dos kits legacy de DRUM en los ids 10/11 no desplazan a nadie y ninguna
posición "mostrada" interviene. Los motores retirados y reservados (1, 4, 6, 8, 9, 13) quedan en `PCAT_GONE`.
Ningún nombre se almacena dos veces: el nombre vive en su tabla de motor, la categoría son cuatro bits.

## La página PRESETS (SELECT y PRESETS)

- **SELECT** en SAVE > PRESETS recorre las categorías y lo dice (`CATEGORY BASS`) en el mensaje del footer; en
  cualquier otra página SELECT sigue pasando las páginas de la sección, como antes (probado).
- **PRESETS** navega dentro de la categoría elegida y carga el sonido, conservando pasos, ARP, SCL y SLICER.
- El filtro se **compone** con el de favoritos de KNOB 3/KNOB 4: `BASS` + `FAV` son los favoritos de BASS, y la
  lista vacía dice cuál de los dos filtros la ha vaciado (`NO SOUNDS / IN THIS CATEGORY`,
  `NO FAVORITES / IN THIS CATEGORY`).
- La tarjeta **LIST** (KNOB 4) dice las dos cosas: la categoría como valor (`ALL` cuando no hay ninguna) y
  `FAV` como unidad mientras el filtro de favoritos está puesto. KNOB 4 sigue conmutando ese filtro.
- Un **preset de usuario** no tiene categoría de fábrica (las reglas leen un nombre de fábrica): sale en `ALL`.
- La categoría es estado de sesión, como el filtro de favoritos: no viaja en el proyecto ni en los ajustes.

## DRUM: los dos kits legacy (la inconsistencia que había)

`engine_library()` recortaba los alias legacy **solo al final** de la tabla. DRUM guarda los suyos en los ids
10 y 11, en medio, así que el navegador del firmware listaba **18** sonidos de DRUM mientras el editor, los
documentos y `web/preset_categories.json` decían **16** — y peor: los índices del JSON para los kits 12..17 no
correspondían a los sonidos que el firmware cargaba en esa posición.

Ahora:

- `engine_t.legacy_n` cuenta los alias de cada motor (`engine_library(e) = npresets - legacy_n`, un compare en
  vez de un bucle) y `eng_preset_nth` / `eng_preset_rank` saltan los legacy **en cualquier posición**. DRUM
  muestra sus 16 kits y la lista de 273 sonidos es la misma en firmware, editor y documentos.
- **Ningún id se mueve**: un proyecto que guardó `HEARTH KIT` (10) o `CYBER DRUM` (11) sigue cargando ese sonido;
  lo único que ya no hace es aparecer en la lista, que es lo que decía la auditoría de 1.6.
- Los consumidores de la lista pasan por `eng_preset_nth`: el dado de sonido (KNOB y DICE ALL), la aleatoriedad
  de las canciones (`song_dice.c`), `ED_NAMES` / `ED_PRESET` del Web Studio y el volcado de descriptores. Así el
  índice que el editor manda es el que el editor enseñó.
- La posición que se enseña (`OVERVIEW`, `ENGINE`) es la de la lista, no la de la tabla: `preset_rank`/`preset_shown`.

## Espacio XIP: lo que costó y lo que se liberó

Todo medido con el toolchain y el SDK fijados en la fase 1 (única fuente de estos números;
`docs/MEMORY_BUDGET.md` §7 tiene las tablas).

| Paso | Imagen | XIP code | Constantes |
|------|--------|----------|------------|
| FASE 1 | 582 388 B | 418 768 B | 143 140 B |
| + categorías | 583 292 B | 419 332 B | 143 480 B |
| + `legacy_n` (un bucle menos) y tres celdas de icono de 12 px que nadie dibuja | 583 164 B | 419 352 B | 143 332 B |
| (las cajas de glifo de la cara S en cuatro bits por campo, 688 B: probadas y retiradas, ver abajo) | — | — | — |

- **Margen XIP final: 416 B** (slot `APP_SLOT` = 0x8E7BC = 583 612 B). La función entera cuesta **808 B**: es el
  margen más fino del proyecto y **no** cumple el objetivo de dejar un kilobyte largo.
- `aag4_t`: donde la caja de tinta de un glifo cabe en cuatro bits por campo (bx −8..7, bw, by, bh 0..15 — la
  cara S: 105 glifos × 4 fases), la tabla guarda 4 B por fase en vez de 6 (−840 B de constantes, +128 B de
  código de decodificación). Las caras M (ascent 16) y L (bw 29) siguen con `aag_t`. **Los píxeles dibujados son
  los mismos**: `tests/text_ref_test.c` reimplementa el camino de texto y compara 6 732 cadenas × caras ×
  paletas, 0 píxeles distintos; `tests/text_spacing_test.py` sigue en 0,425 px de error máximo en L.
- Las tres celdas de icono de 12 px que se fueron (`x_matrix`, `x_gauge`, `x_dyn`) no las dibuja ninguna pantalla;
  los nombres siguen en el enum.
- Nada de esto toca RAM: `.data+.bss` 90 676 B (7 628 B libres), pool 9 248 B de repuesto, RAM code 19 684 B.

## Pruebas ejecutadas

- `tests/preset_categories_test.py`: 273 sonidos, una categoría cada uno, sin nombres repetidos, el JSON igual a
  la tabla **y** la cabecera generada igual a la tabla (por motor e índice de tabla, 275 entradas, 138 B de nibbles exactos). Falla si el generador y la imagen se separan.
- `tests/preset_library_test.py`: 273 sonidos de fábrica, los 10 ids de DRUM intactos y los dos legacy en 10/11.
- `tests/ui_test.c`, bloque nuevo de categorías (9 comprobaciones): ALL abre los 273; cada sonido navegable
  tiene categoría (WTABLE 22 y DRUM 16 incluidos); DRUM lista 16 y `nth(10)` es 12; SELECT pasa a BASS y lo dice;
  la lista contiene exactamente esa categoría; PRESETS carga dentro de ella sin tocar los pasos; se compone con
  favoritos; vuelve a ALL; un preset de usuario no aparece en ninguna categoría; en otra página SELECT sigue
  pasando páginas. La suite de UI completa pasa (899 comprobaciones).
- `tests/text_ref_test.c` (0 pens, 0 píxeles), `theme_test`, `text_spacing_test.py`: la cara S empaquetada dibuja
  lo mismo que antes.
- `NO_PACKAGE=1 tests/run_tests.sh` completo, `tests/memory_map_test.py` y `tests/target_budget.py`.

## Límites

- No hay unidad FM-1 en este entorno: nada de esto es una prueba física, y el WASM del Web Studio no se
  reconstruyó (no hay `emcc`). El simulador del editor es JavaScript y no compila el firmware.
- El paquete (`felucca.fwsc`) sigue rellenándose hasta el slot, así que su tamaño no cambia: lo que cambia es lo
  que queda libre dentro del slot.
