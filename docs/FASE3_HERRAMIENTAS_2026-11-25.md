# FASE 3/5 — Herramientas sonoras livianas (WAVETABLE, ADDITIVE, RULE)

Fecha: 2026-11-25 · versión v2.5.3 · base v2.5.2 (`4e34447f`)

Todo lo de este documento está implementado, probado y en el binario. Las medidas se etiquetan por clase:
`[AC791N]` es la compilación real con el toolchain de JieLi (no una medición en hardware), `[HOST x86-64]`
es el host de pruebas, `[EMU]` el emulador WebAssembly.

## 1. Auditoría de lo que ya existía

| Área | Lo que hay de verdad | Decisión |
| --- | --- | --- |
| WAVETABLE | `eng_wavetable.c`: 8 frames × 3 límites de banda, MORPH/POSMOD/FOLD en CHARACTER, MORPH/DETUNE/UNI/SUB/CUT/RES/DRIVE/HARM en EDIT, warp de fase sinusoidal con WARP (`P_X0`, `xamt`), 3 122 B XIP | añadir **PHASE** reutilizando WARP como profundidad |
| ADDITIVE | `eng_additive.c`: 8 perfiles (ORGAN…METAL), SPECTRUM/TILT/DECAY/STRETCH/SUB/EVOLVE, normalización por bloque a 32767, 2 186 B XIP | DRAW sobre los 8 parciales y un segundo slot CHARACTER |
| Modulación | matriz de 8 slots (4 en `p`, 4 en `mx`), LFO con formas, RAND por nota, M1..M4, CC1/CC11/aftertouch/CC2 | no duplicar nada |
| CC2 Breath | ya es fuente `MS_EXPR`: `mod.c` toma `min(ex_off, br_off)`, CC2 escribe `br_off` | **sin cambios** |
| CC5 Portamento Time | ya está en `MIDI_CC_MAP`: `{5, P_GLIDE}`, escribe el glide de la pista como una perilla | **sin cambios** |
| Aleatoriedad por nota | `MS_RAND` (xorshift32 en el note-on) | **sin cambios** |

No hay REMAP propietario, ni tablas de Serum, ni presets, ni muestras: las tres transformaciones y la regla
celular son formulaciones propias sobre la tabla que el motor ya genera (`tools/gen_tables.py`).

## 2. WAVETABLE > PHASE

`P_LRTRG` (alias de `P_FM1_ATK`) es el selector. Es un slot retirado que sólo LOFI usaba (su `T.CLK`), así que
no se toca `P_COUNT` (127, lleno) ni el formato de los stores. La página PHASE sólo aparece en WAVETABLE; la
RETRIG de LOFI sigue igual.

| Modo | Qué hace | Coste por muestra |
| --- | --- | --- |
| 0 SINE | el warp de siempre: `ph + sin(ph)·profundidad`, hasta ¼ de ciclo | 0 extra |
| 1 KNEE | `sin(ph) + ½·sin(2ph)` en la fase: una rodilla asimétrica | 1 seno extra |
| 2 HOLD | retiene la fase en 32 pasos del ciclo y mezcla con la continua | 0 lecturas extra |
| 3 X2 | lee dos ciclos y mezcla las dos lecturas | 1 lectura interpolada extra |

Anti-aliasing: con el efecto activo se usa el frame de 16 armónicos (`lvl+1`) y la profundidad se desvanece
entre 459 y 689 Hz. Así HOLD (32 muestras por ciclo) y X2 (32 armónicos) quedan por debajo de Nyquist por
construcción. Con WARP en 0 el modo no se aplica y el render es **bit a bit** el anterior (el test lo compara
muestra a muestra, y los goldens no cambian).

Profundidad modulable: WARP sigue siendo `CHARACTER 1`, destino `MD_X1` de la matriz y del LFO.

## 3. ADDITIVE > DRAW y CHARACTER 2 RULE

**DRAW** escribe los niveles de los 8 parciales en `P_LN0..P_LN7`, los bytes por pista que DRUM usa para sus
niveles (una pista ADDITIVE no los leía). Default 127 = perfil intacto, y ese camino es exactamente el de antes.
La página DRAW (H1..H4) y DRAW 2 (H5..H8) sólo aparecen en ADDITIVE; LEVELS/LEVELS 2 siguen siendo de DRUM.

