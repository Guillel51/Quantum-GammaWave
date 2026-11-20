# Fase 3 — sonido: formas nuevas, resonancia como destino y dos tipos de filtro (v1.5.24)

Los números de esta página son `[AC791N]` (binario compilado con el toolchain `pi32v2` y el SDK fijados),
`[HOST]` (x86-64, las fuentes reales) o `[ESTÁTICO]` (medido sobre el `.elf`/`.bin`/`.dis`). **No hay una FM-1 en
este entorno**: nada de aquí afirma sonido, panel ni DAC de la unidad. Todo lo que se dice de "cómo suena" es
una forma de onda medida en el host.

## 0. Lo que la fase pedía, y lo que el árbol deja

La fase pedía, para cada uno de los once motores, una opción nueva y audible, más modulación y FX. El árbol
llega a esta fase con dos techos que ya están medidos y documentados (`docs/FASE3_CAPACIDADES_2026-11-19.md`,
`docs/FASE4_OPTIMIZACION.md`) y que siguen en pie:

| Recurso | Estado en v1.5.24 | Consecuencia |
|---|---|---|
| Ids de parámetro por pista | **125 de 127** (`core.h`, `P_COUNT`) | un parámetro nuevo rompe el formato de proyecto, los motion records y la compatibilidad de todo lo guardado |
| XIP de la app | **582 340 B de 583 612** (1 272 libres) | una tabla o un motor nuevo no entra; lo que entra son decenas de bytes por opción |
| `.data+.bss` | 86 912 B de 98 304 (11 392 libres) | la RAM no es el límite |

Por eso esta fase **no agrega parámetros**: agrega **valores nuevos dentro de parámetros que ya existen**
(`P_LWAVE`, `P_M1DST`, `P_FTYPE`). Un valor nuevo no cambia el formato de nada —un preset o un proyecto que
guarda `3` sigue leyendo `3`— y cada uno cuesta decenas de bytes. Las cuatro incorporaciones suman **196 B**
de imagen, medidos.

## 1. LFO: `PLUK`, una caída dentro de cada ciclo

`P_LWAVE` tenía nueve formas: SIN, TRI, SAW, SQR, S&H, RAMP, EVO, HSIN, PULSE. La décima es **PLUK**
(`firmware/src/voice.c`, `lfo_wave`):

```
x = phase >> 16                 0 .. 65535 dentro del ciclo
PLUK(x) = (1 - x)^4             en Q15: 32 767 en el ataque, 0 al final del ciclo
```

No es otra forma de onda: es **un disparo atado al RATE del LFO**. En ON el ciclo es el de siempre; en
`RATE` negativo (in time) cae en el paso de la rejilla que se elija, así que es un generador de "plucks"
rítmicos de filtro o de amplitud, sin usar el secuenciador. Se distingue de SAW (línea recta) y de RAMP
(recta inversa y bipolar) por la curva y por el signo: PLUK nunca pasa de cero.

Medido en el host (`tests/mod_test.c`, sección FASE 3): el ciclo empieza en **32 768**, a un tercio vale
**14 266** (menos de la mitad), y en el último cuarto es positivo mientras RAMP ya es negativo — lo que
prueba que no es una forma existente con otro nombre.

**SLEW y INVERT** (los dos CHARACTER de la FASE 2) siguen actuando encima, así que PLUK + SLEW es una caída
redondeada y PLUK + INVERT una subida.

## 2. Matriz: `RES`, la resonancia del filtro como destino

Los destinos eran 26 (`MD_*`, `mod.c`): PITCH CUT SHP AMP PAN DIST CHO DLY REV RATE VIB E1..E8 CLIP PNCH
X1..X4. El filtro de la parte tenía **corte** (CUT) modulable y **resonancia** no. `RES` (valor 26, al final
de la lista para que todo número guardado siga significando lo mismo) escribe `P_FRES` durante el bloque de
la parte, igual que PAN o los envíos:

- per-block: `mod_begin` guarda el valor, suma `((src × AMT) >> 6) × 127 >> 15` y limita a 0..127;
  `mod_end` lo devuelve. El filtro (`voice.c track_filter`) lee `P_FRES` como siempre, así que el cambio
  llega sin tocar el DSP del filtro.
- Amordazado: con el LFO arriba y AMT 63 la resonancia del bloque llega a 127 y en el bloque siguiente
  vuelve a lo guardado; con el LFO abajo cae a 0. Las dos cosas están en el test, y el valor guardado
  sobrevive al clamp.
- La UI lo nombra `RES` y le da el icono de forma; el editor no cambia (la lista `N_MDST` es la del enum).

Sirve para lo que no había: un barrido de resonancia por LFO o por envolvente — el "wow" de un filtro ácido,
un formante que se estrecha en cada nota, una cola que se vuelve silbido.

## 3. Filtro de la parte: `NOTCH` y `SHELF`

