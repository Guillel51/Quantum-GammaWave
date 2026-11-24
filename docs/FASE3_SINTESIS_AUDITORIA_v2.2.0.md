# Fase 3 — Expansión sonora: auditoría, un prototipo medido y retirado, y el techo que hay que decidir (v2.2.0)

Clases de medida como en el resto de `docs/`: `[HOST]` x86-64 con las fuentes reales, `[TARGET]` binario
compilado con el toolchain `pi32v2` fijado, `[ESTÁTICO]` sobre `.map`/`.elf`. **No hay unidad FM-1 en este
entorno**: nada de este documento afirma cómo suena en el instrumento.

## 0. Veredicto

La fase pedía una lista larga de capacidades de síntesis nuevas. La auditoría del árbol (v2.2.0, `fe61bbd`)
encuentra que **casi todo lo pedido ya está implementado**, repartido entre las fases anteriores del proyecto,
y que lo que la fase pide de verdad es **profundidad**, no superficie.

Y encuentra un techo duro que decide la fase:

| Recurso | v2.2.0 | Consecuencia |
|---|---|---|
| Ids de parámetro (`P_COUNT`) | **127 de 127** (`core.h:245`) | **no cabe ni un parámetro nuevo**: ni un knob, ni un destino de modulación nuevo como parámetro propio |
| XIP de la app `[TARGET]` | **580 180 B de 583 612** (3 432 libres) | entran decenas de bytes por opción; no una tabla ni un motor |
| `.data+.bss` `[TARGET]` | 92 848 B de 98 304 (5 456 libres) | la RAM no es el límite de esta fase |
| CPU `[HOST]` | lazo 1 628 ns, p99 de audio 190 µs de 725 | hay margen de CPU moderado, no de sobra |

Por eso lo que se añadió en fases anteriores de esta misma naturaleza fueron **valores nuevos dentro de
parámetros existentes** (PLUK, TRAP, STEP en `P_LWAVE`; NOTCH, DIP en `P_FTYPE`): un valor no cambia el
formato de nada y cuesta decenas de bytes. Esta fase se evaluó con esa misma vara.

## 1. Mapa de la petición: dónde vive ya cada cosa

| Lo pedido | Estado en v2.2.0 | Dónde |
|---|---|---|
| Nuevas formas de onda | **11** en ANALOG: SAW SQR TRI SIN PWM SYNC RING SAW3 TUBE FOLD PM | `eng_analog.c:11` (`N_ANALOG_WAVE`) |
| Wavefolding | FOLD en ANALOG (sin(g·sin x), g 1..8 rad), FOLD como DIST, FOLD en DRUM, FOLD como CHARACTER de FM6 | `eng_analog.c:68-82`; `params.c:52` (`N_DTYPE`); `eng_drum.c:571`; `eng_fm6.c:460` |
| Distorsión armónica | TUBE (ANALOG), DRIVE + softclip, DIST con 6 modelos, DRV por golpe en DRUM | `eng_analog.c:68`; `fx.c` |
| Modulación de fase | PM en ANALOG (osc 2 curva la fase de osc 1, índice por MORPH+SHAPE, ratio por INT/DTN) y motor PD entero con 9 formas | `eng_analog.c` (AW_PM); `eng_phase.c:13` (`N_PD_WAVE`) |
| Nuevas opciones FM / feedback / operadores | FM6 con 6 operadores, **32 algoritmos**, feedback por operador 0..7, FM ENV, transpose, detune | `eng_fm6.c:13` (`FP_FB`), `:134`, `:329` |
| Aliasing y calidad de osciladores | **polyBLEP** en sierra y pulso (`blep` con la pendiente de la muestra), triángulo y seno sin alias por construcción; filtro SVF de 8 tipos y las 4 variantes de pendiente | `dsp.c:61,78,83`; `eng_analog.c` (`svf_mode`) |
| Modos de LFO, sync, fade, fase, retrigger | **12 formas** (SIN TRI SAW SQR S&H RAMP EVO HSIN PULSE PLUK TRAP STEP), sincronía a negra/compás hasta 1/32, PHASE con FREE, FADE, CURVE (LSHP), SLEW, INVERT | `params.c:4`, `:110`, `:111`, `:181`; `voice.c` (`track_lfo_tick`) |
| Destinos y curvas de modulación | **31 destinos** (PITCH CUT SHP AMP PAN DIST CHO DLY REV RATE VIB E1..E8 CLIP PNCH X1..X4 RES ATK DEC REL HARM LSHP) y **17 fuentes** (LFO ENV VEL KEY RAND MODW AT EXPR M1..M4 STEP NOISE PAT BAR) | `params.c:80-83`; `mod.c` |
| Motores | 11: Analog, Digital, FM6, Phase, Formant, Lo-Fi, Noise, Drum, Additive, Wavetable, Wheel (modelado físico) | `engines.c`, `firmware/src/eng_*.c` |
| Motores híbridos | el MORPH de ANALOG ya interpola entre las 11 formas; el WHEEL y el ADDITIVE son híbridos por construcción | `eng_analog.c` (`analog_morph`) |

