# QUANTUM MORPH (v1.8.5): estados sonoros, dos extremos y sus controladores

Fecha: 2026-11-23. Versión: **v1.8.5**.

## Qué había ya (y no se duplicó)

`LFO > SCENES` existe desde la fase 6: cuatro escenas del sonido de la pista seleccionada (`msc_p`, `msc_mx`), una
pareja A/B, una posición 0..127 que interpola **todos los ids que el sistema de movimiento clasifica como sonido**
(`motion_param`), y un slew de 3/127 por cuadro que impide que un salto llegue a los motores. Lo que no toca, a
propósito: el motor (un cambio de motor es un evento discreto), el routing, el transporte y el asignador de voces.

La auditoría del proyecto (`docs/AUDIT_UI_2026-11-11.md`, `docs/AUDIT_PHASE1_v1.12.13.md`) tenía dos huecos
abiertos: **estados A/B persistentes** ("el formato de preset no tiene dos estados") y **control desde MIDI**. Esta
versión cierra los dos sin tocar el formato de proyecto, que ya está lleno hasta el último byte de su objeto de
flash (3 840 B).

## Lo nuevo

### 1. Un extremo de otra pista

KNOB 4 de la página pasa de cuatro valores a dieciséis:

| valor | qué hace |
|---|---|
| `--` | nada |
| `STORE A` / `STORE B` | captura el buffer de edición de la pista seleccionada en la escena A / B |
| `DIFF` | reservado |
| `A < T1` … `A < T6` | captura **el sonido de la pista 1..6** en la escena A |
| `B < T1` … `B < T6` | lo mismo en la escena B |

`msc_capture_trk(slot, t)` copia `t->p[]` entero y los cuatro slots de matriz `t->mx[]`; `msc_capture()` es la
misma función con `TSEL`. La copia es lo que hace que la pareja sea **persistente sin almacenamiento nuevo**: el
proyecto ya guarda las seis pistas, así que un morph "mi sonido ↔ el sonido del track 4" se recupera entero al
cargar el proyecto, y editar el track 4 después no cambia la escena.

### 2. Controladores

| CC | valor | efecto |
|---|---|---|
| 78 | 0..127 | la posición entre A y B, por el mismo slew de la perilla (≤ 3/127 por cuadro) |
| 79 | ≥ 64 | `STORE A` de la pista del canal |
| 80 | ≥ 64 | `STORE B` de la pista del canal |

La posición es una sola: la mueven la perilla y cualquier canal, y el audio nunca ve un salto. Los CC 78..80 no
estaban en el mapa (`MIDI_CC_MAP`) ni en el `switch` de controladores, así que ningún dispositivo que hoy mande esos
números cambia de comportamiento salvo por esta función nueva.

### 3. La página

`LFO > SCENES` dibuja ahora el morfo:

- el raíl de A a B con la parte recorrida rellena (color de tema, o de calor en las paletas con chaflán);
- el marcador de la posición, y el porcentaje en la esquina;
- un chip por extremo con el número de escena (tema para A, acento para B; gris si ese extremo no está capturado);
- una fila por extremo: `A SCENE 2 STORED` / `B SCENE 3 KNOB 4: STORE`;
- un pie que dice qué hacen los CC, o `STORE BOTH ENDS TO MORPH` cuando falta un extremo.

Coste: rectángulos y dos números por cuadro, sin bucles por id. La imagen del AC791N crece **1 116 B**
(579 448 → 580 564 de 583 612, 3 048 libres); `.data+.bss` no cambia (93 800 de 98 304).

## Comprobaciones (host x86-64, las mismas fuentes)

`tests/morph_test` — página y perillas, la captura como copia, la interpolación (0 = A exacto, 127 = B exacto, 64 =
la media), lo que nunca se toca (MUTE, motor, SRC/DST de la matriz), el slew y su llegada exacta, **y las nuevas**:
`B < T4` guarda el sonido de la pista 4; no es una referencia; el morfo va de 20 a 110 en la cutoff y pasa por la
media; `A < T4`; CC 78 lleva la posición a 127 y vuelve a 0 exacto; el controlador camina en pasos de 3 como mucho;
−9 y 300 se recortan; CC 79 y 80 guardan la pista del canal.

`tests/morph_audible_test` — 21 casos, 0 fallos (cada motor audible, escalón medido en el giro).
`tests/morph_six_test` — 6 transiciones con seis pistas y FX, 0 fallos (ninguna con click).

Nada de esto dice cómo suena en la FM-1 física: son las mismas fuentes compiladas en el host. El gasto real del
AC791N, los FPS del panel y el comportamiento del encoder en la unidad no se declaran aquí.

## Pendientes que quedan (declarados)

- Las escenas siguen siendo **RAM**: no hay formato que las guarde como tales. Lo que sí persiste es un par hecho de
  pistas (`A < Tn` / `B < Tn`), que es la ruta recomendada para un preset que dure.
- La lista de ids que participan del morfo es la del sistema de movimiento; no hay máscara por escena.
- `DIFF` sigue reservado (no dibuja nada).
