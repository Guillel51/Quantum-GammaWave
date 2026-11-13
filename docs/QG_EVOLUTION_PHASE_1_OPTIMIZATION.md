# QG — EVOLUCIÓN, FASE 1: optimización total del firmware

**Fecha:** 13 de noviembre de 2026 · **Base:** `main` = `e70c199` (v2.9.0) · **Rama:** `freebuff/5b290d6da44f6281decb5d8d`
**Este documento no declara ninguna optimización que no se haya medido o demostrado, y no cita ninguna medición de
hardware. No hay unidad FM-1 en este entorno y no se ha escrito nada en ninguna.**

## 0. Clases de evidencia

Las mismas reglas de `AGENTS.md` §4. Cada número lleva su clase; ninguna clase se presenta como otra.

| clase | qué prueba |
|---|---|
| `[HOST]` | x86-64, esta VM compartida, `cc -O2`; tiempos de **este árbol en esta máquina**, no del chip |
| `[PROFILE]` | `gprof` sobre el binario `[HOST]` del banco de audio (muestreo de 10 ms) |
| `[RENDER]` | `tests/ui_render.c` (COST), coste de dibujo en esta VM; sólo valen proporciones dentro de una tanda |
| `[STATIC]` | recuento objetivo sobre el binario/objeto construido aquí (símbolos, instrucciones de división, bytes) |
| `[TARGET]` | toolchain pi32v2 + AC79 SDK: **no disponible en esta sesión**; todo lo que se cita es de builds anteriores |
| `[FÍSICO]` | una unidad M-VAVE FM-1: **ninguna**. No hay latencia, ni CPU real, ni JEDEC, ni escritura |

---

## 1. Estado recuperado antes de tocar nada

### 1.1 Árbol y suite

- `main` está en **`e70c199`** (v2.9.0). El informe de la fase 1 anterior
  (`docs/QG_PHASE_1_OPTIMIZATION.md`) se midió sobre `72c645c` (v2.8.4): **está desfasado**, no es el árbol de
  esta fase. `docs/OPTIMIZATION_BASELINE.md` y `docs/OPTIMIZATION_PHASES.md` son de v2.2/v2.3.
- La suite del árbol de partida pasa: **285 renders dorados, 0 cambiados** (`tests/regress.c`, ejecutado en esta
  sesión antes de cualquier cambio), y el barrido de UI no da hallazgos. El informe anterior hablaba de 220
  dorados y el registro maestro de **215** presets de fábrica; el árbol de hoy tiene **267** presets y **285**
  dorados. **Documentación desfasada, no código roto**: se anota aquí para que nadie vuelva a citar 220/215.

### 1.2 Pendientes: qué está implementado y qué no

De `docs/MASTER_PENDING_REGISTER_2026-11-11.md`, verificado contra el árbol:

| pendiente | estado real en `e70c199` | ¿entra en esta fase? |
|---|---|---|
| M STRT (micro-start) | **implementado** (`P_MSTART`, `voice.c`, página VOICE 2) | no: ya está |
| splash sin `BOOT %` | **implementado** (no queda la cadena) | no |
| medidores de FX (balística, hold, CLIP) | **implementado** (`fx_meter_step`) | no |
| pantalla propia de PHASE | **no implementada** | **no**: es función musical nueva (prohibido en esta fase) |
| MORPH con estados A/B persistentes | **no implementado** | no: formato/preset nuevo |
| animación por modelo de reverb | **no implementado** | no: es dibujo nuevo |
| familias de presets nuevas | **no** (267 remasterizados) | no: contenido musical |
| noveno motor | **evaluado, no viable** por POOL | no |
| build AC791N / WASM / unidad | **bloqueado** (sin toolchain, sin `emcc`, sin unidad) | se reporta, no se marca hecho |

