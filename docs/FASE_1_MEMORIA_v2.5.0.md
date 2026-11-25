# Fase 1 — Memoria: el hueco de RAMTEXT, y lo que no se puede tocar (v2.5.0)

Medido sobre el build de AC791N de este árbol (`build/felucca.map`, `build/felucca.elf`), no sobre informes
anteriores. Clase de cada cifra: **[TARGET]** el mapa que escribe el toolchain real; **[HOST]** las pruebas.

## 1. El mapa, antes

| Ventana | Usado | Libre | Suelo |
|---|---|---|---|
| LOWBUF (0x8000) | 31 648 | 1 120 | 1 024 |
| **RAM .data+.bss (96 KiB)** | **93 556** | **4 748** | 4 096 |
| POOL (0x50000) | 312 124 | 15 556 (spare del pool 8 384) | 4 096 / 8 192 |
| **RAMTEXT (0x6000)** | **3 936** | **20 640** | 1 024 |
| NOINIT (0x3D50) | 15 572 | 124 | 0 |
| XIP imagen | 580 728 | 2 884 | — |

Lo primero que salta: **RAMTEXT es una ventana de 24 KiB con 3 936 B de código y 20 640 B vacíos**, mientras la
ventana de RAM estaba a 652 B de su suelo. RAMTEXT no es un hueco libre del montón: es SRAM reservada para el código
que tiene que seguir corriendo mientras el flash está ocupado, y su cola no la usa nadie.

## 2. Lo que se cambió

Cuatro buffers que **sólo toca el bucle principal** se mudan a la cola de RAMTEXT, en una sección nueva
`.ramdata` (`firmware/app.ld`), con `ASSERT` de ventana:

| Buffer | Bytes | Quién lo usa |
|---|---|---|
| `upf` (espejo de los presets de usuario) | 3 728 | editor, backup, STORE |
| `gen_keep[2][NSTEP]` (scratch del generador) | 2 176 | `gen.c`, undo del generador |
| `motion_base[NTRK][P_COUNT]` (valores del patch) | 1 524 | editor (`motion_base_value`) |
| `hc_tab[HC_FONTS]` (tablas Huffman de las caras) | 1 548 | `gfx.c`, al decodificar una cara |
| **Total** | **8 976** | ninguno desde la ISR de audio |

Tres detalles que hacen que esto sea seguro y no una mudanza a ciegas:

1. **`NOBITS`**: los cuatro van a `.bss.ramdata`, así que **no entra un solo byte en la imagen** (comprobado:
   580 728 B antes y después). Si hubieran ido a una sección normal, sus ceros habrían viajado en el flash.
2. **El boot los limpia**: la SRAM arranca indefinida, y un `upf` con basura podría pasar el `magic` o un `hc_tab`
   con un puntero cualquiera haría que la decodificación de una cara leyera donde no debe. `main.c` limpia
   `_rd_start.._rd_end` en el mismo bucle que ya limpia 312 KiB de pool y 31 KiB de lowbuf; el emulador declara
   el rango vacío con el mismo truco que ya usaba para LOWBUF y HOT.
3. **Un `ASSERT` en el linker** falla el build si la ventana se llena, y `tools/mem_budget.py` la mide ya con
   `.ramdata` dentro (`RAMTEXT: [".ram_text", ".ramdata"]`).

## 3. El mapa, después

| Ventana | Antes | Después | Δ |
|---|---|---|---|
| **RAM .data+.bss** | 93 556 (4 748 libres) | **84 524 (13 780 libres)** | **+9 032 B de margen** |
| RAMTEXT | 3 936 (20 640 libres) | 12 768 (11 808 libres) | −8 832 |
| LOWBUF | 31 648 (1 120) | 31 648 (1 120) | 0 |
| POOL | 312 124 (spare 8 384) | igual | 0 |
| NOINIT | 15 572 (124) | igual | 0 |
| Imagen XIP | 580 728 | 580 728 | 0 |

Objetivo de la fase para RAM (**≥ 8 KB libres**): **cumplido, 13 780 B** (4 748 → 13 780, +190 %). No se quitó ni
una voz, ni un motor, ni un preset, ni un píxel: los goldens de audio y la puerta de píxeles salen idénticos.

## 4. Lo que se miró y NO se cambió (con el número en la mano)

| Candidato | Bytes | Por qué no |
|---|---|---|
| `dly_buf` / `cv_buf` / `sl_buf` / `cloud_buf` | 131 072 + 59 520 + 32 768 + 32 768 | Son las líneas de FX y el lienzo: recortarlas es calidad sonora o visual, y la fase lo prohíbe |
| `trk[6]` (10 584) | — | `track_t` está empaquetado a mano desde hace fases; no hay campos de 32 bits donde bastan 16 sin tocar la persistencia |
| `fm6_note[6]` (7 200) | — | Estado por voz del motor FM6, leído en la ISR |
| `WT_GEN` (6 192) | — | Tabla generada al arranque y leída en el camino de audio (el comentario de `dsp.c` explica por qué está en RAM) |
| `drum_kit[6][8]` (5 376) | — | Estado de las percusiones, ISR |
| `proj_scratch` / `undo_st` / `sec_stage` (26 770) | — | Ya viven en LOWBUF; `sec_stage` tiene un campo `volatile` que lee la ISR, así que no se mueve sin revisar ese camino |
| `hc_tab` a una sola ranura | 516 | La búsqueda ya está medida: reconstruir la tabla por cara cuesta más de lo que ahorra (`gfx.c`, 1.3.2) |
| `-O2` global | — | Ya medido y descartado en `tools/build.py`: no entra en XIP y rompe el presupuesto de `analog_render` |

## 5. El objetivo de XIP (8 KB libres): no alcanzable sin recortar constantes en uso

La imagen son **449 768 B de código + 126 180 B de constantes**. Para llegar a 8 KB libres habría que quitar
~5 300 B, y el inventario de constantes grandes está entero en uso:

| Constante | Bytes | Uso |
|---|---|---|
| `AF_S_DATA` + `AF_M_DATA` + `AF_L_DATA` + `AF_S_G` | 19 749 | Las tres caras de texto (glifos comprimidos y sus tablas) |
| `ANALOG_PRESETS` y las demás tablas de presets | ~20 000 | Los 290 presets, leídos al cargar |
| `GENRE` (2 548) + `TP` (2 540) + `GP` (1 500) | 6 588 | El generador y los valores por defecto de los parámetros |
| `UAC_FIR` (3 840), `WT_COEF` (2 048), `FM6_EXP2_D2` (2 046), `AI12/AI16` (6 033), `KC_DATA` (1 947) | — | Audio USB, wavetable, curva del FM6, iconos, keycaps |

`--gc-sections` ya está activo, así que no queda código muerto que soltar. La alternativa real (y el trabajo
pendiente que propongo) es **un auditor de tamaño por característica** que diga qué constantes no se referencian
desde ningún camino alcanzable, y sólo después decidir. Bajar el listón de la fase a «no perder margen» no es una
opción: 2 884 B libres es el margen que hay hoy, y sigue igual.

## 6. CPU

No se tocó ningún camino de audio ni de interrupción: el cambio es de *dónde* viven cuatro buffers, y todos están
en SRAM en ambos casos (misma latencia). El único coste nuevo es **una pasada de 8 976 B al arranque** (≈ 2 244
escrituras de 32 bits), en el mismo bucle que ya escribe 312 KiB. Las pruebas de CPU del target y del host salen
exactamente iguales.
