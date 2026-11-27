# FASE 6 — Punch In FX: auditoría y qué se pudo implementar (2026-11-27)

Este documento es deliberadamente corto porque **la fase no está terminada**. Recoge lo auditado,
lo implementado, la medida que bloqueó el resto del rediseño y lo que queda pendiente.

## 1. Auditoría funcional (leída, no ejecutada aún en hardware)

`pfx.c` contiene los nueve efectos y su estado por pista, sin duplicar DSP:

| efecto | estado que el DSP publica | lo que la capa FX puede leer |
|---|---|---|
| REPEAT 1/8, 1/16, 1/32 | `PFX_REP` en `t->pfx`, `t->pfx_sc` (fase de la compuerta), `t->pfx_snn` | sí |
| HALF | `PFX_HALF` | sí |
| DEC- / DEC+ | `PFX_DEC` | sí |
| ATK+ | `PFX_ATK` | sí |
| ARP | `PFX_REP` + período `b/4` | sí |
| RND (CRUSH en banco 2) | `PFX_RND`, `pfx_bank` | sí |
| VIBRATO / DIVE | `pfx_vib`, `pfx_dvn` | sí |
| GATE / RETRIGGER / FREEZE / TAPE / GLITCH | bits de `PFX_*` + `sl_buf` | sí |

La lane de automatización (`pfx_lane`, `pfx_ltgt`, `pfx_block`/`pfx_end`), LATCH (`perf_latched`,
`perf_latch_on`) y los mutes (`PF_M1..PF_M6`) viven en el mismo estado que dibuja el mapa: la
auditoría no encontró ningún control que la interfaz ofrezca y el DSP ignore, ni al revés.

## 2. Implementado en esta pasada

`firmware/src/ui_layer.c`, capa FX: una tecla **blanca** que está activa eleva su cuerpo hacia el
acento de la paleta (`ux_mix(fill, T_ACCENT, 48)`). Antes sólo las teclas negras cambiaban de color
al entrar; las blancas —los nueve efectos y los mutes— se distinguían únicamente por el texto
vertical. La señal se toma del mismo `held = perf_held | perf_latched` que ejecuta el ISR, así que
no hay ningún elemento decorativo: una tecla se ilumina porque el efecto está corriendo.

Medida: 21 fotogramas de las 3 321 del barrido cambian (GREY/MONO/QUANTUM, escenas
`perform_held`, `perform_harm`, `perform_wait`); en `GREY_perform_held` son 1 737 píxeles con un
delta máximo de 99/255, es decir las teclas activas, y nada más. Base de píxeles actualizada y
verificada (`3321 frames identical to the baseline`).

## 3. Por qué no hay más: la medida que lo bloquea

El intento completo (banda de intensidad leída de `t->pfx_sc`, `pfx_vib`, `pfx_dvn`, flash de
pulsar/liberar por tecla) **no enlaza**:

    region `XIP' overflowed by 1608 bytes`

La versión mínima de ese mismo trabajo seguía sin entrar (1 320 B y 1 160 B por encima en dos
recortes sucesivos). La cuenta del build base lo explica: XIP = 576 888 B de 583 612 B de región,
y esa región también aloja la imagen de carga de `.ram_text` (3 940 B), `.c1_text` y `.data`, de
modo que el hueco real ronda 1 KB, no los 6,7 KB que sugiere la resta de código+constantes.

Conclusión honesta: cualquier rediseño del mapa que añada dibujo nuevo exige **primero recuperar
espacio de XIP** (mover una función a RAM, recortar código muerto o comprimir una tabla). No se
hizo en esta pasada, y por eso no se declara la fase terminada.

## 4. Pendiente

- Recuperar ~2 KB de XIP y entonces sí: medidores de intensidad por tecla (`t->pfx_sc` del período,
  `pfx_vib`, `pfx_dvn`), animación de pulsar/liberar con decaimiento, iconos en las teclas.
- FX > BUSES: icono de DELAY, los seis bloques en todas sus variantes, los cuatro que quedaron sin
  cambios, páginas 1/3 y las de DELAY/REVERB.
- Pruebas de hardware: activar/liberar los nueve efectos, combinados, cambios rápidos, LATCH,
  automatización, grabación/reproducción, seis pistas, todas las paletas.

## 5. Verificación de esta pasada

- `./build.sh` AC791N: `image 581668 B`, `XIP code 456628 B, constants 120260 B`, RAM 86 072/98 304.
- `tests/target_budget.py`: `BLOCK_PATH 34380/34380`, `FM6_PATH 404/404`, sin FAIL.
- `build/host/regress`: 316 renders dorados, 0 cambiados.
- `tests/ui_pixels.py`: 3 321 fotogramas idénticos a la base nueva.
