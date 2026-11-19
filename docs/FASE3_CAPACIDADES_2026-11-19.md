# FASE 3 — Capacidades musicales: qué hay, qué cabe y qué no

*19 de noviembre de 2026. Estado: `v1.5.2` (`28a07f6`).*

Esta fase pedía, antes de nada, estudiar lo que ya existe para no duplicarlo, y después agregar capacidad
musical real. El estudio está abajo, con los números del árbol. La conclusión, medida y no supuesta, es que
**tres de los cuatro caminos pedidos están cerrados por presupuesto**, y el cuarto —presets nuevos— no se
puede hacer sin oído. Preferí no entregar nada a entregar parámetros sin efecto, motores a medio integrar o
presets que nadie escuchó.

## 1. Lo que ya existe (no hay que agregarlo)

| Área | Estado real en `v1.5.2` | Dónde |
|---|---|---|
| Motores | 11: ANALOG, PHASE, FM6, WAVETABLE, WHEEL, DIGITAL, LOFI, ADDITIVE, FORMANT, NOISE, DRUM-X | `firmware/src/eng_*.c` |
| Parámetros por pista | **125 de los 127 ids** que graban motion records, proyectos y presets de usuario | `core.h` `P_COUNT` |
| Matriz de modulación | 4 slots × (SRC, DST, AMT), más LFO con `LD_FLT`/`LD_AMP`/`LD_PITCH` por pista | `mod.c`, `core.h` `P_M1SRC..` |
| LFO | formas múltiples seleccionables por `P_LWAVE`, rate y profundidad por destino | `eng_*.c`, `mod.c` |
| ARP / SEQ | arpegiador por pista y secuenciador con patrones (`PATTERNS`, `pat_load`) | `seq.c`, `ui.c` |
| Escalas / acordes | `SCL` con escalas y acordes por pista, chord memory | `scale.c` |
| Macros de performance | cuatro perillas (`perf_k[0..3]`) con latch y punch-in (`pb`) | `perform.c` |
| GLO MIXER | niveles, mutes, envíos FX por pista, en una pantalla | `ui.c` |
| FX | DIST, CHORUS, DELAY, REVERB con buses compartidos y flush por preset | `fx.c` |
| Banco de fábrica | **238 presets** medidos uno por uno, con nivel propio | `preset_level.c` |

Bancos medidos: ANALOG 68, PHASE 32, LOFI 29, VOICE 25, WHEEL 22, NOISE 22, WTABLE 22, DRUM 18.

## 2. Los tres techos, con números

**(a) No entran parámetros nuevos.** `P_COUNT` vale 125 y el espacio de ids es de 7 bits (127): los motion
records, el formato de proyecto y los presets de usuario llevan el id en 7 bits. Quedan 2 ids y el comentario
de `core.h` los reserva. Un parámetro más no es «una línea en una tabla»: es cambiar el formato de proyecto,
los records de motion y la compatibilidad de todo lo guardado. No es una decisión de esta fase.

**(b) No entra un motor nuevo.** La ranura de aplicación tiene **1 508 B libres** de 583 612. El motor más
barato del árbol (NOISE) ocupa varios KB de código, más su tabla de parámetros, su rama de edición y sus
presets. Un motor nuevo tampoco es un motor: hay que integrarlo en edición, modulación, MIDI, SEQ, FX, presets
y niveles medidos, y eso pide liberar XIP primero.

**(c) No entran presets nuevos sin medirlos.** El nivel de cada preset no es un número de diseño: sale de
renderizar el sonido y llevarlo a la mediana del banco (`tools/gen_preset_levels.py` → `preset_level.c`, 238
presets medidos). Agregar presets obliga a re-medir el banco, y escribirlos sin poder escucharlos es
exactamente lo contrario de la regla del repo: nada de presets redundantes ni de cambios que aplasten la
variedad. Eso necesita la unidad, no este árbol.

## 3. Qué se hizo en esta fase

Nada de código. El trabajo de la fase fue el estudio de arriba y el descarte razonado. Lo que sí quedó
cerrado en las fases anteriores, y sigue en pie: el corte de audio al cambiar preset (#106 y `perf_reset`,
`v1.5.2`), el monitor de polifonía, el hold de audio en el arranque y el gate V16.

## 4. El camino de desbloqueo, en orden de coste

1. **Liberar XIP.** Es la llave de (b) y de la mitad de (a). Los tres candidatos ya medidos están en
   `docs/FASE4_OPTIMIZACION.md`; el primero es el banco de formas del wavetable (`WT_GEN`, 12 288 B de
   `.bss` → flash si se comprime por simetría), y hay 1 508 B de holgura para el primer paso.
2. **Un id de parámetro, sólo si se cambia el formato de una vez.** Con 2 libres y un bump de formato
   documentado se puede sumar un destino de modulación, pero conviene hacerlo junto con el motor nuevo, no
   antes.
3. **Presets nuevos con la unidad en la mano.** Cuatro a seis por motor con más espacio tímbrico, medidos con
   `tests/preset_levels` y regenerando `preset_level.c`; sin oído, no.

Mientras las tres cosas no estén, todo lo que se agregue es decorativo, y este árbol no acepta eso.
