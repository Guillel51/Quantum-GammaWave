# Fase 4 — Optimización: la medición, y por qué la expansión está bloqueada

Este documento registra **lo que se midió en este árbol** (2026-11-19, commit `3d0c164`) y lo que se decidió
con esos números. Clase de cada cifra: `[AC791N]` es el binario compilado con el toolchain `pi32v2` y el SDK
fijados; `[HOST]` es x86-64 con `cc`; nunca hay una medición de unidad.

## 1. Estado del binario (la restricción real)

| Región | Origen | Tamaño | Usado | Libre | Uso |
|---|---|---|---|---|---|
| **XIP** | `0x02000120` | 583 612 | 582 104 | **1 508** | **99,7 %** |
| POOL | `0x01C20000` | 327 680 | 318 432 | 9 248 | 97,2 % |
| RAM (`.data+.bss`) | `0x01C08000` | 98 304 | 91 608 | 6 696 | 93,2 % |
| LOWBUF | `0x01C00000` | 32 768 | 30 564 | 2 204 | 93,3 % |
| RAMCODE | `0x01C70800` | 24 576 | 19 756 | 4 820 | 80,4 % |
| NOINIT | `0x01C7C000` | 15 696 | 15 572 | 124 | 99,2 % |

Secciones del `.elf` (`[AC791N]`): `.text` 561 552, `.hot_text` 15 816, `.ram_text` 3 792, `.c1_text` 148,
`.data` 796, `.bss` 90 776, `.pool` 311 256, `.c1_stacks` 7 176, `.lowbuf` 30 564, `.noinit` 15 572.

**No hay nada que recortar sin tocar código**: no existe `.eh_frame`, ni `.comment`, ni tablas de unwind — el
`.elf` sólo lleva las secciones que el firmware usa. La conclusión que ordena toda la fase es una: **el slot de
la aplicación tiene 1 508 B libres**. Cualquier función nueva compite con eso, no con la RAM.

## 2. Lo medido en esta pasada, y su resultado

| Sospechoso | Medición | Resultado |
|---|---|---|
| Secciones de descarte en el `.elf` | parseo de la tabla de secciones | **0 B**: sólo `.text/.data/.bss/.pool/.lowbuf/.noinit/.c1_*` |
| Duplicados en la imagen de sólo lectura | SHA-256 por símbolo, agrupado (pasada anterior) | **68 B** en 3 pares: LLVM ya fusiona constantes idénticas |
| Código muerto | link con `--gc-sections` (pasada anterior) | **32 B**: no queda función sin referencia |
| Fuentes del panel | `tools/gen_aa_font.py` | ya son caras **dispersas** con el juego de glifos elegido; recortar más rompe texto arbitrario |
| Presupuesto de render | `tests/target_budget.py build/felucca.dis` | **todo en o por debajo**: `perf_block` 253/254, `DRUM path` 1 153/1 153, `rev_room` 243/243, `rev_spring` 126/126, `perf_pre` 31/30 (+3 %, límite +10 %) |
| Coste de dibujo `[HOST]` | `build/ui_render` | HOME completo 275,2 µs, sin `force` 98,5 µs; visualizador 375,1 µs HIGH / 260,7 µs LOW; paneles con firma sin cambios 0,33–0,80 µs |
| Polifonía `[HOST]` | `poly_report_test` + `poly_stress_test` | 12/12 voces, PK 12, STL 0 en #107, STL 8 con 24 notas, DEN 0, 14,04 µs por bloque con 6 partes y FX |

## 3. Alternativas evaluadas y por qué no entraron

| Alternativa | Coste medido | Veredicto |
|---|---|---|
| Polifonía 14 voces | +1 134 B de `.bss` por voz (medido, `VOICE_POOL.md`); quedan 6 696 B; el pool tendría que ir a `VPOOLN=20` (+176 B) | **No**: entra en memoria, pero cambia el orden de asignación y obliga a re-bendecir renders; se decide con la comparación de sonido delante, no de paso |
| Compartir `proj_scratch` / `sec_stage` / `undo_st` (26,7 KiB) | solapamiento de vidas no demostrable (autosave durante PLAY) | **No**: libera LOWBUF, que **no** es la región escasa, y el riesgo es corromper un proyecto en una unidad |
| Comprimir fuentes en XIP | −14,9 KiB de constantes posibles | **No sin medir CPU**: decodificar por glifo en el dibujo (275 µs/frame ya en host) castiga la UI en el target, que es lo que hay que proteger |
| `UAC_FIR` (7 680 B de tabla) acortado | −~3,8 KiB | **No**: cambia la respuesta del audio USB; es una decisión de sonido, con renders delante |
| Mover código de `.hot_text` a XIP | +0 B de XIP libre (el código igual viaja en la imagen) | **No aporta** |
| `-Oz` → `-Os`/`-O2` | medido en su momento: `-Oz` ahorró 34 KB de imagen y 2,3 KB de RAM | **Se conserva `-Oz`**: ninguna alternativa lo superó en el target |

## 4. Consecuencia para los agregados de la fase

Los agregados de sonido (más opciones de LFO / envolventes / filtros / macros, mejoras de ARP, SEQ, SCL,
CHORD, FX y edición de presets) **no se implementaron en esta pasada**, y la razón es medida, no de criterio:
con **1 508 B** de XIP libre, cualquier motor nuevo o tabla de parámetros adicional no entra sin quitar algo
primero. El orden honesto es: liberar código (las tres vías caras de la tabla de arriba, cada una con su
comparación de sonido), y recién entonces agregar.

Lo que sí queda cerrado en esta fase es la **verdad de las mediciones** (fases 2 y 3) y este mapa: a partir de
aquí, cualquier propuesta de optimización tiene un presupuesto contra el que medirse, y las tres regiones
críticas están identificadas con su margen exacto.

## 5. Fase 1 de `v1.5.6`: lo que sí se pagó, y con qué se midió

El techo de esta fase era el de la tabla de §1, y la primera pasada lo dejó ahí: **1 508 B libres de XIP**. Lo que
cambió en `v1.5.6` no es un recorte de código sino **tráfico de panel**, que no estaba medido:

| Medición | Antes | Después | Clase |
|---|---|---|---|
| Frame quieto, 201 escenas (`ui_render --idle`) | 1 485 216 px mandados al panel | **109 056 px** | `[HOST]` |
| Frame quieto de una página (240 × 122) | 29 280 px | **0 px** | `[HOST]` |
| Imagen de la app (AC791N) | 582 200 B, 1 412 libres | 582 504 B, **1 108 libres** | `[ESTÁTICO]` |
| RAM `.data+.bss` | 91 612 B | 91 648 B | `[ESTÁTICO]` |

El mecanismo: `gfx.c cv_blit()` es el único camino a `lcd_blit`, y guarda el hash de la última banda enviada con su
ventana; si el canvas sale idéntico no se manda. `ui_render --idle` queda en la suite como guarda: si un frame
quieto vuelve a escribir píxeles, el paso falla.

Lo que sigue **sin** entrar por presupuesto es lo mismo que en §3 y §4: fuentes comprimidas (−14,9 KiB posibles,
sin medir su CPU de decodificación), `UAC_FIR` acortado (−3,8 KiB, cambia el sonido del USB) y cualquier motor o
parámetro nuevo. `v1.5.6` gasta 304 B de la holgura que había; quedan **1 108 B**.
