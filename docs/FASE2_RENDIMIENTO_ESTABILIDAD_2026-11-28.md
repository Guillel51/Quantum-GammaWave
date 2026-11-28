# Fase 2/4 — rendimiento DSP, estabilidad multicore, presets, instalador y resistencia a fallos

Fecha: 2026-11-28. Versión: **v2.22.0**, sobre v2.21.0 (commit `9b6131bc`).

**Clases de medición, por separado y sin mezclarlas** (AGENTS.md §4): `host x86-64` es
`tests/regress` / `tools/dsp_bench.py` compilados con `cc -O2` y ejecutados en este equipo; `build AC791N` es el
`./build.sh` con el toolchain `pi32v2` (instrucciones estáticas sobre `build/felucca.dis`, ventanas de memoria
sobre `build/felucca.map`); `WASM` es el emulador de Emscripten. **No hay FM-1 física en esta fase**: ninguna
cifra dice nada sobre los ciclos de la unidad, y no se escribió nada en ella.

## 0. Punto de partida comprobado

La Fase 1 quedó entregada y publicada: `main` en `9b6131bc`, Release `v2.21.0` con sus cinco activos, XIP
573 244 B e imagen de 578 024 B (5 588 B libres en el slot). No se repitieron sus optimizaciones: esta fase
parte de ese árbol y audita lo que la Fase 1 no tocó.

## 1. Rendimiento DSP y audio en tiempo real

### 1.1 Qué se recorrió, y qué ya estaba

Se leyeron las rutas críticas reales, no los nombres: `audio.c` (`fm1_alnk0_irq`, `audio_block`, `dac_scope`,
`audio_out_peak`), `fx.c` (`mix_block`, `mix_part`, `events_block` -> `seq.c`, `fx_buses`, `fx_gate`,
`fx_sleep`, `slicer_bus`, `lev_block`, `master_out`, `perf_master`), `perform.c` (`perf_begin`, `perf_block`,
`perf_mute`, `perf_pre`), `voice.c` (`track_render`, la ventana de voces y su diferido), `seq.c` (`events_block`,
`kb_lat`), y los diez motores con sus nombres reales en `engines.c`: **ANALOG, PHASE, FM6, LOFI, VOICE
(FORMANT), WHEEL, NOISE, DRUM (DRUM-X), WTABLE, ADDTV**.

Lo que ya estaba resuelto antes de esta fase, y se comprobó leyendo el código (no se volvió a "optimizar"):

| ruta | estado encontrado |
|---|---|
| buses de FX en silencio | `fx_gate` + `fx_sleep` (Fase 3 de 0.6.x): con 1,5 s sin envíos y `wet` bajo −54 dB los buses duermen y sólo avanzan posiciones y LFO |
| capa PERFORM idle | `perf_begin` devuelve 0 y `mix_block` **no llama** `perf_master`/`perf_block`: la pasada por muestra de la capa no existe cuando no hay nada armado |
| SLICER apagado | `slicer_bus` sólo avanza su reloj; `sl_seg` (la pasada por muestra) no corre |
| envíos de una pista seca | el `clamp` de los envíos y las tres ramas de bus viven dentro de `if (c \| d \| r)` |
| coeficientes de TONE / FILT | cacheados por valor (Fase 5); `div_by_n` en las rampas (Fase 1); `mc_split` en un pase sobre el pool (Fase 4) |
| voces por bloque | `mix_part` no asigna memoria y una pista sin voz activa cuesta el tick del LFO y un buffer limpio |

**Conclusión medida de la auditoría: no queda trabajo por muestra estructuralmente redundante en el
plumbing** (mix por pista, buses, máster). El coste que queda es el de los motores y los buses: `regress`
mide 105–110 ns/muestra con la caja en reposo, 296 ns con tres pistas y 342–346 ns con seis (host x86-64).

### 1.2 El cambio: los límites simétricos, en un solo compare

Lo único redundante que quedaba en el camino que corre **siempre** eran dos comparaciones por límite
simétrico. `clamp(v, -k, k)` es la misma operación que `clamp_sym(v, k)` de `dsp.c` —un único compare sin
signo: `(unsigned)v + k > 2k`— y estaba escrito en la forma de dos comparaciones en:

- `fx.c master_out`: los dos clamps de ±262143 que aplican la ganancia del leveler, **en cada muestra de los dos
  canales** (es la línea más caliente del instrumento);
- `fx.c mix_part`: el clamp de los envíos a ±`xmax` (una muestra por pista con envíos);
- `perform.c perf_block`: las cuatro entradas de los filtros LPF/HPF del layer (±32767).

