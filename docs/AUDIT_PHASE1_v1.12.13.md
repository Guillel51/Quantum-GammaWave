# Fase 1 (2026-11-22): auditoría real, perfilado y estado técnico — QG-1

Punto de partida verificado en el árbol, no citado: `main` en `1ac98cd5` (release `v1.12.12`, sellada y pusheada),
firmware **1.12.12**, sitio y `versions.json` en 1.12.12, emulador reconstruido en esa release, imagen
**580 148 / 583 612 B**, `.data+.bss` **93 640 / 98 304 B**, pool spare **9 248 B** (suelo 8 KiB).
Clases de evidencia usadas aquí: **host x86_64** (`cc -O2`), **render host** (`tests/ui_render.c`), **WASM**
(`web/emu`), **AC791N build** (toolchain y SDK del proyecto), **físico**: no hay unidad, nada se afirma sobre ella.

## 1. Serie de versión

`main` está en la serie **1.12.x** (1.12.12 publicada). La actualización de esta fase es **1.12.13**: publicar
como 1.7.x sería un *downgrade* frente a 1.12.12 — el instalador y `versions.json` comparan versiones — así que la
numeración se mantiene monotónica y el número pedido se explica en vez de obedecerse.

## 2. Perfilado: qué había y qué hay ahora

`tests/regress.c` imprime "el preset más caro de cada motor". En este host esa tabla salía **vacía (0.0 ns en las
diez filas)**: el máximo se elegía por *instrucciones por muestra* (`ri_instructions`, que sólo existe en macOS vía
`proc_pid_rusage`), así que con el contador a 0 ningún preset superaba el máximo inicial y las filas quedaban en
cero. El perfil que la tabla existe para dar no existía justo en la máquina donde se trabaja.

Corregido en v1.12.13: el máximo se elige por instrucciones cuando el kernel las cuenta y por **ns por muestra**
(carrera con el mismo trabajo de calibración, ventana menor de varias) cuando no. La cabecera de la tabla dice
cuál de los dos instrumentos se usó. Medición de esta sesión (`cc -O2`, mediana de ventanas, host x86_64):

| trabajo | ns / muestra |
|---|---:|
| idle (mezcla sin notas, con metrónomo apagado) | 76.2 |
| idle con pistas DRUM | 106.9 |
| ANALOG · TUBE KEYS (8 notas) | 371.4 |
| NOISE · DUST HAZE | 360.8 |
| VOICE · BROKEN HYMN | 341.7 |
| DRUM · CONCRETE KICK | 336.8 |
| LOFI · CYBER DRONE | 329.3 |
| 6 pistas llenas | 325.0 |
| WHEEL · FULL ORGAN | 292.4 |
| FM6 · EMBER TINES | 267.1 |
| WTABLE · TIDAL MIRROR | 247.3 |
| ADDTV · TINE EP | 247.5 |
| PHASE · CRYPT BELL | 235.6 |

Lectura: el coste **fijo** de la mezcla en reposo (76 ns) es una cuarta parte del preset más caro; los motores
que más pesan por nota son ANALOG, NOISE, VOICE y DRUM. Cualquier optimización de DSP de la fase siguiente debe
apuntar primero a lo que corre **siempre** (buses, envíos, retornos) y después a ANALOG/NOISE, en ese orden.

## 3. Pendientes del registro maestro: estado re-verificado

| pendiente | estado hoy | evidencia |
|---|---|---|
| Medidores de la página FX (`vu[]`/`hold[]` muertos) | **resuelto** | `tests/fx_meter_test.c` 13/13, `ui_test` |
| Coste de páginas FX/Reverb/Master/DRUM-X | **medido** | informe COST de `ui_render` |
| Splash sin BOOT/% | **resuelto** | `ui_pixels` mide la línea de tiempo |
| M STRT (micro-start) | **resuelto** | `tests/microstart_test.c` |
| Pantalla propia de PHASE | **abierto** | PHASE usa EDIT 1/2 + MORPH + FILTER; una página nueva desplaza índices del protocolo del editor |
| MORPH con estados A/B persistentes | **abierto** | el formato de preset no tiene dos estados |
| Animaciones de Reverb por modelo | **abierto** | hay 5 modelos y pantalla por modelo; falta identidad visual por modelo |
| Presets nuevos por familias | **parcial** | 287 sonidos de fábrica hoy (el registro decía 215); v1.12.12 puso TILT/CURVE/ENV ONE en 117 |
| Noveno motor | **descartado** | POOL sin margen (9 248 B, suelo 8 KiB) |
| Instalación V16 | **GO CONDITIONAL** | falta lectura/escritura en unidad real; `tests/stock_profile_test.py` y `install_test.py` verdes |

Ninguno de los "abiertos" se arregla sin tocar el formato de preset (MORPH A/B), el protocolo del editor (página
PHASE) o la unidad física (V16): quedan explicados, no anotados como si estuvieran hechos.

## 4. Bugs conocidos: qué se comprobó de verdad

| área | comprobación en esta sesión | resultado |
|---|---|---|
| HOME e indicadores | `ui_test` completo (incluye HOME, medidores, scope) | verde |
| Visualizer (scope del DAC) | `ui_test` + `ui_pixels` (3 088 frames) | verde, 0 frames inesperados |
| Flickering del preset browser | barrido 193×22 con lint de alineación y `ui_pixels` | 0 lint, 0 desalineaciones |
| FX BUSES / Reverb | `tests/fx_meter_test.c`, `regress` (envíos, 5 modelos) | verde |
| ENV 1 / ENV 2 | `voice.c env_tick` (OFF/LOOP/ONE) + `regress` 307 renders | verde, 0 fallos de salud |
| Instalación V16 | `install_test.py`, `stock_profile_test.py`, `release_gate_test.py` | verde en simulación; **sin unidad** |