- **Ramas y PRs:** el triage de `origin/pr/23`..`pr/35` sigue vigente y **no se fusiona ninguna**: son forks del
  árbol anterior a QG-1 que revertirían refinamientos actuales. No hay PRs abiertos con material nuevo. No hay
  stashes.
- **Nada se ha revertido.** Las compuertas de instalación siguen **NO-GO** y no se han tocado; el bootloader, el
  SPL, el mapa de flash y la recuperación no se han modificado (ver §7.4).

---

## 2. Instrumentos y el límite de esta sesión

| instrumento | estado | qué permite afirmar |
|---|---|---|
| `tests/fx_bench` (bloque completo, 3 escenas, pared + CPU de proceso) | **sí** | ns/muestra del bloque en esta VM |
| `gprof` | **sí** | reparto por función y por línea `[PROFILE]` |
| `tests/isr_bench` (tick de TIMER5, GPIO modelado) | **sí** | ns de CPU por tick y por etapa `[HOST]` |
| `tests/ui_render.c` COST | **sí** | µs de redibujado por pantalla `[RENDER]` |
| `objdump` / `nm` / `size` sobre el objeto del camino de audio | **sí** | instrucciones de división, bytes de `.text`/`.data`/`.bss` `[STATIC]` |
| `tests/target_budget.py` | **no**: falta `build/felucca.dis` | instrucciones pi32v2 por función `[TARGET]` |
| build AC791N (`JIELI_TOOLCHAIN` + `AC79_SDK`) | **no** | imagen, secciones, POOL, XIP `[TARGET]` |
| `emcc` (WASM), `valgrind`/callgrind, `perf` | **no** | emulador nuevo, conteo exacto de instrucciones del host |
| unidad física | **no** | latencia, CPU real, estabilidad de audio `[FÍSICO]` |

### 2.1 El ruido del host, medido (y por qué manda en las decisiones)

El mismo binario, `taskset` a un núcleo, seis corridas seguidas del mismo banco:

| corrida | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|
| ns/muestra, escena 0 (mejor de 3) | 131,0 | 128,0 | 132,5 | 126,5 | 159,1 | 137,4 |

**±11 % entre corridas idénticas.** Consecuencia, escrita antes de elegir el trabajo: en esta VM **no se puede
resolver una diferencia del 1–3 %**, y por tanto en esta fase sólo se integra lo que tiene además una prueba
**estructural** (`[STATIC]`: divisiones ejecutadas por bloque, bytes, instrucciones) y una prueba de
**equivalencia bit a bit** (los dorados). Un candidato sin esa prueba no entra (§5).

---

## 3. Línea base medida

### 3.1 Bloque de audio `[HOST]`

Dos medidas, porque una sola no dice nada en esta VM:

**(a) mejor de 8, la corrida de referencia** (`tests/fx_bench 8`, `taskset`):

| escena | antes (`e70c199`) | después (misma hora, corrida aparte) |
|---|---:|---:|
| todos los buses alimentados | 122,4 ns/muestra | 126,8 ns/muestra |
| partes secas (envíos 0) | 119,5 | 121,1 |
| buses alimentados, CHORUS en FLANGER | 116,8 | 119,2 |

**(b) A/B emparejado, misma ruta de build** (`/tmp/fx_before` y `/tmp/fx_after`, 6 alternancias, mediana de los
mejores de 3 de cada corrida):

| escena | antes | después | delta |
|---|---:|---:|---:|
| todos los buses alimentados | 125,4 | 126,3 | **+0,7 %** |
| partes secas (envíos 0) | 122,2 | 123,0 | **+0,7 %** |
| buses alimentados, CHORUS en FLANGER | 119,9 | 120,3 | **+0,3 %** |

**Lo que se afirma:** en este host la diferencia está **dentro del ruido de ±11 % medido en §2.1** y, si algo es,
es una fracción de punto *más lenta*: la división del x86 es hardware y la caché añade comparaciones y un store.
**No se declara ninguna mejora de CPU en el host.** Lo que sí se demuestra es el efecto estático sobre el target
(§4.4: 19 sitios de división menos, ~16 divisiones ejecutadas por bloque menos, donde cada una es una rutina de
libgcc) y la igualdad bit a bit (§4.3). Si el target midiera lo contrario, el cambio se revierte: queda dicho.

