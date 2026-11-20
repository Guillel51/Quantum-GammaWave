# Fase de audio v1.5.17 — seis pistas y doce voces, juntas

Esta fase no cambia el sonido del instrumento: arregla **una** decisión del asignador que cortaba colas de release sin
necesidad y **demuestra con medidas** lo que antes sólo estaba afirmado, que las seis pistas y las doce voces del pool
pueden sonar a la vez — concentradas, repartidas y saturadas — sin robos prematuros ni notas perdidas.

Clase de cada número de este documento: **host x86-64** salvo donde diga **build AC791N** (tamaño de imagen y RAM) o
**estático pi32v2** (presupuesto de CPU por función, `tests/target_budget.py`). No hay FM-1 física en este entorno: no
hay ninguna medida acústica, de DAC, de amplificador ni de latencia audible, y no se afirma ninguna.

---

## 1. Estado de partida (auditoría, no suposición)

Leído en el código, no inferido:

- `NVOICE 12` por defecto y `voice_ucap` 8 / 10 / 12 (12 de fábrica). El pool físico es `VPOOLN = NVOICE + NPART = 18`
  slots con `VDEAD = 17` utilizables.
- Reparto inicial: `share = VDEAD / NPART = 2` slots por pista (12 asignados, 5 sin dueño).
- `voice_window_grow()` presta un slot ocioso de otra pista (el donante conserva al menos uno) o reclama uno sin
  dueño; `voice_grow_to()` sube la ventana de una pista hasta lo que haya libre.
- `voice_victim()` clasifica: release primero, después las voces extra de UNISON, después las sostenidas que no son
  el bajo de la pista; dentro de cada clase, la de menos energía; y el reparto justo (`vshare()`) evita que una pista
  se coma el pool.
- `voice_defer()` (VQ_N 8) ya no descarta notas: las aplica en el bloque siguiente.
- `voice_stats()` (sounding / release / fading / peak / steals / denied) es el monitor; el pico se toma **una vez por
  trama**, y leerlo sin llamarlo devuelve el valor anterior.

## 2. El defecto: el gobernador de carga cortaba colas con el presupuesto vacío

`audio.c` mide el render de media trama y pone `load_tight` por encima del **65 %** de esa media trama; lo quita por
debajo del 60 % tras 16 medias tramas. La protección que de verdad actúa es otra, en el mismo archivo: dos medias
tramas seguidas por encima del **80 %** y se desvanece una voz (`SHED_TICKS`).

Con `load_tight` puesto, `voice_room()` y `voice_alloc()` ejecutaban `voice_kill()` sobre una cola en release **aunque
`voices_busy() < voice_cap()`**. La intención era que el número de voces no subiera por encima del 65 % de carga. El
problema es que una cola en release **renderiza las mismas muestras que la nota que la sustituye**: el corte no
ahorraba CPU, sólo impedía llegar al tope que el usuario eligió, y a cambio truncaba una envolvente.

Corrección (ambos sitios, `firmware/src/voice.c`): con hueco en el presupuesto se usa el slot libre; el reciclado de
una cola queda para cuando el presupuesto está realmente lleno, que es donde ya estaba y donde tiene sentido. El
guardián del 80 % no se toca.

Evidencia (`tests/poly_six_test.c`, escenario E, cuatro colas sonando, gobernador puesto):

| | release antes → después | robos |
|---|---|---|
| antes del fix | 4 → **3** | 0 → **1** |
| después del fix | 4 → **4** | 0 → **0** |

## 3. Las medidas del cuadro completo

`tests/poly_six_test.c`, compilado contra las fuentes reales (`hostsim.c` incluye `voice.c`, los motores, `fx.c`,
`seq.c`). Cada cifra sale de contar en el pool, no de una impresión:

| escenario | petición | medido |
|---|---|---|
| A · seis pistas, 2 notas cada una | 12 sonando | gated **12**, busy **12**, pico **12**, robos **0**, denegadas **0**; 2 voces propias por pista; 8 bloques después siguen 12 |
| B · las doce en una pista | 12 sonando | **12**; ventana de la pista 12, pool 17; ninguna voz que suena tocada; al soltar, otra pista recupera voz |
| C · 4+4+2+1+1 | 12 repartidas | **4/4/2/1/1/0**, robos **0**, denegadas **0** |
| D · seis sueltan | 6 + 6 | gated **6**, release **6**, pico **12**, robos **0** |
| D2 · una nota más al tope | reciclar una cola | gated 11→**12**, release 1→**0**, **1 robo sobre la cola**, ninguna sostenida tocada |
| E · gobernador con hueco | no cortar | release 4→**4**, robos **0** |
| F · doce con chorus+delay+reverb | 12 sonando | **12**, robos **0**, pico de bloque 15 679 (< riel) |
| G · cambio de preset con el acorde sostenido | no perder voces | gated **12**, robos **0**, pico de bloque 13 325 |
| H · TRACKS 4 (fábrica) | T5/T6 duermen | T5 **0**, T6 **0**, `asleep_hit` **1**, robos **0** |
| S · saturación (+2 notas) | suenan, no se deniegan | gated 12, robos **+2**, denegadas **0**, pico **12** |
| I · coste (reloj host) | plazo 725 µs/bloque | 4 pistas/8 voces **6,9 µs**; 6 pistas/12 voces **8,9 µs** (+29 %), mejor 8,4 µs |

La distinción que el encargo pedía no se diluye en ninguna línea del test: **pedidas** ≠ **asignadas** ≠ **activas con
compuerta** ≠ **en release** ≠ **robadas** ≠ **pico**.

## 4. Lo que no se cambió, y por qué

- **`trk_live` sigue en 4 de fábrica.** El modo **TRACKS 6** existe (MENÚ > TRACKS), está probado aquí y funciona; subir
  el valor de fábrica exige una medida en la unidad que no se puede hacer en este entorno. Se documenta en vez de
  afirmarse.
- **`FM6_POLY 4`**: llevarlo a 6 cuesta ~3 552 B de `.bss` (6 pistas × 2 notas × ~296 B) y sólo hay 3 268 B libres de
  imagen además del suelo de 4 KiB de RAM.
- **`eng_formant.c` `.poly = 4`**: su render es el más caro por voz del presupuesto estático (238).
- **Gobernador del 80 % (`SHED_TICKS`)**: intacto. Es la protección que sí hace falta.

Los topes por motor son topes de CPU/RAM declarados; el pool reparte el resto entre las pistas que los piden.

## 5. Reproducir

```sh
cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/poly_six tests/poly_six_test.c -lm && ./build/host/poly_six
sh tests/run_tests.sh          # la suite entera, con POLY SIX cableado
```

## 6. Sin verificar, y se dice

- **Pop de encendido, clicks, latencia audible, polifonía oída**: no medidos. Requieren FM-1 física, panel, DAC y
  amplificador. Este cambio no afirma nada sobre el pop físico.
- **Coste en la unidad**: los µs del escenario I son del host x86-64. No se extrapolan. El presupuesto estático
  pi32v2 (`tests/target_budget.py`) sigue siendo la referencia de CPU del destino, y no ha cambiado.
- **RAM/flash**: build AC791N, cifras en el CHANGELOG; ninguna de estas rutas crece el binario.
