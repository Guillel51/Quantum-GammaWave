# Fase de evolución musical v1.5.18 — herramientas para trabajar sobre el patrón

## Qué se incorporó, y por qué eso y no otra cosa

Antes de añadir nada se leyó lo que había. El instrumento ya trae: 16 motores (11 vivos), arpegiador por pista con
modos, rate, octavas, gate, swing, probabilidad, hold y orden; acordes y voicings desde la escala; progresiones;
tres cabezas de fuga; matriz de modulación de 8 ranuras con LFO, secuencia de modulación, macros; automatización por
paso con locks; punch-ins; ducker; slicer; buses de FX; y una tabla de parámetros en **125 de 127** ids.

Con eso, lo que falta no es otro parámetro más: es **mano sobre el patrón**. La página SEQ > TOOLS existía con seis
operadores (DUP, HALF, ROT ⟵, ROT ⟶, REV, CLEAR) y un preview de los 16 pasos. Se amplió con cuatro operadores
musicales que no duplican nada de lo anterior:

| operador | idea musical | cantidad |
|---|---|---|
| **EUCLID** | el ritmo de una frase, redistribuido sobre onsets regulares sin perder ninguna nota | HITS 2..16 |
| **MIRROR** | la inversión diatónica de la línea alrededor de la tónica de la pista | IN KEY |
| **THIN** | bajar densidad sin cambiar la forma | 1 OF 2..8 |
| **CRESC** | dar dirección a la frase: del primer onset al último, que llega a velocidad plena | RISE 12..96 |

Todos respetan la regla que la página ya imprimía: **el tiempo, la compuerta y la probabilidad de un paso no se
mueven**; sólo cambia lo que suena. Todos usan el UNDO existente y ninguno necesita id de parámetro, byte de `.bss`
ni cambio de formato de proyecto o preset.

## Dónde vive el código

- `firmware/src/seq.c` → `seq_tool_apply(track_t *t, uint32_t op, uint32_t amt)`: los diez operadores, puros sobre
  `t->step`. Está aquí, y no en la página, para que el test ejercite exactamente lo que giran los knobs.
- `firmware/src/ui_seqtools.c` → la página: preview de los 16 pasos, la cantidad en la unidad de cada operador
  (`HITS 6`, `IN KEY`, `1 OF 2`, `RISE 48`) y una línea con lo que va a hacer.
- `firmware/src/params.c` → la lista de operadores, ampliada al final (los números guardados de un proyecto siguen
  significando lo mismo).

## Medido (clase host x86-64, `tests/seq_tools_test.c`)

Frase de prueba: arpeggio 60 64 68 72 en los pasos 0/4/8/12, acorde 67+71 en el 6, golpe de línea en el 10.

| operador | resultado medido |
|---|---|
| EUCLID ×3 | onsets en **0 3 6 8 11 14**, las **6** notas intactas y en su orden, acento y golpe viajando con su nota |
| EUCLID ×8 sobre 8 pasos | recortado a la longitud: 8 onsets, **nada escrito más allá del paso 8** |
| MIRROR | 60 64 68 72 → **60 56 52 48**; el acorde 67/71 → 53/49; **6 de 6** notas dentro de la escala; dos MIRROR seguidos vuelven al original |
| THIN 1 de 2 | de 6 onsets a **3** (0, 6, 10), los que quedan en su sitio |
| CRESC ×4 | velocidad 79 → 95 → 103 → 111 → **127** (monótona; el último onset a plena) |
| los seis previos | DUP ×2, HALF, ROT ⟶ 4, REV, CLEAR, con longitud, compuertas y probabilidades comprobadas |

Lo que el test **no** dice: cómo suena. Eso pide la unidad.

## La biblioteca de presets, medida antes de tocarla

`tests/preset_levels` renderiza los **285** presets de fábrica; `preset_level.c` nivela cada uno al cargarse.

- Mediana de la biblioteca: **−18,5 dB RMS**. Medianas por motor: **−18,5 / −18,6** en los diez motores (la
  nivelación funciona; el reparto entre motores es plano).
- Excepciones de 10–11 dB por debajo, con el fader ya en su tope (127): `AURORA FM` (FM6, −29,5 RMS y −19,2 de
  pico: el único con la señal realmente floja), `STATIC HAZE` (−29,4) y `GEIGER CNT` (−28,7), dos texturas NOISE
  cuyos picos son normales (−4,5 / −3,5): están dispersas, no ahogadas.
- El generador de niveles ya documenta que un sonido más flojo que el tope del fader no se corrige con el número
  sino con el sonido, y eso exige oído en la unidad. **No se reescribieron presets a ciegas**: se midió, se dejó
  constancia con nombre y cifra, y no se añadió ningún preset DRUM-X.

## Coste (build AC791N)

- Imagen **580 280 → 581 160 B** (+880 B, **2 452 B libres** del slot), `.data+.bss` **92 832 B** sin cambios.
- Sin parámetros nuevos, sin estado nuevo, sin cambios de formato.

## Sin verificar

- Sonido de los operadores en la unidad, en el DAC y en el amplificador: no medido (no hay FM-1 aquí).
- La biblioteca de presets no se rediseñó, así que no se afirma nada nuevo sobre cómo suena.