### 3.2 Reparto del tiempo `[PROFILE]`

`cc -O2 -pg`, 3 repeticiones de 4 000 bloques:

| función | % del bloque |
|---|---:|
| `fx_buses` | **35,7** |
| `mix_part` | 14,3 |
| `track_render` | 14,3 |
| `fm6_render` | 14,3 |
| `master_out` | 7,1 |
| `rev_room` | 7,1 |

Llamadas por bloque (mismo perfil): `div_samples` **8,1**, `fx_glide` **3**, `lev_block` **1**, `drum_mix` **1**
(parte de batería). Es exactamente donde se ha actuado (§4).

### 3.3 Tick de TIMER5 `[HOST]`

`tests/isr_bench 200000`: **32,6** ns/tick (idle), **36,1** (16 LEDs + respiración), **37,2** (columna pulsada).
Etapas: `fm1__sr_bits` (16 bits) **45,8** ns, `fm1__keys` (una columna) **2,9** ns, `fm1__frame` **19,4** ns cada
11 ticks. El tick ya se optimizó en la fase anterior (una columna sin cambios no recorre el debounce).

### 3.4 Dibujo `[RENDER]`

De `build/ui_new/report.txt` (barrido de esta sesión, `cc -O1`, 200 redibujados):

| pantalla | redibujado completo (µs) | textos+iconos |
|---|---:|---:|
| HOME | 224,9 | 34 |
| PRESETS | 114,2 | 38 |
| ROLL_CHORDS | 179,7 | 24 |
| MENU | 143,4 | 27 |
| FX | 236,3 | 49 |
| ABOUT_CREDITS | 249,8 | 48 |
| MASTER | 110,2 | 31 |
| HOME, un frame **sin forzar** (strips perezosos) | **59,6** | 3 |

Las pantallas caras son las de texto denso (ABOUT/CREDITS: 51 494 px de glifo por dibujo), no las animadas. El
sistema de strips ya evita el redibujado completo: un frame de HOME sin cambios forzados cuesta el **26 %** de uno
completo.

---

## 4. Lo integrado

Tres cambios, todos en `firmware/src/fx.c`, todos **bit a bit idénticos** y todos con el mismo objetivo: **quitar
divisiones del camino de audio**, que es lo caro en pi32v2 (no hay división en hardware: cada `/` es una rutina de
libgcc, y el árbol ya paga por ello — es la razón medida de que `-Os` sea un 355 % más lento que `-Oz`,
`docs/OPTIMIZATION_BASELINE.md` §3).

### 4.1 El tempo del bloque se calcula una vez, no ocho

`beat_samples()` dividía `FS * 60 / bpm` **en cada llamada**, y un bloque la llama hasta ocho veces (la rejilla del
transporte, el AUTO PAN de cada parte, el X-MOD de DRUM-X, el eco WIDE del delay). Es una función pura de tres
palabras (`BPM`, `G_CLOCK`, `midi_beat_samples`): ahora se guarda con esas tres palabras como clave. Un acierto es
una comparación y una carga; la división sólo ocurre cuando el tempo, el reloj o el tempo MIDI medido cambian de
verdad.

`div_samples()` dividía además por `DIV_DEN[div]`; para los ids 0..3 los divisores son **1, 2, 4 y 8**, es decir un
desplazamiento exacto (`quarter >> div`), y sólo quedan divisiones para los ids 4, 5 y 10+ (3 y 6).

### 4.2 Las rampas por bloque, por desplazamiento

Tres sitios dividían por la longitud del bloque, que en todo camino de audio es `CTL` = 32 (potencia de dos):