Es bit a bit el mismo resultado por construcción: dentro de ±k devuelve `v`, fuera devuelve el límite del signo,
exactamente lo que hacía `clamp`. Los 316 renders dorados de `tests/regress` salen **0 changed** y los 3 353
frames de `tests/ui_pixels.txt` no se mueven.

| medición (host x86-64, mínimo de 3 corridas) | antes | después | Δ |
|---|---|---|---|
| `idle_drums` (ns/muestra) | 107,9 | 105,8 | **−1,9 %** |
| `3parts_full_drums` | 296,1 | 295,6 | −0,2 % |
| `6parts_full` | 343,2 | 341,8 | −0,4 % |
| `idle_click` | 77,4 | 76,2 | −1,6 % |

**El tamaño del cambio es pequeño y así se reporta.** La escena que más lo nota es la que casi no tiene motores
sonando (el máster pesa más); con seis pistas el motor domina y la mejora queda dentro del ruido del host. No se
presenta como una ganancia de CPU de la unidad: es una medición de host x86-64.

En el objetivo (build AC791N, estático) el cambio **achica** las dos líneas que toca:

| línea (`tests/target_budget.py`) | presupuesto | v2.21.0 | v2.22.0 |
|---|---|---|---|
| `perf_block` (coste ponderado) | 254 | 254 | **249** (−2 %) |
| `BLOCK_PATH` | 34 380 | 34 529 | 34 529 (+0 %) |
| `FM6_PATH` | 404 | 404 | 404 |
| `DRUM_PATH` | 1 153 | 1 016 | 1 016 |
| imagen / slot | 583 612 | 578 024 | **577 992** (5 620 B libres) |
| XIP (código + constantes) | — | 573 244 | **573 212** |
| `.data+.bss` | 98 304 | 90 628 | 90 628 (sin cambio) |

XIP, imagen y RAM se midieron **con el mismo `./build.sh` en los dos árboles** (HEAD extraído con `git archive`
y este árbol, ambos build AC791N), no contra una cifra de otra clase de build: la comparación es la de la
misma tabla. `DRUM_PATH`, `BLOCK_PATH` y `FM6_PATH` son instrucciones estáticas en los lazos de
`build/felucca.dis`, no ciclos. El `.data+.bss` no se mueve ni un byte: la bandera de test de §2 vive bajo
`#if !FELUCCA_DUAL_CORE` y no existe en el build de la unidad.

### 1.3 Lo que se descartó, con la razón

- **Saltar `fx_insane()`** (el OR sobre `fx_wet`/`fx_sl` por bloque): es la red que detecta un bus fuera de
  rango y tira el bloque antes de que un estado corrupto rinda segundos. Cuesta dos operaciones por muestra y
  compra una garantía; no se toca.
- **Un camino rápido en `perf_block`**: no hace falta, `mix_block` ya no lo llama cuando la capa está ociosa.
- **Dividir el lazo de `mix_part` en seco/húmedo** para sacar la rama invariante: reduce trabajo dinámico pero
  **sube** el recuento estático de `BLOCK_PATH` (el gate cuenta instrucciones dentro de lazos), y el intercambio
  no se puede defender con una medición de host que quedaría en el ruido.
- **`tsvf_coef` cacheado por voz**: cada voz tiene su corte (la modulación es por voz) y una caché por voz
  cuesta más estado del que ahorra; la división es una por voz y bloque, no por muestra.

## 2. Estabilidad multicore

### 2.1 Lo auditado

`firmware/hal/fm1_multicore.h` (`fm1_mc_start`, `fm1_mc_run`, `fm1_mc_wait`, el buzón con palabra de control y
`csync`, el autotest de arranque, `fm1_mc_bar`), y en `fx.c` `mc_split`, `mc_job`, `mc_lost`, `mc_join`,
`cpu_mode_apply`/`cpu_mode_step`, `fx_trip` y el reparto por coste medido. En `voice.c`, el diferido de notas
mientras el core 1 trabaja (`voice_defer`/`voice_defer_flush`, cola de 8, nota-off nunca diferido).

Resultado de la revisión: el protocolo es correcto en los tres puntos que importan — (a) lo que core 0 publica
antes de `job` queda ordenado por `csync`, y lo que core 1 escribe antes de `done` también; (b) la fuente de
carrera real (la exploración de teclas por TIMER5 anidada dentro del ISR, que asigna voces de ventanas que core 1
puede estar leyendo) está guardada por `fm1_mc_job_busy` y las notas se reintentan al inicio del bloque; (c) un
core que falla se descarta para siempre (`fm1_mc_bar` + `fm1_mc_hold`), y `fx_bad`/`fx_trip` cubren un bus
corrupto. **No se encontró un defecto nuevo de sincronización en el camino normal.**

### 2.2 El hallazgo: la ruta de fallo no tenía prueba, y en el host divergía

