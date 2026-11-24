# FASE 2/9 — Motor de profundidad Quantum (v2.2.0)

Versión de este árbol: **v2.2.0** (`FELUCCA_VERSION`), identidad `FM-1_965`.
Clases: `[HOST]` x86-64 nativo, `[TARGET]` build AC791N, `[STATIC]` inspección. **No hubo unidad FM-1 física**:
ninguna compuerta de instalación cambia de estado.

## 1. Etapa 1 — estado y memoria

- **Repositorio**: rama `main` en v2.1.0 (`94a3de1`), árbol limpio, **0 issues y 0 PRs abiertos**, sin trabajo
  anterior a medias. Las fases previas están cerradas en `docs/` (CPU/RAM v2.0.3, identidad visual v2.1.0).
- **Auditoría de `.bss`/`.data`** `[STATIC]` sobre `build/felucca.map` y `nm --size-sort`: los mayores son
  `dly_buf` 131 072 (línea de delay), `cv_buf` 59 520 (canvas 240×124), `sl_buf` y `cloud_buf` 32 768 cada uno
  (slicer y reverb cloud), `proj_slot` 15 360, `trk` 10 584, `rev_comb` 9 874, `proj_scratch` 9 644,
  `sec_stage` 9 446, `undo_st` 7 680: **todos vivos y del tamaño que su función exige**. No apareció ninguna
  tabla duplicada ni buffer sin uso; las superposiciones que el proyecto ya había descartado con números
  (`proj_scratch`/`sec_stage`, `gen_keep`) siguen descartadas.
- **Memoria recuperada, medida**: el recurso escaso es la imagen. Reemplazar el dibujo píxel a píxel de la
  envolvente por el motor compartido bajó la imagen de **581 476 a 580 180 B**: **−1 296 B** (3 432 libres,
  suelo 2 048). En RAM el motor cuesta **+212 B** (82 del buffer de muestras, 100 del contexto de la envolvente,
  30 de estado) y quedan **5 456 libres** sobre un suelo de 4 096. Se intentaron dos variantes más baratas en RAM
  (contexto en `uint8_t`, contexto en la pila): las dos **subieron la imagen** 1 616 B y 1 936 B respectivamente,
  así que se descartaron con la medición en la mano.

## 2. Etapa 2 — el motor (`qz_`, `firmware/src/ui_graph.c`)

Pseudo-3D con geometría 2D, sin matrices ni senos por punto:

- `qz_set(q, x0, y0, w, h, n, frente, fondo)` describe el stack: la caja de la capa de adelante, cuántas capas,
  y de dónde a dónde va la rampa de color.
- Cada capa `k` se dibuja con `sw = w·1000/(1000+k·sx)` y `sh = h·1000/(1000+k·sy)` (9 % más chica por capa),
  corrida `dx = w/14` a la derecha y `dy = h/6` arriba: eso es toda la perspectiva.
- `qz_curve(q, fn, ctx)` pide los niveles a la página (`fn(ctx, k, u)`, `u` 0..1000) y los guarda en **un solo
  buffer estático de 41 muestras** (`qz_v`, 82 B) antes de trazar cada capa: la curva se evalúa una vez, no una
  vez por capa, y no hay asignaciones.
- La capa 0 es la de adelante: 2 px y un piso de luz de dos filas; las de atrás, 1 px y mezcladas hacia el frío
  con `ux_mix` (GREY/MONO siguen neutros porque los tokens lo son).
- `env_fall()` da `exp(-4.6u)` en nueve pasos Q15 (`ENV_E`): la misma caída que usa el DSP para una etapa.

## 3. Etapa 3 — dónde se aplicó

| Pantalla | Capas | Qué es cada capa |
|---|---|---|
| **ENV** | 4 | la envolvente de este track adelante; detrás, las de otros tres tracks, leídas de `trk[i].p[pg->id[]]` |
| **LFO** | 3 | la misma onda con un paso de fase entre capas (por dónde pasó la modulación) |
| **CHORUS** | 3 por canal | el carril y dos copias de su rastro, con el desfase de STEREO |
| **DELAY** | — | ya tenía profundidad real desde v2.1.0 (túnel concéntrico por eco audible) |
| **BUSES** | — | conserva las miniaturas; la consola 3×2 no necesita profundidad para leerse |
| REVERB / SLICER / SEQ / MASTER | — | sin cambios a propósito: escalera, compuertas y medidores se leen mejor planos |

Nada de esto es animación decorativa: las capas de ENV son los cuatro tracks reales con sus cuatro knobs
reales; las del LFO son la onda real del motor con un paso de fase; las del chorus, el mismo carril con el
mismo desfase que el DSP aplica.

## 4. Etapa 5 — medidas

| Medida | v2.1.0 | v2.2.0 |
|---|---:|---:|
| `[TARGET]` imagen | 581 476 / 583 612 (2 136 libres) | **580 180 / 583 612 (3 432 libres)** |
| `[TARGET]` `.data+.bss` | 92 636 / 98 304 (5 668 libres) | 92 848 / 98 304 (5 456 libres) |
| `[TARGET]` LOWBUF / POOL / NOINIT | 1 120 / 15 556 / 124 libres | 1 120 / 15 556 / 124 libres |
| `[HOST]` renders de audio | 311 | 311 idénticos |
| `[HOST]` lint de alineación | 0 | 0 (3 953 168 mediciones) |

## 5. Lo que no se probó

- **No hubo unidad FM-1**: los gráficos se verificaron en el render de host y en el emulador WASM.
- `valgrind` no está en el entorno: el perfilado fino de la UI queda como pendiente declarada.