| sitio | qué dividía | antes | después |
|---|---|---|---|
| `fx_glide` (3 llamadas/bloque: DEPTH, WIDE y MIX del coro; MIX del delay) | `(d>>2)/n` y `d/n` | **2 divisiones por llamada** | desplazamiento |
| `lev_block` (1/bloque) | `(g1 - lev_g)/n` | 1 división | desplazamiento |
| `drum_mix` (1 por parte de batería/bloque) | `(pm1 - pm0)/n` | 1 división | desplazamiento |

El helper `div_by_n(x, n)` reproduce **exactamente** la división de C (truncamiento hacia cero, no suelo) mediante
`(x + ((x >> 31) & (n - 1))) >> log2(n)`, la misma forma que ya usa `amp_at` en `dsp.c`, y **conserva la división
real** para cualquier otra longitud de bloque (tests, `hostsim`, futuros tamaños): fuera de `CTL` no cambia nada.

**Divisiones ejecutadas por bloque en las escenas del banco:** ≥ 16 antes (8,1 de `div_samples`/`beat_samples`,
6 de `fx_glide`, 1 de `lev_block`, 1 de `drum_mix`), **0 en esas rutas** después (con `n == CTL`, que es siempre el
caso en el camino de audio).

### 4.3 Prueba de equivalencia (no una promesa)

1. **285 renders dorados, 0 cambiados, 0 perdidos** (`tests/regress.c tests/golden.txt`), 0 fallos de salud, 0 de
   voces/routing, 0 entradas de CPU sobre presupuesto. El sonido es byte a byte el mismo.
2. **`tests/tempo_test.c` (nuevo, en la suite):**
   - `beat_samples()` contra la división directa para **BPM 40..300 × reloj interno on/off × 4 tempos MIDI**
     (incluido 0 = «aún no medido»), leyendo tres veces cada combinación (la segunda y la tercera son la caché);
   - cambio de tempo en marcha, llegada y retirada de un reloj MIDI externo, y BPM < 40 leído como 120;
   - `div_samples(0..15)` contra la expresión de referencia a 38 tempos;
   - `div_by_n` contra `/` en 6 longitudes (incluida `CTL` y longitudes no potencia de dos) × 17 valores extremos
     (incluidos `±1`, `±31/32/33`, `±262143`, `±1048576`) y 200 000 valores barridos.

   Resultado en esta sesión: **3/3 ok**.

### 4.4 Coste `[STATIC]`

| medida | antes | después |
|---|---:|---:|
| instrucciones de división en la unidad de traducción del banco (`objdump`) | 284 | **265** |
| `.text` del objeto del banco (host) | 218 125 B | **216 421 B** (−1 704 B) |
| `.data`/`.bss` del objeto | 27 224 / 280 914 B | 27 232 / 280 914 B (**+8 B**) |
| caché en el binario del banco (`nm`) | — | **11 B** (`beat_cv` 4 + `beat_cm` 4 + `beat_cb` 2 + `beat_cc` 1) |
| dorados | 285 | **285, 0 cambiados** |

**RAM:** +11 B de `.bss` sobre un margen citado de 10 608 B (`[TARGET]`, §7.1). **Flash `[TARGET]`: no medido**;
en el host el `.text` **baja** 1 704 B porque los caminos de división se pliegan. La cifra del chip no se afirma.

---

## 5. Medido y **rechazado** (para que no se repita)

