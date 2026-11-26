# FASE 5/5 — cierre, estabilidad y estado final

Fecha: 2026-11-26 · Versión: **v2.6.0** · Clases de medición: `[AC791N]` compilación del
target, `[HOST x86-64]` esta VM, `[EMU]` WebAssembly. Ninguna afirmación de hardware.

## 1. Qué se revisó y qué se encontró

Esta fase no repite los arreglos de las fases 1–4: parte de la suite completa y de lo que
quedó abierto en el árbol. El resultado fueron **cuatro defectos reales**, todos corregidos,
y **dos límites medidos** que se documentan sin maquillar.

### 1.1 Dos pruebas que ya no probaban el firmware (corregido)

`sine_table_test` y `fm6_exp2_test` comprobaban la tabla del seno y la curva exp2 de FM6
contra el generador, pero **decodificaban el formato viejo**: diferencias de 8 y 16 bits. La
compactación a tres bits (`tools/gen_tables.py`, función `bits3`) hizo que `SINE_D2` pasara a
384 B y `FM6_EXP2_D2` a 384 B, y desde entonces las dos pruebas fallaban —y con ellas la
suite—. Lo que estaba mal era la prueba, no la tabla: el firmware decodificaba con el lazo
empaquetado y los goldens de audio seguían idénticos.

- `tests/sine_table_test.c` y `tests/fm6_exp2_test.c` decodifican ahora con **el mismo lazo
  que `sine_init` / `fm6_exp2_init`** (tres bits, valor + 3) y verifican tamaño, monotonía,
  punto de cuarto, simetría y el último punto. 6 y 7 comprobaciones, 0 fallos.
- Ahorro que confirman, `[HOST]` sobre los headers generados: 2 050 → 384 B y 4 100 → 384 B,
  **5 382 B de XIP** que ya estaban en el árbol y que la suite no estaba midiendo.
- Los comentarios de `dsp.c` y `fm6_core.c` describían el formato de 8/16 bits; ahora dicen
  el real.

No se tocó el DSP, ningún golden de audio ni un umbral.

### 1.2 Los números públicos estaban viejos (corregido)

La pantalla ABOUT decía «10 Engines. 290 Presets.», y README, BUILDING.md y `fmhub.json`
repetían 290. Las tablas de los diez motores tienen **298 entradas, 296 de fábrica**: los dos
kits DRUM de la pre-1.6 (`HEARTH KIT`, `CYBER DRUM`, `.legacy_n = 2`) existen para que un
proyecto viejo cargue su sonido, pero nunca se listan.

- `firmware/src/ui_menu.c`, `README.md`, `BUILDING.md` y `fmhub.json`: **296**.
- La fuente citada pasa a ser `preset_audit_test` (lee las tablas del motor en ejecución);
  `preset_slots_test.py` cuenta 295 porque su expresión regular no ve tres entradas, y no se
  usa ya como fuente del número público.

### 1.3 El manual no documentaba HOLD (corregido)

La perilla 4 de SEQ > TIMING (duración real grabada: GATE o 1/16 … 15/16), añadida en la fase
4, faltaba en la tabla y en el texto de TIMING de `docs/MANUAL.md` y `docs/MANUAL.es.md`.

### 1.4 La puerta de píxeles estaba congelada en v2.5.1 (corregido)

`tests/ui_pixels.txt` no se había vuelto a generar desde v2.5.1, así que la cadena de versión
del splash, ABOUT y las tarjetas del hub hacía fallar **89 cuadros** (todas las paletas) desde
v2.5.2. Se regeneró la base con `tests/ui_pixels.py --update` **después** de comprobar que los
únicos cuadros que cambian son los que llevan la versión y el recuento de presets; el motivo
está en el commit y en el CHANGELOG.

## 2. Medidas finales

`[AC791N]` compilación del target, `tools/resource_map.py`:

| Recurso | Valor | Libre | Suelo |
|---|---|---|---|
| RAM `.data + .bss` | 75 600 B | 22 704 B | 4 096 |
| POOL (DSP) | 303 328 B | 24 352 B | 8 192 |
| LOWBUF | 28 488 B | 4 280 B | — |
| RAMTEXT | 20 596 B | 3 980 B | — |
| NOINIT | 15 572 B | 124 B | 64 |
| XIP (imagen) | 576 552 B | 7 060 B | 2 048 |

`[HOST]` presupuesto del bloque de audio (`tests/target_budget.py`): BLOCK_PATH 34 832
instrucciones ponderadas, +4 % sobre la línea base y dentro del límite +10 %. `[HOST]`
`audio_stress`: p99 por bloque dentro de los 725 µs en esta VM, 0 muestras no finitas, 0
clamps. `regress`: 316 goldens sin cambios en esta fase.

La fase no introduce trabajo nuevo en el camino de audio: los únicos binarios que cambian son
las dos pruebas y la cadena de versión. La memoria recuperada que se puede citar es la que ya
estaba en el árbol por las compactaciones anteriores y que ahora está **medida y probada**:
5 382 B de XIP entre el seno y exp2, más los 384 B de `step_t` de la fase 4.

## 3. Barrido de pantallas y paletas

`[HOST]` `ui_render` dibuja todas las páginas, motores, valores de columna y las 22 paletas:
0 hallazgos de lint (solapamientos, recortes, desbordes de tarjeta), 0 medidas de alineación
fuera de 1 px, y el barrido de parpadeo (`--bands`, `--regions`, `--idle`) sin píxeles
borrados y reescritos. El coste de dibujo por pantalla está en `build/ui_new/report.txt`; las
páginas con animación ya limitan su trabajo (HOME a 200 µs `[HOST]` en HIGH / 112,5 µs en LOW,
panel del roll a 37,5 µs, 0 µs cuando la firma no cambia) y el audio tiene prioridad por el
presupuesto de bloque. No se cambió el dibujo: no había un defecto de rendimiento que
justificara tocar arte ya medido.

## 4. Límites que quedan abiertos (medidos, no silenciados)

1. **`text_spacing_test`, fuente S: 0,258 px en «Gl»** contra el límite de 0,25 px (1 par de
   199). Ya estaba documentado en `docs/FASE2_XIP_RECURSOS_2026-11-25.md` y se reproduce igual
   sin los cambios de esta fase. Cerrarlo exige rediseñar las avanzadas de la S, lo que mueve
   píxeles en todas las pantallas; no se bajó el umbral.
2. **`cloud_hot_test`, FREEZE (SIZE 127) con RATE 127: 6,1 dB de caída** entre el envío y la
   retención 1–2 s después, donde el criterio pide ≤ 6,0. No hay clamp, ni click, ni cruce del
   cabezal de escritura en ninguna de las 20 configuraciones medidas; es un umbral de 0,1 dB en
   el extremo del rango. Tocar la normalización de granos para ganar 0,1 dB cambiaría los
   goldens de CLOUD sin una medición que lo justifique.

## 5. Lo que esta fase no puede afirmar

- No hay lectura ni prueba en una unidad FM-1 física: todo lo anterior es compilación,
  `[HOST]` o `[EMU]`. Ningún resultado se presenta como de hardware.
- La suite completa tarda más que el límite de 5 minutos por comando de este entorno; se
  ejecutó en la terminal compartida y quedó registrada en `build/tests_full.log`, con las dos
  pruebas corregidas reejecutadas por separado y en verde.