**RULE** es `CHARACTER 2` (`P_X1`, `F_PCTINV`, 127 = apagado). Por bloque, dos generaciones de una regla
original de 8 celdas:

```
row = ((row<<1) ^ (row>>1) ^ (row>>3) ^ (row>>6)) & 0xFF     (dos veces, semilla SPECTR y parte)
amp[k] = 24 + 58·c[k] + 28·c[k+1] + 17·c[k+5]               (0..127)
```

y se mezcla con el perfil: `v' = v·(127-ra) + amp·258·ra`. La normalización posterior divide la escala 127
completa, así que **no hace falta ninguna división por parcial**; DRAW usa `(g·lv·258 + 16384) >> 15` para
1/127 (error < 2 LSB, y 127 ni siquiera entra en esa rama). Todo el trabajo es por bloque: 0 por muestra.

## 4. Coste

| Medida | v2.5.2 | v2.5.3 | Δ |
| --- | --- | --- | --- |
| Imagen `[AC791N]` | 574 752 B | 576 172 B | **+1 420 B** |
| Margen de la ranura | 8 860 B | 7 440 B | −1 420 B |
| XIP código | 449 632 B | 450 448 B | +816 B |
| XIP constantes | 120 340 B | 120 944 B | +604 B (5 presets) |
| RAM `.data+.bss` | 76 048 B | 76 048 B | **0 B** (22 256 libres) |
| POOL / LOWBUF / código RAM | 296 220 / 29 256 / 3 940 | igual | 0 |
| Presupuesto estático del bloque | — | +5 % (límite +10 %) | ok |

`[HOST x86-64]`, 8 notas, WAVETABLE con UNI, ns por muestra (mínimo de 30 procesos; esta VM tiene ±20–30 % de
ruido, así que son cotas, no ciclos): SINE 18,3; HOLD 19,5 (+5 %); KNEE 22,8 (+20 %); X2 24,3 (+33 %). ADDITIVE
por bloque (RULE + DRAW): la diferencia queda dentro del ruido del host (≈1–2 ns/sample, una división por
bloque menos que la variante ingenua). Ningún modo toca el camino por defecto.

## 5. Pruebas

- `regress`: **316 goldens, 0 cambiados** (311 previos + los 5 nuevos), 0 fallas de health/voces. El chequeo
  de CPU del host falla en esta VM también **con el binario de HEAD** (mediana 2,8× la baseline): es el host,
  no el cambio; se verificó con un binario compilado desde `HEAD`.
- `wavetable_test`: los tres modos no cambian nada con WARP 0 (muestra a muestra), cambian el sonido con
  profundidad, y a 330 Hz sólo llevan armónicos. `additive_test`: RULE apagado en 127, DRAW 64 baja la
  fundamental 11,9 dB contra el 2.º armónico, RULE 40 cambia el espectro.
- `six_track_live` 61/61 s; `phase2_test`; `mod_test` (5 demos, coste); `theme_test` 22 paletas;
  `text_ref_test` 6 732 cadenas 0 píxeles; UI: 203 pantallas × 22 paletas + barrido de páginas/valores,
  **0 hallazgos** de lint, 0 tintas sobre su color, 0 medidas de alineación fuera de 1 px.
- `target_budget.py`: todas las líneas dentro (BLOCK_PATH +5 %), `wt_render`/`add_block` no estaban en la
  lista y se añadieron sus tamaños al mapa (`0x598` y `0x206`).
- `tests/ui_render.c` tenía el hook `GFX_HOOK_PEN` leyendo `.by/.bh` del `aag_t` anterior al empaquetado de la
  FASE 2/5: no compilaba en HEAD. Se corrigió a `AG_BY/AG_BH` (sin cambiar ninguna medida).

## 6. Límites

- No hay medición en un FM-1 real: `[AC791N]` es compilación, `[HOST]` es el host de CI.
- El banco de pruebas no puede separar del todo el coste de RULE/DRAW del ruido de esta VM.
- Los presets nuevos escriben sólo 4 de los 8 niveles de DRAW (los otros 4 quedan como los tenga la pista);
  DRAW es un control por pista, no un parámetro del preset.