| candidato | medida / razón | veredicto |
|---|---|---|
| **Saltarse el coro cuando la línea está en silencio** (bit-exacto: tras 93 ms de envío 0 la línea de 4 096 muestras es toda ceros) | se midió el **techo**: con el coro desactivado del todo, la escena seca pasa de 119,5 a **118,4 ns/muestra** (≈ 1,1 ns, dentro del ruido). El ahorro no paga la máquina de estados ni el riesgo de tallos obsoletos | **rechazado** |
| **Quitar la pasada de picos de los envíos** (96 cargas/bloque) plegándola en el bucle que copia `send_*` a `fxq_*` | ahorro estimado ~1 %, pero acopla `fx_buses` a un estado global que `tests/mod_test.c` ya limpia a mano (`memset(fxq_c…)`): picos obsoletos cambiarían la compuerta (audio, no sólo un medidor) | **rechazado por fragilidad** |
| `-O3` en el camino de audio | ya medido: 174,6 vs 124,3 ns/muestra (+40 %) | descartado (sigue vigente) |
| `-Os`/`-O2` globales para el target | ya medido con toolchain: `-Os` +355 % en `analog_render`; `-O2` no entra en XIP | descartado |
| 12 voces | ya medido: +2 272 B de `.data+.bss` con el layout viejo; con el pool, 10 720 B libres pero sin unidad para el coste real | no se cambia el perfil enviado |
| recíproco fijo en las rampas de DRUM-X | ya medido: el presupuesto estático empeora +91 % (el clang de pi32v2 duplica la rama en el bucle) | descartado |
| mover `.bss` a LOWBUF/POOL | las tres vidas de LOWBUF **no** son disjuntas (guardar un proyecto durante la reproducción) y al POOL le queda ~1 KiB útil sobre el suelo de 8 KiB | descartado |
| limitador del master sin división por muestra | cambia el sonido (tabla o Newton dan otros bits) | no se toca |

---

## 6. CPU y arquitectura: análisis y estado

Diseño vigente (sin cambios en esta fase; verificado en el árbol):

- **Núcleo 0:** master, UI, pantalla, partes 1–3 y los buses cuando el núcleo 1 no está vivo.
- **Núcleo 1:** partes 4–6 y los buses (`MC_FX`), con su propio banco EMU y su buzón. El reparto lo decide
  `mc_split()` con **lo que cada núcleo tardó** en los bloques anteriores; un núcleo 1 que falla, no contesta o lee
  mal el buzón queda **descartado para siempre** (`mc_lost()`), con el bloque de sus partes descartado y el núcleo 0
  renderizando todo desde el siguiente. Se muestra en MENU > ABOUT.
- **Interrupciones:** TIMER5 (10 kHz, prioridad 4) **anida** en ALNK0 (audio, prioridad 3) y sólo escanea GPIO y
  teclas; el tiempo anidado se descuenta del presupuesto de carga del render (`t5_nested_ticks`). El USB de audio
  (`uac_service`) también corre anidado, porque un paquete por milisegundo no puede esperar al render.
- **DMA:** ALNK0, doble búfer; una mitad perdida se cuenta (`audio_lost`) y se recupera en la siguiente.
- **Sincronización:** buzón no bloqueante; los buffers del núcleo 1 (`c1_buf`, `c1_mix`) son suyos; los resultados
  se suman al final del bloque (`mc_join`). La comprobación de cordura de los buses (`fx_insane`) sigue activa y
  sigue siendo la red que detecta una lectura mala del núcleo 1.
- **Sobrecarga:** `shed_check` (dos mitades seguidas por encima del 80 % → se suelta una voz) y `load_tight` al 65 %
  (no se añaden voces, el coro pasa a media carga en LOAD ECO, los granos de CLOUD a la mitad).
- **Sin `malloc` en el camino de audio:** verificado en el árbol (`grep` sobre `firmware/src`: **0** llamadas a
  `malloc`/`calloc`/`realloc`; `pb_free` de `perform.c` es un `int`, no `free()`).

**Lo que queda por atacar** es el tick de 10 kHz (el mayor consumidor **estático** del núcleo 0 en el modelo del
target: 1 564 instrucciones, `[TARGET]`, citado) y el propio DSP. Ninguno de los dos se puede mover sin el build
del target (para el recuento de instrucciones) o sin la unidad (para el tiempo real): esta fase **no lo finge**.

---

## 7. Memoria

