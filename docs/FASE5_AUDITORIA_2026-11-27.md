# Fase 5 — auditoría de los diez motores, medida

Fecha: 2026-11-27. Clase de las cifras: **host x86** (`tests/param_sweep_test.c`, `tests/preset_audit_test.c`,
`tests/regress`) y **estáticas del AC791N** (`tests/target_budget.py` sobre `build/felucca.dis`). No hay FM-1
físico: ninguna cifra dice nada sobre los ciclos de la unidad.

## 1. Método: se mide el render, no el texto

Un knob que está en la tabla y no en el DSP se ve idéntico a uno que funciona, y el `grep` miente: los motores
leen `e[k]` sobre un puntero a `P_E0`, no `P_Ek`. El primer barrido por texto marcó dos knobs de FM6 como
muertos y era falso (FM6 los suma en `fm6_eff[tr].fb = clamp(p[FP_FB] + e[1], 0, 7)`).

Por eso la auditoría es un test nuevo, `tests/param_sweep_test.c`: carga el primer preset del motor, sostiene
una nota, y **fuerza cada parámetro a su mínimo y a su máximo** comparando el audio contra la misma nota sin
tocar. La medida es la RMS relativa de la diferencia sobre el tercio final (pasado el ataque): 0 % = el
parámetro no cambia una sola muestra.

```
PARAMS: 80 knobs de motor (10 motores × 8)      -> 0 DEAD
PARAMS: 118 parámetros de pista (sobre ANALOG)  -> 0 DEAD
```

**Conclusión medida: no hay parámetros muertos que "hacer vivir".** La expansión sonora de esta fase tiene
que ser control nuevo, no cablear fantasmas. El test queda en `tests/run_tests.sh` como puerta: si un knob
deja de mover el audio, falla.

## 2. Opciones reales por motor (80 knobs + página CHARACTER)

| motor | presets | EDIT | CHARACTER | lo más estrecho |
|---|---|---|---|---|
| ANALOG | 73 | WAVE DETUNE MIX NOISE CUT RES DRIVE INT | 2 | dos motores en un solo `MIX` |
| PHASE | 34 | WAVE WAVE2 DCW ENV DETUNE LINE SUB FB | 2 | sin bloqueo de DC (ver §5) |
| LOFI | 29 | CHIP WAVE DUTY CRUSH CUT VIB BEND MASK | 2 | `CRUSH` y `MASK` se solapan |
| VOICE | 25 | VOWEL VOWEL2 TALK SHIFT BUZZ BREATH Q RAND | 3 | — |
| WHEEL | 22 | REG SUB BODY TOP PERC CLICK DRIVE ROTOR | 3 | — |
| DRUM | 18 | MORPH TUNE TONE … | 0 | sin página CHARACTER (edita en sus páginas) |
| NOISE | 22 | … FREQ PEAK TRK DENSE DRIFT CRUSH | 3 | — |
| FM6 | 37 | ALG FB MOD RATIO MEG VMOD DETUNE SLOT | 2 | `FB` global + feedback por operador |
| WTABLE | 26 | MORPH DETUNE UNI SUB CUT RES DRIVE HARM | 3 | — |
| ADDTV | 12 | SPECTR TILT DECAY STRETCH CUT RES DRIVE SUB | 2 | 8 parciales fijos: sin niveles por parcial |

## 3. Inventario: 296 y 298 eran los dos correctos

`tests/preset_library_test.py` cuenta **296 navegables** (ANALOG 73, PHASE 34, LOFI 29, FORMANT 25, WHEEL 22,
DRUM 16, NOISE 22, FM6 37, WTABLE 26, ADDTV 12). `tests/preset_audit_test` cuenta **298 entradas**: las mismas
296 más **los dos kits legacy de DRUM** (HEARTH KIT, CYBER DRUM) que siguen en la tabla en los índices 10 y 11
para que un proyecto anterior cargue exactamente el sonido que guardó. No es una discrepancia: son dos
conjuntos distintos y los dos están bien.

**El test valida de verdad** (probado mutando el dato, no leyéndolo): con `TOTAL = 297` imprime
`FAIL: library: 296 sounds, expected 297` y **sale con código 1**; restaurado, sale 0. Ojo con el `$?` después
de una tubería: el primer intento pareció salir 0 porque medía el `tail`.

## 4. Los cinco presets descentrados de DC: un defecto real, arreglado en el motor

La auditoría de biblioteca encontró cinco sonidos con el centro desplazado más del 2 %: PHASE **SYNC CATHEDRAL
3,7 %** (las dos mitades negativas: DC de verdad), ETHNIC PLUCK 2,2 %, PD 808 2,1 %, LOFI BYTEBEAT 2,2 %,
BYTE VAR 2,6 %. ADDITIVE, WAVETABLE, NOISE y FM6 terminan sus voces en un bloqueo de DC; **PHASE no tenía
ninguno**, y un ciclo de trabajo de pulso nunca es simétrico en media onda.

Arreglo (motor, no nivel): PHASE termina ahora en el mismo bloqueo de DC (~5 Hz, `dc_pole`, redondeado como
WAVETABLE y ADDITIVE), con sus dos muestras en `v->s[6]`/`v->s[7]`.

| | antes | después |
|---|---|---|
| DC máximo, PHASE | 3,7 % | **1,35 %** |
| DC máximo, LOFI | 2,6 % | **1,48 %** |
| presets por encima del 2 % | 5 | **0** |
| `phase_render` / `_noise` (coste AC791N) | 94 / 113 | 99 / 119 (+5 %, banda +10 %) |
| imagen / XIP | 581 572 / 456 540 B | 581 668 / 456 636 B (+96) |

**Dorados: 34 renders cambiados y aceptados**, exactamente los 34 presets de PHASE (`GOLDEN_UPDATE=1`), y
después 316/0. Es un cambio de sonido deliberado: el DC no era un timbre, era headroom robado y un golpe al
soltar.

## 5. Similitud: HARP PLUCK contra SLOW TIDE, y el resto

Huellas de `preset_audit_test --fingerprint` (10 bandas, RMS, pico, ataque, caída):

- HARP PLUCK: ANALOG 48, RMS 2 832, pico 7 508, ataque 2,2 ms, caída 4,4 ms.
- SLOW TIDE: ANALOG 62, RMS 2 512, pico 5 251, ataque 2,9 ms, caída 5,8 ms.
- Coseno espectral: **0,9896** — parecidos, no gemelos.

En toda la biblioteca, **127 de 5 567** pares del mismo motor están por encima de 0,995. Los más cercanos:
GLASS KEYS / EMBER TINES (FM6) 0,9997, SQR BASS / 808 BOOM 0,9995, VELVET EP / AURORA FM 0,9995, PLUCK / FM
PIANO 0,9991. Queda como lista de trabajo para diferenciarlos; no se tocaron en esta ronda.

## 6. Lo que NO está hecho en esta ronda (para no venderlo como hecho)

- **No hay knobs nuevos todavía.** La auditoría dice dónde tiene sentido ponerlos (ADDTV sin niveles por
  parcial, ANALOG con dos motores en un `MIX`, LOFI con `CRUSH`/`MASK` solapados), pero añadir un parámetro
  toca enum + UI + proyecto + Web Studio + emulador: es el siguiente paso, no este.
- Los 12 presets fuera del fader no necesitan más volumen: medidos, **ninguno es silencioso ni caliente** (0
  silent, 0 hot de 298) y ninguno repite nombre. Su problema, cuando lo hay, es de estructura.
- Los 127 pares ≥ 0,995 están medidos y listados, sin corregir.
