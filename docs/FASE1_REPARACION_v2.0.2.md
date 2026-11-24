# FASE 1/9 — Reparación general y optimización crítica (v2.0.2)

Versión de este árbol: **v2.0.2** (`FELUCCA_VERSION`, `firmware/src/felucca.c`), identidad `FM-1_965`.
Clases de evidencia, como en AGENTS §4: `[HOST]` nativo x86-64, `[TARGET]` build AC791N (pi32v2 clang +
AC79 SDK), `[WASM]` Emscripten, `[STATIC]` inspección, `[PUBLIC]` leído de las URLs públicas.
**No hubo unidad FM-1 física en ningún momento**: nada de este documento es una medición de hardware, y
ninguna compuerta de instalación cambia de estado.

## 1. Estado revisado (no supuesto)

| Qué | Cómo se comprobó | Resultado |
|---|---|---|
| Rama y árbol | `git status`, `git log` | `main` en `7768f2f` (v2.0.1), árbol limpio antes de esta fase |
| Issues y PRs abiertos | API de GitHub | **0 abiertos** |
| Releases publicados | API de GitHub | v2.0.1 (2026-11-24T08:25:58Z, cinco assets), v1.13.0, v1.11.2, v1.11.1, v1.9.4, v1.9.3 |
| Suite del host | `tests/run_tests.sh` | `ALL HOST TESTS PASSED` sobre v2.0.1 (con el emulador **saltado**: ver §2.1) |
| Presupuestos del objetivo | `./build.sh`, `tests/resources_test.py` | XIP 2 136 libres, RAM 5 616 libres, pool 8 384, LOWBUF 1 120, NOINIT 124 |
| Golden renders | `tests/ui_render`, `tests/ui_pixels.py` | 196 pantallas × 22 paletas, 0 lint, 0 desalineaciones, 3 129 frames |

## 2. Defectos encontrados y corregidos

### 2.1 La suite se saltaba el emulador aunque Emscripten estuviera instalado

`tests/run_tests.sh` preguntaba sólo `command -v emcc`. En este entorno `emcc` vive en `$HOME/emsdk` y se
alcanza con `emsdk_env.sh`, así que la suite imprimía `== skip the browser emulator` y **el grupo de paridad
con el build nativo (88 200 frames bit a bit) nunca corría como parte de la suite**. Un grupo saltado es un
hueco en la evidencia, no un resultado verde. Ahora la suite, si no encuentra `emcc` en `PATH`, busca el SDK
en `$EMSDK`, `$HOME/emsdk` y `/opt/emsdk` y lo carga; el grupo se ejecuta.

### 2.2 La firma del autoguardado se calculaba dos veces cada 250 ms

`autosave_poll()` alimenta dos lectores del mismo hash: el aviso de llenado del proyecto (`proj_fill_poll`)
y el autoguardado. Cada uno llamaba a `autosave_sig()` por su cuenta, con la misma cadencia de 250 ms, así
que los ~9 KB del proyecto (6 pistas × todos sus parámetros vía `motion_base_value`, sus 64 pasos y sus
parches FM6, más los globales) se recorrían **dos veces por cuarto de segundo** para responder lo mismo.

Ahora el sondeo calcula **una** firma y la comparte; cada lector conserva su reloj (`fill_poll` / `as.poll`),
de modo que un sondeo que sólo le toca a uno cuesta un hash y uno que no le toca a ninguno no cuesta nada.
La semántica de los tiempos (`AS_POLL_MS`, `AS_IDLE_MS`, `AS_GAP_MS`, `AS_RETRY_MS`) y las condiciones de
`autosave_quiet()` no cambian, y `tests/persistence_test.c` (que corre el lazo principal con
`autosave_poll`) sigue pasando entero.

## 3. Optimización medida

`tests/frame_bench` [HOST] señala la firma como **la llamada más cara de un lazo inactivo**: 12 137 ns, unas
cuatro veces un frame completo (`frame` = 3 098 ns en esa misma corrida). Dos cambios, los dos invisibles en
pantalla y en audio:

1. **Ancho del plegado.** `as_mix` era FNV-1a byte a byte. El valor que produce es de RAM
   (`as.saved` / `as.seen` se comparan dentro de un mismo encendido; `project_load` los siembra), así que el
   ancho del plegado es libre: ahora cubre cuatro bytes por multiplicación y deja el byte a byte en la cola.
2. **Ventana de parámetros.** Los `P_COUNT` parámetros por pista se juntan de ocho en ocho antes de mezclar,
   así que una pista cuesta `P_COUNT/8 + 1` mezclas en vez de `P_COUNT`. La ventana son 16 B de pila del lazo
   principal (nunca la ISR de audio).

| Medida (`cc -O2`, 1 200 llamadas, mínimo de seis series, mismo host) | antes | ahora | Δ |
|---|---:|---:|---:|
| `autosave_sig()` | 10 904 ns | **3 661 ns** | **−66 %** |
| llamadas por sondeo de 250 ms | 2 | **1** | **−50 %** |
| trabajo de firma por segundo | ~87 µs | **~15 µs** | **−83 %** |

`[TARGET]` coste del cambio: imagen **581 476 → 581 556 B** (+80 B), **2 056 B libres** sobre el suelo de
2 048; `.data+.bss` 92 688 B (sin cambio), pool 8 384 de holgura, LOWBUF 31 648/32 768, NOINIT 124 libres.

## 4. Lo que se auditó y no se cambió

| Candidato | Por qué no |
|---|---|
| Recortar `gw_planet_sig()` / el redibujo de HOME cada 2 frames | es la animación del planeta, no trabajo redundante: quitarla es degradar la pantalla |
| Cachear el suelo de HOME (`ux_ground_rows`) | ya agrupa filas del mismo color; sin framebuffer de 240×200 (96 KB) no hay dónde cachear |
| Volver a los recíprocos de DRUM-X (FASE 4 de la serie anterior) | exige duplicar el cuerpo del lazo por caso y el presupuesto de `DRUM_PATH` está a 2 instrucciones de su techo |
| Perfilar la UI con callgrind | no hay valgrind en este entorno: se informa como bloqueado, no se sustituye por una corazonada |

## 5. Pruebas de esta fase

- `tests/frame_bench` [HOST]: la tabla de §3 (A/B con el mismo binario, mismo host, mínimo de seis series).
- `tests/persistence_test.c` [HOST]: pasa entero (autoguardado, secciones, apagado simulado).
- `./build.sh` [TARGET]: imagen y las cuatro ventanas dentro de sus suelos.
- Suite completa `tests/run_tests.sh` [HOST]: el resultado de esta fase está en el registro del release.

**No ejecutado, y por eso no se declara:** ninguna lectura, escritura, instalación, backup ni recuperación
sobre una FM-1 física; ninguna medición de reloj, CPU o audio en la unidad; valgrind/callgrind (no está).