### 7.1 Presupuesto `[TARGET]` (citado de builds anteriores, **no re-medido aquí**)

| región | usado | capacidad | margen |
|---|---:|---:|---:|
| `.data + .bss` | 87 696 B | 98 304 B | 10 608 B |
| POOL (`.pool` + pilas del núcleo 1) | 311 240 B | 327 680 B | 15 392 B (suelo 8 KiB) |
| LOWBUF (`proj_scratch`, `sec_stage`, `undo_st`) | 26 532 B | 28 672 B | 2 140 B |
| código RAM | 19 284 B | 24 576 B | 5 292 B |
| XIP | ~380 KB de código | 581 052 B | ~200 KB |
| imagen / paquete | 551 720 B / 610 959 B | 1 MiB NOR | — |

### 7.2 Auditoría de residentes (lo que ocupa y por qué)

Del binario `[HOST]` construido aquí (`nm --size-sort`), los mayores residentes del camino de audio y su
naturaleza: `dly_buf` 128 KiB (la línea del delay: es el **tiempo** de delay, en POOL), `cloud_buf` y `sl_buf`
32 KiB cada uno (granular y SLICER, POOL), `rev_comb` 9,9 KiB y `comb_buf` 6 KiB, `trk` 10,4 KiB (las seis partes),
`fm6_note` 7 KiB, `cho_buf` / `shim_buf` / `rpre_buf` 4 KiB cada uno. **Ninguno es duplicado de otro**: los tres
buffers de LOWBUF son los únicos multi-KiB con vidas teóricamente disjuntas y ya se demostró que **no** lo son
(§5). No hay asignación dinámica, no hay fugas posibles y no hay fragmentación: todo es estático.

**Comprobación de un falso positivo habitual:** en la tabla de símbolos del host los arrays de presets aparecen
como `d` (datos), lo que sugeriría 15 KB copiados a RAM en el arranque. **No lo son**: están declarados
`static const preset_t …[]` y el símbolo cae en `.data.rel.ro` por las reubicaciones de los punteros de nombre en
un ejecutable PIE. En el target van a XIP. No hay nada que mover ahí.

### 7.3 Lo que cuesta esta fase

**+11 B** de `.bss` (la caché del tempo). Cero buffers nuevos, cero tablas nuevas, cero estado nuevo.
NOINIT, POOL, LOWBUF y el mapa de flash **no se han tocado**.

### 7.4 Zonas protegidas

Bootloader, SPL, mapa de flash, `NOINIT` y recuperación **no se han modificado**. Ninguna compuerta de instalación
se ha movido: la instalación sigue **NO-GO** por falta de evidencia física, exactamente como estaba.

---

## 8. DSP y gráficos

- **Osciladores, filtros, envolventes, FM6, voces:** revisados; el árbol ya trae las optimizaciones de las fases
  anteriores (operadores FM6 que se saltan por debajo de umbral con cuatro bucles sin banderas por muestra,
  coeficientes de filtro por bloque, pool de voces, cero divisiones por muestra en el filtro trapezoidal,
  `clamp_sym` en una comparación). **No se ha encontrado trabajo redundante que quitar sin cambiar el sonido.**
  La única familia de trabajo redundante que quedaba era la de divisiones por bloque (§4), ya quitada.
- **UI:** el dibujo no enmascara interrupciones en ningún punto (verificado: las únicas llamadas a `fm1_irq_off`
  están en `hal/fm1_irq.h`, en el guardián de arranque/fallo, no en el camino de la pantalla). Un dibujo largo no
  puede retrasar al ISR de audio: compite por CPU, no lo bloquea. Con strips perezosos un frame sin cambios cuesta
  el 26 % de un redibujado completo (§3.4) y las pantallas tienen refresco mínimo por página (`SCR_MIN`).
  **No se ha cambiado nada**: no hay problema medido que justifique tocar el dibujo en una fase que promete cero
  regresiones visuales.

---

