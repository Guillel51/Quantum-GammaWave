# Pasada de recuperación de XIP (v1.9.3, 2026-11-23)

Objetivo: volver a tener holgura en el slot de aplicación y acercarse al suelo de 2 048 B libres. Esto es lo que se
midió, con qué y qué quedó.

## Punto de partida y de llegada

| | antes | después de la pasada |
|---|---|---|
| imagen | 583 336 B | **583 336 B** |
| slot de aplicación | 583 612 B | 583 612 B |
| libre | 276 B | **276 B** |
| suelo | 2 048 B | 2 048 B (**faltan 1 772 B**) |

Clase: build AC791N (`./build.sh --release`), `tools/resource_map.py`. Ninguna medida de la FM-1 física.

**La pasada no recuperó bytes.** No es que no se buscara: lo que sigue es qué se miró y qué costó cada palanca.

## Dónde están los 583 336 B

Del mapa del enlazador (`build/felucca.map`):

| ventana | bytes |
|---|---|
| `.text` (código + tablas constantes en XIP) | 437 336 |
| `.rodata` (tablas de motores, fuentes, presets) | 103 396 |
| `.rodata.str1.1` (cadenas) | 25 376 (el enlazador ya juntó 2 159 B de duplicados: 27 535 antes de relajar) |
| `.rodata.cst4` / `.cst8` | 96 / 184 |
| relleno de alineación | el resto hasta 566 796 (XIP) + 16 540 (RAMTEXT/.data) |

Por módulo (sólo `.text`, símbolos atribuidos por fichero que los define): `ui_graph.c` 46 164 · `gen.c` 28 668 ·
`ui_draw.c` 26 718 · `ui_input.c` 24 626 · `ui_being.c` 24 324 · `gen_reel.c` 20 650 · `project.c` 16 670 ·
`ui_layer.c` 13 612 · `ui_tech.c` 12 554 · `ui.c` 10 664 · `gfx.c` 8 866 · `ui_scl.c` 7 894 · `seq.c` 7 696 ·
`gen_trap.c` 7 560 · `main.c` 7 280 · `song_dice.c` 6 690 · `editor.c` 6 542 · `ui_menu.c` 6 214.

## Código y constantes muertos: no hay

1. **`--gc-sections` ya está haciendo su trabajo**: el mapa lista **52 012 B** de secciones de entrada descartadas.
2. **Barrido de símbolos**: de 818 símbolos de ≥128 B en `.text`, `.rodata` y `.data`, sólo **5** aparecen con ≤1
   referencia en `firmware/src`, `build/gen` y `firmware/hal`: `fm1_cstart`, `fm1_fault`, `fm1_fault_c`,
   `fm1_guard_enable` (entradas de arranque/vector, referenciadas desde `.S`) y `fm1__anim_frame`. **Ninguna tabla
   ni función muerta de ≥128 B.** Las grandes (`AF_*`, `AI*`, `UAC_FIR`, `WT_COEF`, `FM6_*`) están todas en uso.
3. **Caminos fríos**: todo el código no alcanzable por interrupción ya va a `minsize` (`tools/size_fns.py`, 60+
   ficheros). Lo que quedaba en `-Os` era el DSP y los motores.

## Palancas medidas en esta pasada

| palanca | imagen | diferencia |
|---|---|---|
| referencia | 583 336 B | — |
| motores y DSP también a `minsize` (23 ficheros más) | 583 448 B | **+112 B**, peor: `minsize` inhibe inlining. Revertido |
| `-fmerge-all-constants` + `-fno-unwind-tables` + `-fno-asynchronous-unwind-tables` | 583 336 B | 0 (ya es el comportamiento de `-Oz`) |
| `-falign-functions=1 -falign-jumps=1 -falign-loops=1` | 583 336 B | 0 (ya alineado a 1) |
| `--sort-section=alignment` | 583 336 B | 0 |
| `-fdata-sections` (con `--gc-sections` ya activo) | 583 328 B | **−8 B** y `PITCH_OCT` sigue dentro: no hay datos que recoger; no compensa tocar el build |

Ya descartadas y documentadas antes, con su número: quitar el kerning (bloqueado por `tests/text_spacing_test.py`,
0,239 de 0,25 px), dibujantes en `.ram_text` (0 B: `AT > XIP` deja la copia en el mismo sitio), `FELUCCA_SIZE=0`
(582 748 B, unos 590 B menos, pero todo el DSP a `-Oz`).

## El slot no puede crecer

`APP_SLOT = 0x8E7BC` ya incluye los 2 048 B libres que había dentro de la imagen (comentario en
`tools/fm1pkg_make.py`: `0x8DFBC + 2048`). La región de código termina en `0x8ED2B` y el límite duro de la flash en
`0x8F000`: quedan **725 B**, y mover `APP_SLOT` cambia el mapa de la flash y el paquete, cosa que no se hace sin
evidencia de la unidad.

## Qué sí podría liberar bytes (y no es «recuperación»)

Son decisiones de producto o de formato, no basura que quitar:

1. **Guardar decodificado lo que hoy se guarda entero**, como ya se hace con el seno y el `exp2` (2 046 B de
   diferencias en `.text` → 4 100 B en `.bss`): sólo los presets de
   fábrica (`ANALOG_PRESETS` 2 628, `PHASE_PRESETS` 1 224, `LOFI_PRESETS` 1 044, `FM6_PRESETS` 1 332,
   `DRUM_MAP` 1 408) son **7 636 B**, y las tablas de audio (`UAC_FIR` 7 680, `AF_*` 19 819, `AI12/AI16` 6 033) otros
   33 532 B. Un packer delta/RLE de los presets, decodificado al cargar, podría devolver del orden
   de 3–4 KB, a cambio de RAM para el búfer de decodificación (libre: 4 348 B, suelo 4 096) y de un decodificador
   que probar.
2. **Recortar una familia**: los generadores (`gen.c` + `gen_reel.c` + `gen_trap.c` + `song_dice.c`) son 63 568 B
   de código; los visualizadores (`ui_being.c`) 24 324 B; las páginas TECH (`ui_tech.c`) 12 554 B; el OTA (`ota.c` +
   `ota_hw.c`) 3 604 B.
3. **Aceptar el suelo**: 276 B libres es un slot lleno, no un enlace roto; el objetivo de 2 048 B es una política del
   repositorio, y hoy no se cumple con ningún cambio que no cueste una función o un formato.

Nada de esto se aplicó: la pasada fue de medida, y el árbol queda exactamente como estaba en v1.9.3 (imagen
583 336 B, 276 B libres).