El filtro de la parte tenía seis tipos: LP HP BP COMB VOWEL SCREAM. Los dos nuevos salen **del mismo
state-variable filter**, de sus salidas que ya se calculaban (`v2` = paso bajo, `bp` = banda):

| Tipo | Fórmula | Qué es |
|---|---|---|
| `NOTCH` (6) | `x - bp` = LP + HP | el corte queda fuera; pasan los dos lados |
| `SHELF` (7) | `x + v2` = x + LP | un estante de agudos planos: +6 dB por debajo del corte, el resto como estaba |

Los dos están en `voice.c tf_run` (el filtro de la parte, para todos los motores) y en `eng_analog.c
svf_mode` (el filtro por voz de ANALOG y LOFI), así que **los once motores los tienen**. El dibujo de
respuesta del visor (`ui_being.c`, `gw_filter_resp_y`) tiene sus dos casos, y `is_vowel` ya no clasifica
como vocal cualquier tipo ≥ VOWEL.

Medido en el host (`tests/filter_types_test.c`, nuevo, por `track_filter`, con LP y HP como referencia):

| Barrido 80 Hz .. 6,4 kHz, CUT 72, RES 40 | Medición |
|---|---|
| `NOTCH`: el punto más profundo | **−11,9 dB** en 873 Hz, con los dos extremos dentro de 0,3 dB de unidad |
| `SHELF`: 80 Hz | **+6,0 dB** (LP: 0,0 dB) |
| `SHELF`: 6 400 Hz | **0,0 dB** (LP: −32,2 dB) |
| `SHELF` con RES 127 | finito, por debajo de +40 dB en todo el barrido |

La conmutación entre tipos ya tenía su crossfade de 8 bloques desde 0.6.81 y los dos nuevos entran por el
mismo camino: cambiar de LP a NOTCH no hace clic.

## 4. Los motores, uno por uno: qué se evaluó

La tabla es el trabajo de la fase sobre los once motores. "Ya existe" quiere decir que la idea **no** se
implementó porque ya estaba; "no entra" quiere decir que la medición la bloquea hoy.

| Motor | Ya existe (no se repite) | Candidato evaluado | Veredicto |
|---|---|---|---|
| ANALOG | 11 formas (SAW SQR TRI SIN PWM SYNC RING SAW3 TUBE FOLD PM), MORPH entre formas, 8 EDIT, CHARACTER KTR+ / TONE, filtro por voz de 8 tipos | un oscilador 3 con su propio nivel e intervalo | **no entra**: tres ids de parámetro (no hay) + ~400 B de DSP y render |
| PHASE | línea 1 y 2, DCW, FB (realimentación de fase), LINE = PM, HARM / NOISE en CHARACTER | un "hard sync" de la línea 2 sobre la 1 | **no entra**: el barrido de sincronía pide una fase por voz extra (~120 B de `.bss`) y otro id; el PM ya da el movimiento |
| FM6 | 6 operadores, algoritmos, feedback, TO NE / FOLD en CHARACTER, envolventes propias | un operador 7 como modulador global | **no entra**: 1 id + tabla de algoritmo + render; el "feedback" ya cubre el ruido metálico |
| WAVETABLE | MORPH, POSMOD, WARP, FOLD, UNI, SUB, HARM, morph por LFO/matriz | interpolación de dos tablas a la vez (crossfade de frames) | **no entra**: la tabla ya se interpola por MORPH continuo; un crossfade extra duplica memoria de tabla |
| WHEEL | LEAK, ROTOR+, P.DCY, barras y rotor por bloque | un segundo rotor desafinado | **no entra**: +2 estados por voz y ~300 B; el rotor ya se modula por matriz |
| DIGITAL | modos de byte/flotante, RTRG, corrupción, SWEEP | un modo "bit-reverse" | **ya existe** el equivalente en la corrupción de bits de LOFI (no se duplica) |
| LOFI | BITS+, RATE+, SWEEP, corrupción, DCW por voz | dither con ruido rosa | **no entra**: es un timbre, pero necesita un id; el NOISE del motor ya se mezcla por pista |
| ADDITIVE | espectro parciales + TILT + EVOLVE | un parcial 13-16 | **no entra**: +4 osciladores por voz × 12 voces = CPU del render por voz, sin unidad para medirlo |
| FORMANT | F1..F3, ARTIC, NASAL, TALK | una cuarta formante | **no entra**: +1 formante en el lazo del render por voz, y el ancho de banda ya se abre con ARTIC |
| NOISE | modos, BAND, EVOL, HPF | una coloración "pink" extra | **ya existe**: el modo META y el BAND la cubren |
| DRUM | 8 lanes, transient, cuerpo, tono, pitch env, kits | nuevos presets DRUM-X | **no se agregan** por pedido explícito de la fase |