## 9. Verificación ejecutada en esta fase

```
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"   # tablas y headers
cc -O2 … tests/regress.c && build/host/regress tests/golden.txt tests/cpu_baseline.txt
build/host/tempo_test
NO_PACKAGE=1 ./tests/run_tests.sh
```

### 9.1 Resultado

- `tests/regress`: **285 renders dorados (0 cambiados, 0 perdidos)**, 0 fallos de salud (clipping/DC/silencio),
  0 de voces/routing, 0 entradas de CPU sobre presupuesto, 0 crashes.
- `tests/tempo_test` (nuevo, dentro de la suite): **3/3 ok**.
- Suite completa (`NO_PACKAGE=1 ./tests/run_tests.sh`) sobre el árbol de esta fase:

```
== tempo: the cached beat, div_samples and div_by_n equal the divisions they replaced
tempo: beat_samples: the cache returns the division, at every tempo and clock ok
tempo: div_samples: 0..15 equal the DIV_DEN divisions at every tempo          ok
tempo: div_by_n: truncation toward zero, block length and others              ok
...
ALL HOST TESTS PASSED
EXIT=0
```

**4 874 líneas `ok`, 0 `FAIL`, exit 0.** El log de esa corrida es `run_tests.log` en la raíz del árbol: es el
registro de que la suite pasa en el árbol que se entrega, no en uno anterior.

### 9.2 Lo que la suite cubre (y por tanto lo que esta fase ha vuelto a probar)

ASan/UBSan en editor, proyecto, backup, robustez y banco FM6; los dos fuzzers con semilla fija (20 000 paquetes de
editor, 5 000 stores de proyecto); el barrido de UI (todas las pantallas × 17 paletas, lint de alineación, tintas);
el emulador nativo bit a bit; paquete, instalador, backup, Verify Only y firma (simulados); y las pruebas de audio,
persistencia, editor, USB y MIDI.

### 9.3 No ejecutado, y por qué

`tests/target_budget.py` (falta `build/felucca.dis`), `tests/memory_map_test.py` (falta el ELF del target), build
AC791N (sin toolchain ni SDK), WASM (sin `emcc`), callgrind (sin valgrind) y **cualquier cosa en una unidad física
(no hay unidad)**. Nada de eso se presenta como aprobado.

---

## 10. Riesgos residuales

| riesgo | severidad | estado |
|---|---|---|
| Ninguna medición `[TARGET]` en esta sesión | alto | reportado; el cambio no toca memoria crítica ni flags y baja el número de divisiones |
| El delta horario de esta fase está por debajo del ruido del host | medio | declarado como tal; la prueba es estática (§4.4) y bit a bit (§4.3) |
| La caché del tempo añade 11 B de estado persistente entre bloques | bajo | clave de tres palabras; `tests/tempo_test.c` cubre cambio de tempo, reloj MIDI y BPM inválido |
| `div_by_n` sólo desplaza cuando `n == CTL` | bajo | el camino de audio siempre usa `CTL`; el resto conserva `/` y está probado en ambos casos |
| Documentación desfasada (220 dorados / 215 presets / fase 1 sobre v2.8.4) | medio | **corregido en este documento**; los informes viejos no se citan como estado actual |

---

## 11. Lo que la fase 2 necesita para mover la aguja de verdad

1. **Build AC791N** (`JIELI_TOOLCHAIN` + `AC79_SDK`): sin él no hay `.dis`, ni `target_budget.py`, ni presupuesto
   de secciones, ni paquete. Es la puerta de todo lo `[TARGET]`.
2. **Un contador de instrucciones en CI** (callgrind o `tests/cpu_vg.py`): permitiría resolver cambios del 1 % que
   el reloj de esta VM no distingue.
3. **La unidad**: sólo ella puede decir si el tick de 10 kHz y el DSP caben con margen, y sólo ella puede validar
   latencia, pops y estabilidad con varios motores y efectos a la vez.