`fm1_mc_wait` tiene tres salidas de fallo (job que no vuelve en 8 ms, buzón mal leído, fallo del core 1). En el
host, `mc_wait()` devolvía siempre 0: la rama `mc_lost()` de `mix_block` —la que descarta el bloque, restaura la
matriz y pasa a un core— **nunca se ejecutaba en ninguna prueba**. Al escribirla apareció además una divergencia
real: en el objetivo, `fm1_mc_wait` llama a `fm1_mc_hold()` (deja `fm1_mc_on = 0`, y `mix_block` toma el camino de
un core en el bloque siguiente); en el host, `mc_lost()` marcaba la barra pero **no** apagaba `mc_host`, así que
el arnés seguía repartiendo trabajo a un core que en la unidad ya estaría parado. Es un defecto del arnés, no del
firmware, y es exactamente el tipo de cosa que una ruta sin cobertura esconde.

Cambios (todos de host salvo el ninguno del firmware de producción):

- `core.h`: `mc_host_lost`, una bandera de test **bajo `#if !FELUCCA_DUAL_CORE`** (no ocupa un byte del
  `.data+.bss` de la unidad).
- `fx.c`: `mc_wait()` del host consume la bandera y devuelve −1 una vez; `mc_lost()` apaga `mc_host`, como el
  `fm1_mc_hold()` del objetivo.
- `tests/ui_test.c`: prueba nueva **DUAL CORE LOST** — un job que no contesta: el core queda barrido, el bloque
  se descarta, las mismas 7 voces siguen activas (ninguna colgada ni perdida por el cambio de core), todas las
  pistas vuelven al core 0 (`mc_mask_last == 0`) y la salida no es silencio.

`ui_test` completo: **0 FAILED** (1 102 líneas de comprobaciones).

### 2.3 Lo que no se puede probar en el host

El instante exacto en que core 1 está a mitad de una pista (`mc_cur`) y `mc_lost` restaura su matriz: en el host
el trabajo corre de forma síncrona dentro de `mc_run`, así que `mc_cur` ya volvió a −1 cuando se inyecta el
fallo. Esa parte de `mc_lost` sigue probándose sólo por lectura; en la unidad requiere el core físico.

## 3. Presets y calidad sonora

No se repitió la auditoría de knobs (80 de motor y 118 de pista, 0 muertos, `tests/param_sweep_test.c`): sigue
como regresión y pasa.

Sobre los **127 pares con coseno espectral > 0,995** del informe de la Fase 5:

- **Sonido distinto, no duplicados.** El test de biblioteca compara la firma completa de cada preset (los 8 `e[]`,
  el sobre, `fenv`, `mono`, los 4 envíos y los hasta 6 `x[]`). En los 296 sonidos navegables **no hay ni una firma
  repetida dentro de un motor** — por eso los pares se parecen en el espectro y no son el mismo sonido.
- **No se tocó ningún preset.** Sin oídos en este entorno, "mejorar" un par a 0,995 sería adivinar; el informe de
  la Fase 5 ya dice que HARP PLUCK / SLOW TIDE (0,9896) difieren en RMS, pico, ataque y caída. Cambiar la
  biblioteca a ciegas rompería proyectos y presets de usuario sin una razón medida.
- **El gate se endureció:** una firma idéntica dentro de un motor ahora **falla** `tests/preset_library_test.py`
  (antes se imprimía como nota). Hoy hay 0, así que el gate pasa y una futura redundancia no entra como "otro
  preset". Los 296 navegables y los dos kits DRUM legacy quedan como estaban.
- **Compatibilidad de lo nuevo de la Fase 5b** (ANALOG SYNC/SUB, ADDITIVE DRAW): `tests/analog_sync_test.c`
  (cada knob mueve el audio, el neutro es bit a bit), `tests/project_test.c` (FUN1..FUN6) y
  `tests/upreset_test.c` pasan sin cambios: los proyectos y presets anteriores cargan igual.

## 4. Seguridad del instalador y de la actualización

Revisado el flujo completo: `tools/fm1_install.py` (paquete, identidad, SHA-256, límites, handshake, escritura
por el loader, reinicio, verificación), `tools/fm1_safewrite.py`, `tools/fm1_backup.py` / `fm1_restore.py`,
`tools/fm1_preflight.py`, y del lado del firmware `firmware/loader/loader.c` (`ldr_flash_known`),
`firmware/hal/fm1_flash_id.h` (`FL_JEDEC_OK(id)`, allowlist de dos JEDEC documentados) y `ota.c` (geometría por
JEDEC). **No se modificó el bootloader, ni el mapa de flash, ni ninguna protección de chip.**