Lo que sí llegó a **todos** los motores es lo de §3: el filtro de la parte es el mismo para los once, y
NOTCH/SHELF son carácter nuevo en cualquiera de ellos (un pad de WAVETABLE con NOTCH, un bajo de FM6 con
SHELF, un DRUM con el corte fuera). Lo mismo vale para §1 y §2: el LFO y la matriz son por pista, no por
motor.

## 5. Coste, medido

| Medición | v1.5.23 | v1.5.24 | Clase |
|---|---|---|---|
| Imagen de la app | 582 144 B (1 468 libres) | **582 340 B (1 272 libres)** | `[AC791N]` |
| `.data+.bss` | 86 912 B | **86 912 B** | `[AC791N]` |
| Coste de las cuatro incorporaciones | — | **+196 B** | `[AC791N]` |
| `.pool` / LOWBUF | 311 256 / 30 564 B | sin cambios | `[AC791N]` |

196 B para: una forma de LFO, un destino de matriz, dos tipos de filtro, sus dos curvas en el visor y cuatro
etiquetas. No hay RAM nueva, ni tabla nueva, ni estado nuevo por voz: los dos tipos de filtro usan las salidas
que el SVF ya calculaba, y `RES` escribe un parámetro que ya existía.

## 6. Compatibilidad

- **Presets y proyectos**: ningún formato cambia. Los valores guardados de `P_LWAVE` (0..8), `P_M1DST` (0..25) y
  `P_FTYPE` (0..5) significan lo mismo; los nuevos son valores que antes no se podían guardar. Un preset de
  v1.5.23 carga en v1.5.24 sin migración y suena igual (los 306 renders golden del `regress` lo comprueban).
- **MIDI**: los tres valores nuevos son parámetros que ya viajaban por el editor y el editor no cambia de
  protocolo: la lista de nombres es la misma tabla.
- **Motion records**: graban ids de parámetro, no valores de enum, así que no hay nada que migrar.

## 7. Pruebas ejecutadas

| Check | Resultado | Clase |
|---|---|---|
| `tests/mod_test.c` (sección FASE 3: PLUK y RES) | **all passed**, 11 comprobaciones nuevas | `[HOST]` |
| `tests/filter_types_test.c` (nuevo, 11 comprobaciones) | **all passed** | `[HOST]` |
| `tests/regress.c` (306 renders golden) | **0 changed, 0 gone**, 0 fallos de voz/routing | `[HOST]` |
| `tests/audio_test.c`, `poly_phase2_test`, `eq_*`, `six_track*` | **all passed** (suite completa) | `[HOST]` |
| `[AC791N]` build `--release 1.5.24` | imagen 582 340 B, 1 272 B libres, `.data+.bss` 86 912 B | `[ESTÁTICO]` |
| Emulador WASM v1.5.24 | reconstruido con la misma fuente: 1 285 187 B, build `ac18d6e`, sha256 `06aa6b17…` | `[HOST]` (wasm) |
| Paquete `release/felucca-v1.5.24.fwsc` | **610 933 B**, sha256 `6f1f66941b68f390c98495d0c71250f9c8d00920e6750cc2adeb8cbefe63d136`, build `ac18d6e` | `[ESTÁTICO]` |

## 8. Lo que quedó pendiente, y por qué

- **Los cuatro pendientes de la Fase 2 siguen abiertos**: son `[HW]` (reproducir el "7" del issue #107 en la
  unidad, medir el coste de 12 voces de BRASS con FX en el AC791N, verificar que el pico no gasta tiempo de
  ISR) y uno `[ESTÁTICO]`: `voice_window_grow` escribe `vidx[]` de otra parte mientras el core 1 puede estar
  renderizando esa parte; hoy se confía en que la asignación ocurre entre bloques. Un punto de sincronización
  explícito con `mc` es un cambio de concurrencia que no se puede validar sin la unidad ni con los tests de
  host: queda escrito en `docs/FASE2_ISSUES_106_107.md` §1 en lugar de cerrado a ciegas.
- **Presets nuevos**: el banco de fábrica mide el nivel de cada preset renderizándolo (`preset_level.c`, 238
  presets). Agregar sonidos que demuestren PLUK/RES/NOTCH/SHELF sin poder escucharlos y sin re-medir el banco
  es exactamente lo que este árbol no acepta; queda como el primer trabajo de la próxima pasada con la unidad.
- **Un parámetro nuevo por motor**: con 2 ids libres y 1 272 B de XIP, es una decisión de formato (proyectos,
  motion records, compatibilidad) que necesita la unidad delante, no este árbol.

## 9. Lo que no está medido

No hay medición de la FM-1 física en esta página. "Cómo suena" es una forma de onda en el host; el panel, el
DAC y el oído de la unidad no participaron. Las cuatro validaciones de `docs/FASE2_POLIFONIA_v1.5.23.md` §7
siguen siendo las mismas y ninguna se marca como hecha.