Ninguna de las filas de esa tabla admite "añadir otra vez" sin duplicar: **el árbol ya tiene la superficie
que la fase describe.**

## 2. El prototipo: `CHAOS`, una 13.ª forma de LFO — medido y **retirado**

Lo único que la lista pedía y no estaba era una fuente de modulación con **memoria** (S&H tira los dados
cada ciclo; EVO interpola entre puntos nuevos). Se prototipó una forma nueva dentro de `P_LWAVE`, que es la
clase de cambio que este proyecto admite sin tocar formato:

- **Diseño**: mapa logístico `x' = 3.94·x·(1−x)` sobre el campo de estado que S&H ya usa (`t->lfo_rnd`),
  un paso por ciclo de LFO (dos multiplicaciones), más una variante con inercia (recorrer 1/4 del camino).
- **Coste `[TARGET]`**: **80 B** de imagen (580 180 → 580 260), 0 B de RAM (el estado ya existía).
- **Prueba**: banco host que reproduce el mismo sawnote con `LFO → CUT` al mismo rate y profundidad, dos
  segundos, y mide el brillo por bloque (RMS de la primera diferencia, un proxy de la pendiente espectral):

  | Medida `[HOST]` | S&H | CHAOS | CHAOS con inercia |
  |---|---:|---:|---:|
  | brillo medio | 17 | 18 | 18 |
  | desviación del brillo | 15 | 18 | 18 |
  | salto bloque a bloque | 9 | 9 | 9 |

  **No hay diferencia audible que sostener**: la modulación es igual de rápida y de rango parecido, y con un
  filtro de 12 dB el oído no distingue la estructura temporal. Con inercia, además, la caminata se quedaba en
  una banda estrecha (29 776..48 901 de 65 536) — peor.

Se retiró el código (`git checkout` de `voice.c`, `params.c`, `tests/lfo_test.c`): el árbol queda en `fe61bbd`
sin cambios. **No se publica versión**: una versión con una onda que no cambia el sonido sería exactamente lo
que la fase prohíbe.

## 3. Lo que de verdad falta, y lo que cuesta

Ordenado por relación sonido/precio, con el techo de la sección 0:

1. **Desbloquear ids de parámetro** (bloqueante de todo lo demás). `P_COUNT` está en 127 porque los registros
   de motion guardan el id en 7 bits. Alternativas medidas:
   - ensanchar el id a 8 bits en los motion records: rompe los proyectos guardados (formato) — descartado
     por la política del proyecto;
   - **reutilizar un id existente** como "selector de motor" y mover un control poco usado: 0 B de formato,
     pero hay que elegir qué se pierde (decisión de producto, no técnica);
   - **añadir profundidad sin id**: nuevos *valores* dentro de parámetros existentes. Es lo que se hizo en
     v1.5.24 y lo que se intentó aquí; el límite es que un valor debe sonar distinto por sí mismo.
2. **FM de 4 operadores con realimentación propia** (híbrido FM + analógico): necesita al menos un id
   (profundidad de realimentación) → bloqueado por 1. Coste estimado `[HOST]`: **~1,5 KB** de imagen y
   ~8 % de CPU por voz.
3. **Supersaw de 7 sierras** con fase aleatoria: cabe en una *forma nueva* de `P_E0` (no necesita id), coste
   **~300 B** de imagen y **×7** el trabajo de oscilador del ANALOG (el motor ya gasta 1 628 ns de lazo en
   host; ×2,5 en el peor caso lo pone contra el presupuesto de CPU).
4. **Oscilador de tabla con interpolación de 2º orden** (mejora de calidad audible solo en notas agudas):
   **~250 B** y ~15 % más de coste en el seno.

Ninguna de las cuatro se hizo ahora porque las tres últimas dependen de la decisión de la 1, y porque
publicarlas sin medirlas contra el presupuesto sería repetir el error que esta fase acaba de evitar.

## 4. Lo que se comprobó en esta fase

- `./build.sh` en `fe61bbd`: imagen **580 180 / 583 612 B** (3 432 libres), `.data+.bss` 92 848 (5 456 libres),
  `tests/resources_test.py` 13 checks 0 FAILED.
- `tests/lfo_test.c` `[HOST]`: 0 fallos con las 12 formas actuales (el barrido recorre `TP[P_LWAVE].max`).
- El prototipo se compiló y se midió con un banco ad hoc (`build/chaos_probe.c`, no forma parte de la suite);
  sus números son los de la tabla de la sección 2.
- **No** se tocaron presets, motores ni formato: no hay nada que actualizar si no hay función nueva.