Lo que las pruebas cubren hoy (todas pasan en esta entrega):

| riesgo | prueba |
|---|---|
| paquete truncado / demasiado corto, sin marcador de loader, fichero inexistente | `tests/install_test.py` (exit 2, **nada** se envía por MIDI) |
| SHA-256 ausente, que no coincide, o `SHA256SUMS` con otra línea | `install_test.py` (exit 2, sin MIDI) |
| identidad de otro modelo / otro producto tras el reinicio | `install_test.py` (exit 6, nada enviado) |
| loader que no aparece / no vuelve / se cuelga | `install_test.py` (exit 5, con tope de tiempo) |
| desconexión a mitad de transferencia | `install_test.py` (`unplug_after`, exit 4, "nothing was written") + `tests/backup_tool_test.py` |
| el loader pide fuera del paquete | `install_test.py` ("outside the package", exit) |
| lecturas CRC malas, direcciones desplazadas, respuestas inestables | `install_test.py` (faults inyectados en el dispositivo simulado) |
| JEDEC desconocido, dos lecturas que no coinciden, capacidad/protección | `tests/flash_id_test.c`, `tests/preflight_test.py`, `tests/chip_test.py`, `tests/flash_evidence_test.py` |
| backup/restore y recuperación | `tests/backup_test.c`, `tests/backup_tool_test.py`, `tests/backup_audit_test.py` |
| V15/V16 oficiales modificados | `install_test.py` (rechazo por SHA, "no MIDI") |

**SHA-256 verifica integridad, no autenticidad.** El paquete de esta release se publica con su
`build-manifest-v2.22.0.json`, su `.fwsc.manifest.json`, `SHA256SUMS` y la atestación de artefacto de GitHub
(cadena de trazabilidad), pero **no está firmado con una clave del publicador**: nada en esta entrega afirma lo
contrario. La matriz de `docs/VALIDATION_MATRIX.md` sigue siendo la que decide si el instalador público se
habilita, y sus filas que necesitan unidad física siguen sin evidencia de unidad.

No se realizó ninguna escritura sobre un FM-1 físico.

## 5. Pruebas ejecutadas en esta entrega

| grupo | comando | resultado | clase |
|---|---|---|---|
| suite de host completa | `NO_PACKAGE=1 tests/run_tests.sh` | ver §5.1 | host x86-64 |
| regresiones doradas / salud / voces / rutas | `build/regress` | 316 renders, 0 changed, 0 health, 0 routing | host x86-64 |
| motores y FX | `phase2_test`, `fx`/`reverb`/`dly_echo`/`analog_sync_test`, `param_sweep` | ok | host x86-64 |
| estrés con ASan/UBSan | `tests/stress_run.sh 3` | ok | host x86-64 |
| presets | `preset_library_test.py`, `preset_audit_test` | 296 sonidos, 0 firmas repetidas | host x86-64 (datos) |
| multicore | `ui_test` (DUAL CORE, DUAL CORE LOST) | 0 FAILED | host x86-64 |
| instalador / backup | `install_test.py`, `backup_tool_test.py`, `preflight_test.py` | ok | host x86-64 (dispositivo simulado) |
| presupuestos de objetivo | `tests/target_budget.py`, `tools/mem_budget.py` | ver §1.2 | build AC791N (estático) |
| emulador | `web/emu/emu_test.mjs`, `web/test_web.mjs`, `web/test_pkg_bounds.mjs` | ok | WASM |
| sitio y coherencia | `make_release.py --check`, `coherence_check.py`, `web/test_site.py` | ok | datos del repositorio |

Los tiempos del host **no son ciclos del AC791N**; las líneas de `target_budget.py` son instrucciones estáticas
en los lazos, no una medición de tiempo.

## 6. Riesgos y pendientes (no se venden como hechos)

- **Sin unidad física**: nada de esta fase dice cómo suena ni cuánto tarda en la FM-1. El core 1 real, el
  timbre y los ciclos siguen pendientes de una unidad.
- **`mc_lost` a mitad de pista** (matriz restaurada) sólo está probado por lectura en el host (§2.3).
- **Los 127 pares ≥ 0,995 siguen sin diferenciar**: la decisión es musical y necesita oído; lo que esta fase
  garantiza es que no son duplicados por parámetros y que uno nuevo fallaría el test.
- **XIP**: 5 620 B libres (por encima del mínimo de 2 048 y del objetivo de 4 KiB de la Fase 1). La siguiente
  recuperación grande tendría que salir de código (`ui_input_frame`, `fm1_main`, `draw_columns`, `graph_tech`,
  `.pool`), no de banderas.
- **Instalador público**: la habilitación sigue atada a la matriz; esta fase no cambia ninguna fila a `GO`.
