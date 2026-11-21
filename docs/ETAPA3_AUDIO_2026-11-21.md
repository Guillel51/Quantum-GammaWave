# Etapa 3/8 — motor de audio, DSP y CPU: lo medido, lo que no se pudo meter y por qué

Clases: **`[HOST]`** = x86-64 (`tests/eng_bench`, `tests/fx_bench`, `tests/audio_stress`, `gprof`);
**`[AC791N]`** = el linker/compilador del destino (`build/felucca.elf`, `build/felucca.bin`,
`tests/target_budget.py`, log de `./build.sh`). No hubo unidad física: nada de esto es `[HW]`.

## 1. Estado de partida

`main` en `5020f800` (Etapa 2). Árbol limpio. La Etapa 3 no repite lo ya hecho: las tablas del FM6 y de la
senoide siguen en su forma optimizada (probadas punto por punto), y el arnés del host ya inicializa las dos
curvas (bug de la Etapa 2, corregido allí).

## 2. Coste por motor `[HOST]` (8 voces, T1, mejor de 3)

`tests/eng_bench 1600`, µs por bloque de 32 muestras:

| motor | µs/bloque | sobre el silencio |
|---|---|---|
| WHEEL | 13.21 | +6.94 |
| WTABLE | 12.85 | +6.58 |
| NOISE | 12.32 | +6.05 |
| PHASE | 12.12 | +5.85 |
| VOICE | 11.66 | +5.39 |
| FM6 | 11.06 | +4.79 |
| ADDTV | 10.95 | +4.68 |
| DRUM | 9.37 | +3.10 |
| LOFI | 8.57 | +2.30 |
| ANALOG | 5.62 | −0.65 (más barato que el silencio: sin DIST ni segundas etapas) |
| (silencio, todo apagado) | 6.27 | — |

## 3. Dónde se va el bloque `[HOST]` (gprof, 487 680 bloques)

| función | % del bloque | notas |
|---|---|---|
| `fx_buses` | 21 % | coro + delay + ping-pong + pre-delay |
| `mix_part` (6 partes) | 13 % | nivel/pan/sends por muestra |
| `rev_room` | 12 % | la reverb |
| `mix_block` | 9 % | los lazos de mezcla y el zeroing |
| `track_render` | 8 % | el motor y su envolvente |
| `fm6_render` | 8 % | 4 operadores |
| `master_out` | 6 % | por muestra |
| `events_block` | 6 % | secuenciador/MIDI |
| `drum_render` | 4 % | el kit |

**Y el dato que ordena la etapa: con la caja encendida y sin una sola nota, el bloque sigue costando 78 ns por
muestra contra 128 ns con todo sonando — el 61 %.** El perfil de ese estado es el mismo: `fx_buses` 31 %,
`rev_room` 21 %, `master_out` 19 %, `mix_block` 13 %. La FM-1 paga los buses en silencio.

## 4. Lo que se intentó y lo que costó

### 4.1 Saltar los buses enteros cuando no suenan — **rechazado, con número**

Condición propuesta: los envíos en cero y la salida exactamente cero durante una línea de delay completa
(65 536 muestras). Resultado medido con `tests/audio_stress` y una prueba de dos procesos que compara **cada
muestra** con el salto encendido y apagado:

- Con la condición leída **después** de la compuerta (`fx_gate`), la prueba mostró **0.26 de cola rancia**
  liberada en el primer acorde tras la pausa (2 179 840 LSB): la compuerta cierra sobre el silencio y esconde
  la cola detrás de su ganancia. La condición era falsa.
- Leída **antes** de la compuerta, la salida nunca llega a cero exacto: medida la caída, el residuo de los buses
  se asienta en **40–42 LSB para siempre** (un ciclo límite de redondeo del bloqueador de continua de la
  reverb, −106 dBFS). No hay instante en que la condición sea verdadera, así que el salto nunca se activaría.
- Con un piso de 64 LSB la prueba volvió a fallar: la reverb guarda energía en sus líneas que su salida no
  muestra, y al reanudar la libera (0.26 otra vez). **Conclusión: los buses con realimentación no se pueden
  saltar con una condición de salida.**

### 4.2 Saltar sólo el coro cuando su línea está vacía — **exacto, pero no entra en el slot**

El coro es una línea de 2 048 muestras **sin realimentación propia** (el flanger y el phaser son la excepción y
quedan fuera del salto): tras una línea entera de ceros, los tres taps leen cero, el lado también, y el salto no
puede mover **ni un LSB**. La prueba de dos procesos lo confirmó: **24 046 bloques saltados, diferencia máxima
0.000e+00 en 25 510 bloques** (~20 s de audio), con la reanudación exacta.

Y sin embargo **no se puede publicar**: `[AC791N]` imagen **579 876 → 583 048 B**, es decir **+3 172 B de un slot
que tiene 3 736 B libres** (0.6 %). El firmador del destino reventó el presupuesto: el compilador duplica el lazo
del bloque al añadir una rama por bloque dentro de una función `always_inline` (se probó también con `cho_ens`
`noinline` y con la rama fuera del lazo: mismo coste). Dejar 564 B libres es cruzar el piso que la Etapa 2 puso
en 2 048 B, así que el cambio se revirtió y la medición queda como evidencia para la etapa que libere XIP.

## 5. Lo que sí se entregó

- **`tests/audio_stress.c` (nuevo)**: seis motores, seis partes, dieciséis teclas, todos los buses alimentados,
  MIDI entrando mientras suena (notas, apagados, CC), cambios de motor bajo las notas y retriggers; mide pico,
  DC, el salto máximo entre muestras (detector de clicks), NaN/Inf y el tiempo por bloque (p50/p99/máx) contra
  los 725 µs. Resultado de la corrida de 8 s: pico **0.300**, DC **−0.0001**, salto máximo **0.151** (0 pasos
  sobre 0.5), **0** muestras no finitas, **0** frames por encima de escala completa. Pasa.
- **`tests/fx_bench` escena 3 (nueva)**: «idle: los buses armados, sin nada sonando», que es lo que hizo visible
  el 61 %.
- El perfil, la tabla por motor y los dos rechazos quedan medidos y reproducibles con los comandos de cada test.

## 6. Presupuesto del destino tras la etapa `[AC791N]`

| | valor | piso |
|---|---|---|
| imagen / slot | 579 876 / 583 612 B (**3 736 libres**) | 2 048 |
| `.data+.bss` | 93 612 / 98 304 B (4 692 libres) | 4 096 |
| pool | 9 248 B de spare | 8 192 |
| `.lowbuf` | 2 204 B libres | 1 024 |
| `.noinit` | 124 B libres | 64 |

`tests/target_budget.py`: **ningún lazo por encima del +10 %**, `mix_block` en +2 %.

## 7. Pendientes reales para la Etapa 4

1. **La pared es XIP, no el DSP.** Cualquier optimización que añada una rama por bloque dentro de un lazo
   `always_inline` cuesta kilobytes (medido: 3 172 B por una rama). Antes de optimizar CPU hay que liberar
   slot o mover esos lazos a `.hot_text`/RAMTEXT con criterio.
2. **El salto del coro está escrito, probado y medido** (exacto, 24 046 bloques): entra en cuanto haya ~3.5 KB
   de slot libres.
3. **Los buses en silencio** (61 % del bloque): hace falta una condición que mire el estado interno de la
   reverb (no su salida) — por ejemplo, un contador que la propia `rev_room` lleve de su energía por línea.
4. **`master_out` (19 % en silencio, por muestra)**: candidato a comprobación de silencio por bloque con
   continuidad de la compuerta, sin tocar el lazo.
