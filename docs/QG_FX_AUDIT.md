# QG — Auditoría de FX y del catálogo de presets

**Fase:** 5/6 · **Fecha:** 12 de noviembre de 2026
**Clase de toda medida de este documento: host x86, 44,1 kHz, por el código real** (`tests/fx_probe`,
`tests/reverb_test`, `tests/dly_echo_test`, `tests/fx_meter_test`, `tests/preset_audit_test`), **no** la unidad.
Nada aquí es una escucha.

## 1. Qué hay hoy

16 páginas de FX en `PAGES[]` (`params.c`): `FX`, `DIST`, `AUTO PAN`, `DLY`, `DLY 2`, `DLY 3`, `REVERB`,
`REVERB 2`, `CHORUS`, `SLICER`, `SL BUS`, `SL BUS 2`, `SL PITCH`, `MASTER`, `MASTER EQ`, `STEREO M/S`.
El chorus tiene dos tipos: `CHORUS` y `PHASER` (`N_CHOTYPE`).

## 2. Los seis problemas que pedía la fase, medidos

| Lo que se dijo | Medida | Veredicto |
|---|---|---|
| **Phaser débil** | pico del peine **23,6 dB** dentro de 200 Hz..6 kHz (barriendo el tono con el LFO congelado) | no es débil: el peine es profundo. Que un tono fijo no lo note es física del phaser, no un defecto |
| **Phaser con pops / barridos inestables** | salto máximo de muestra al mover DEPTH: **0**; la cadena no se dispara (máximo Q15 14 602) | sin click y acotado |
| **M-WIDE** | 0 = mono exacto (r 1,000); 64 = intacto; 127 = r −0,594 | **arreglado hoy**: ver §3 |
| **EQ / gráficos / respuesta de filtros** | LOW SH −24 dB llega a −24,0 dB y es monótona sin pasarse de 0; +24 dB llega a +23,9; PEAK +12 da +11,7 dB en el centro y 0,0 en los extremos | correcto |
| **HPF/LPF que silencian el audio** | 1 kHz: off 2690, LOWCUT 2616 (−0,2 dB), BASS+ 2483 (−0,7 dB); 60 Hz: off 2693, LOWCUT 620 (**−12,8 dB**), BASS+ 1506 (−5,1 dB) | ningún perfil se acerca al silencio: el corte es sólo de graves y el medio queda intacto |
| **Calidad y consumo de reverb y delay** | `reverb_test`: ROOM idéntico bit a bit al anterior, SPRING con su decaimiento y su chirp, todo modelo acotado bajo 2 s de ruido fuerte, RESO afinado (C +2 st = E, x1,26; FOLW en G x1,50); `dly_echo_test`: el tren de ecos cae por FEEDBK y termina en nada (sin repetición congelada) | correcto en forma y estabilidad; **el coste en CPU del target no está medido aquí** (sin toolchain) |

## 3. Lo que se cambió: M-WIDE deja de subir el nivel

Antes: `L' = M + g·S`, `R' = M − g·S` con `g = ancho/64`. A 127 (`g = 1,98`) el par **subía +3,8 dB**: el
compresor del master entraba por girar el knob, es decir el control de anchura cambiaba también el volumen y hacía
bombear la mezcla.

Ahora el par conserva su potencia: `M' = M·k`, `S' = g·S·k`, con `k = sqrt(2/(1+g²))` en una tabla de 64 valores
(`MW_COMP`, Q15, 128 B de flash, leída una vez por bloque: la raíz no entra en el camino de la muestra).

| Medida | Antes | Ahora |
|---|---|---|
| correlación a 127 | −0,590 | **−0,594** (igual de ancho) |
| nivel a 127 contra 64 | **+3,8 dB** | **−0,0 dB** |
| fuente mono a 127 | mono, +3,8 dB | mono (r 1,000), −3,9 dB |
| 64 | intacto | intacto (bit a bit) |
| 0 | mono exacto | mono exacto |

El −3,9 dB sobre material **mono** al máximo ancho es el precio de conservar la potencia: una fuente sin lado no
tiene nada que ensanchar, y el test lo acota (`fabs < 4 dB`) para que nadie lo empeore sin darse cuenta. El test
de nivel se apretó de ±6 dB a **±1 dB** porque ahora está garantizado.

## 4. VU meters: miden señal

- **Pistas**: `trk[].peak`, el pico del bloque, calculado en `voice.c`/`fx.c`; es lo que dibujan el visualizador
  de HOME y el OVERVIEW nuevo.
- **Buses de FX**: `fx_vu[]` (DIST, chorus, delay, reverb y sus envíos), los picos medidos de verdad; el mixer y
  la fila de GLOBAL los leen de ahí.