Bugs reales encontrados y **corregidos** en esta fase:

1. **Ids de parámetro movidos (compatibilidad de datos).** `P_HARM` (v1.12.4) y `P_LSHP` (v1.12.5) se insertaron
   entre `P_FRES` y `P_MSTART`, desplazando `P_MSTART` 115→117 y `P_NRATE` 116→118. Todo preset de usuario y todo
   proyecto guardados antes de v1.12.4 cargaban M.STRT en el tilt y N.RATE en la curva, perdiendo ambos. Corregido:
   los dos van ahora después de N.RATE; `tests/upreset_test.c` verifica cada invariante por separado (antes era
   una sola línea compuesta, y por eso el fallo sólo decía "old record is wrong"). `regress` 307 renders 0
   cambiados.
2. **El perfil de CPU no existía en Linux** (§2): la tabla salía en 0.0 ns porque el máximo se elegía por
   instrucciones de `proc_pid_rusage`. Ahora hay instrumento para ns y la cabecera lo declara.
3. **`tracks` FAIL 6.91x en ANALOG→LOFI**: el test comparaba el escalón de un acorde bit-crusheado con el de un
   ANALOG. Contra la textura del propio LOFI asentado (3 630 por muestra) es 1.38x: no hay corte, el test estaba
   mal referenciado. Ahora la referencia es el mayor de "antes" y "asentado", con el mismo umbral de 1.5x.
4. Etiqueta falsa en `run_tests.sh` ("251 factory sounds" con 287 en la tabla): el número lo imprime el test.

## 5. Lo que esta fase NO hizo, y por qué

`AGENTS.md` §8: *nunca optimizar el DSP por corazonada*. Sin contador de instrucciones en este host y con el perfil
recién reparado, esta fase entrega **el instrumento y la línea de base** (tabla de §2) y deja la optimización de los
buses en reposo y de ANALOG/NOISE para la fase 2, que podrá medir con las mismas filas antes y después. Tocar
`mix_block` (buses, retornos, reparto entre núcleos) sin ese antes/después sería exactamente lo que la regla
prohíbe.

## 6. Verificación de esta fase (clase host x86_64 / WASM / AC791N build)

- `tests/run_tests.sh` (sin paquete): **NO está verde del todo** — ver §7. Arreglados y verdes:
  `upreset_test` (fallaba desde v1.12.4, punto 1), `tracks` (punto 3), `project_test` y `phase2` / `phase5`
  (literales del layout viejo: ahora afirman el invariante). **Siguen rojos**: `midi_control_test`
  ("CCs outside the map change nothing") y `morph_six_test`.
- `regress`: 307 renders, 0 cambiados, 0 fallos de salud / voz / CPU / crashes.
- `ui_render`: barrido 193 × 22, 0 lint, 0 desalineaciones; `ui_pixels` 3 088 frames idénticos.
- `preset_slots_test`, `preset_library_test` (287 sonidos), `install_test`, `stock_profile_test`,
  `backup_tool_test`, `fmhub_card_test`, `release_gate_test`: verdes.
- `make_release --check`, `sha256sum -c`, `emu: all ok`, `release gate: passed`.

## 7. Lo que estaba rojo, y cómo se cerró (fase 2)

`midi_control_test` — "CCs outside the map change nothing" falla: con el mapa actual, CC20/76/95 sobre la pista 0
modifican algún parámetro de `trk[0].p`. O el mapa creció y el test sigue diciendo que esos CC están fuera, o hay
una escritura fuera de mapa (lo segundo sería un bug de MIDI). Hay que leer la tabla de asignación y decidir con
la evidencia; no se ha hecho en esta pasada.

`morph_six_test` — "MORPH under load" falla en su condición de renders bit-idénticos antes del giro. Puede ser
consecuencia del cambio de layout de §1 (los presets que toca) o una expectativa vieja.

**Cerrado en la fase 2, con la causa real de cada uno:**

- `midi_control`: el test mandaba CC76 como "fuera del mapa", y 76 es TILT desde v1.12.4 (77 es LFO CURVE). La
  lista es 20, 95 y 102. Verde.
- `morph_six`: **nunca se ejecutó**. `run_tests.sh` compilaba `morph_six_test` en la misma línea que el `run` de
  `morph_test`, así que el paso reportaba "not found" siempre. Separada la línea, el binario pasa (6
  transiciones, 0 fallidas).
- `etapa7_audit`: clavaba `N_MDST` en 30; la tabla es 32 desde que HARM/TILT/LSHP se añadieron después de REL
  (los números guardados no cambian, sólo el conteo). Verde.
- `ui_pixels`: los 48 frames de versión (ABOUT/SPLASH) se actualizaron con el barrido de v1.12.13.

**Optimización medida y descartada.** En `mix_block`, el bucle que ponía a cero siete buses escribía 7 palabras por
muestra en una línea de C; reescrito como siete `memset` el resultado fue **idéntico en tiempo** (idle 77.6 / 77.0
/ 78.9 ns por muestra frente a 76.2 / 106.9 de la referencia, misma dispersión). El compilador ya lo vectorizaba:
el cambio se revirtió en vez de presentarlo como mejora. Es el primer dato para la fase 3, que debe atacar el
coste fijo (buses y retornos) con un antes/después, no micro-reescrituras.

**Publicado.** `v1.12.13` sellada y pusheada; suite completa en verde antes de sellar.