- **Balística**: `fx_meter_step` (ataque inmediato, caída a un cuarto por frame, retención de 30 frames, clamp),
  fijada por `tests/fx_meter_test.c`. Antes de 3.6 los arrays `vu[]`/`hold[]` de la página FX se declaraban y se
  dibujaban pero **nunca se escribían**: la barra, la línea de retención y el tick de envío no dibujaban nada.

Lo que queda pendiente aquí: **unificar la presentación** — el OVERVIEW de HOME usa una barra de 12 px sobre
`trk[].peak` y la página FX usa la balística de `fx_meter_step`. Mismo origen de datos, dos dibujos; unificarlos
es una decisión de diseño (y de espacio en pantalla), no un defecto.

## 5. Lo que la fase pedía y **no existe** (estudiado, no implementado)

| Efecto | Estado | Coste / veredicto |
|---|---|---|
| **Flanger** | no existe | barato: es el chorus con retardo corto y realimentación (`fx.cho_*` ya tiene la línea). ~40 líneas + 4 parámetros + página. **Candidato natural de la próxima entrega** |
| **DJ filter** (un solo knob: LPF↔HPF con reso) | no existe | medio: necesita un par de coeficientes por bloque sobre el master y una página de 1–2 parámetros. Encaja en `MASTER` |
| **Bitcrusher global** | sólo por motor (`LOFI > CHIP/CRUSH`, `NOISE > CRUSH`, `MATH` futuro) | bajo si se hace como bus: cuantizador + decimador sobre el master, 2 parámetros. Pero ya hay tres motores que lo dan: valor medio |
| **Compresor / dinámica** | no existe (las páginas `COMP` son *step components*, no compresión) | alto: un compresor de bus con lookahead pide RAM de retardo y una medición de CPU en target. No es una entrega pequeña |
| **Distorsión** | existe (`DIST`, con tipo y tono) | — |
| **Auto-pan** | existe (`AUTO PAN`, con forma y sync) | — |
| **Slicer** | existe (`SLICER` + 3 páginas de bus) | — |

## 6. El catálogo de presets, inventariado y auditado

`tests/preset_audit_test.c` (nuevo, en la suite) carga **cada preset de fábrica** como lo hace PRESETS y lo
hace sonar una nota, midiendo:

| Motor | Presets | Silenciosos | Calientes | Nombres repetidos | Duplicados exactos | Fuera de rango |
|---|---|---|---|---|---|---|
| ANALOG | 56 | 0 | 0 | 0 | 0 | 0 |
| PHASE | 25 | 0 | 0 | 0 | 0 | 0 |
| LOFI | 25 | 0 | 0 | 0 | 0 | 0 |
| VOICE | 20 | 0 | 0 | 0 | 0 | 0 |
| WHEEL | 18 | 0 | 0 | 0 | 0 | 0 |
| DRUM | 12 | 0 | 0 | 0 | 0 | 0 |
| NOISE | 17 | 0 | 0 | 0 | 0 | 0 |
| FM6 | 29 | 0 | 0 | 0 | 0 | 0 |
| **total** | **202** | **0** | **0** | **0** | **0** | **0** |

Qué significa cada columna: *silencioso* = RMS de la nota por debajo de 8 (un LSB de 16 bits es 1); *caliente* =
más del 2 % de la nota dentro del último tramo del soft clipper (se **informa**, no falla: un cuadrado de LOFI
vive ahí a propósito); *fuera de rango* = un `P_E0..P_E7` fuera de su descriptor o una etapa de envolvente > 127.

**Lo que este test no hace: escuchar.** Prueba que el sonido existe, está en rango y no es copia de su vecino. No
puede decir si un pad es bueno. Cualquier afirmación sobre cómo suena la biblioteca necesita la FM-1 y oído.

### Sobre "actualizar los presets" (§3 de la fase)

El inventario está hecho y es verde: no hay duplicados ni silencios que justifiquen una reescritura. Lo que la
fase pide después —aprovechar parámetros nuevos de EDIT, envolventes, modulación, FX nuevos, STRING/ADD/MATH—
**depende de entregas que todavía no existen**:

- **A1 (KTR en ANALOG)** está especificada pero no implementada (`docs/QG_ENGINE_AUDIT.md` §9).
- **STRING, ADD y MATH** están estudiados y no implementados (`docs/QG_NEW_ENGINES.md`); STRING espera la
  decisión de memoria.
- **Flanger / DJ filter** son las dos incorporaciones de FX con mejor relación coste/valor y ninguna está hecha.

Reescribir los 202 presets antes de eso sería cambiar valores al azar, que es exactamente lo que la fase prohíbe.
El orden correcto es: cerrar las entregas de arriba, y **después** tocar presets con una razón por preset.

**Vía de recuperación**: los presets de fábrica son código (`eng_*.c`, tablas `*_PRESETS`), así que cualquier
revisión futura queda en el historial de git, preset por preset, y se puede volver a cualquier versión con
`git show <commit>:firmware/src/eng_analog.c`. Los presets de usuario y los proyectos no se tocan en ninguna de
estas entregas.
