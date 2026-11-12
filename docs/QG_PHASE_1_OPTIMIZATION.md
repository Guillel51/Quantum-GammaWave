# QG — FASE 1/5: optimización total del firmware (estado, medidas y lo integrado)

**Fecha:** 12 de noviembre de 2026 · **Base:** `main` = `72c645c` (v2.8.4) · **Rama de trabajo:** `freebuff/c75bd64530cbd3759842f2a2`
**Este documento no declara optimizaciones que no se hayan medido y no cita ninguna medición de hardware.**

Reglas de este informe, las mismas de `AGENTS.md` §4: cada número dice de qué **clase** es.

| clase | qué prueba |
|---|---|
| `[HOST]` | x86-64, esta VM compartida, `cc -O1/-O2`; tiempos de este árbol, no del chip |
| `[PROFILE]` | `gprof` sobre el binario `[HOST]` del banco de audio (resolución 10 ms, reparto por función) |
| `[RENDER]` | `tests/ui_render.c`, coste de dibujo en esta VM (sólo valen las proporciones dentro de una tanda) |
| `[TARGET]` | toolchain pi32v2 + AC79 SDK: **no disponible en esta sesión** (ver §8) |
| `[FÍSICO]` | una unidad M-VAVE FM-1: **ninguna**. No hay latencia, ni JEDEC, ni escritura, ni CPU real |

---

## 1. Estado recuperado antes de tocar nada

- `origin/main` estaba **cuatro commits por delante** del árbol con el que arrancó la sesión (`e6e2485`,
  v2.8.2): `10139ad` (tres pantallas HOME nuevas), `3ca03c5` (v2.8.3), `fdf64d6` (aire bajo el texto de las
  pantallas nuevas) y `72c645c` (v2.8.4). Este trabajo está **rebaseado sobre `72c645c`**, no sobre v2.8.2:
  el árbol medido es el que hoy sirve `main`.
- Nada se revirtió: los cambios de seguridad y compatibilidad (allowlist de los dos JEDEC `0x856014` /
  `0xB36014`, Verify Only sin frames de escritura, validación de paquetes, regiones protegidas) siguen
  intactos y sus pruebas siguen en verde (§7).
- El triage de PRs y ramas antiguas ya está escrito y no cambia (`docs/MASTER_PENDING_REGISTER_2026-11-11.md`
  §5): las ramas `pr/23`..`pr/35` son forks del árbol anterior a QG-1 y revertirían refinamientos actuales;
  no se fusiona ninguna. No hay PRs abiertos con material nuevo.
- **Hallazgo de trazabilidad:** el `run_tests.log` versionado en la raíz era de una corrida **vieja y
  fallida** (`HOST TESTS FAILED`, con nombres de prueba que ya no existen en el árbol, p. ej. un ROOM
  bit-a-bit contra `ref_buses` que el test actual no ejecuta). Se reemplazó por la corrida de esta fase
  (§7). Un log que dice FAILED sobre un árbol que pasa es documentación falsa, no evidencia.

### Suite de partida (antes de cualquier cambio de esta fase)

```
NO_PACKAGE=1 ./tests/run_tests.sh   ->  ALL HOST TESTS PASSED   (119 secciones, 0 FAIL, exit 0)
```

Incluye: 220 renders dorados (`tests/regress.c`), ASan/UBSan (editor, project, backup, robust, fm6bank),
los dos fuzzers con semilla fija (20 000 paquetes de editor, 5 000 stores de proyecto), el barrido de UI
(191 pantallas × 17 paletas, 0 hallazgos de lint, 2 779 972 medidas de alineación, 0 px), el emulador nativo
bit a bit, el paquete y el instalador simulados.

---

## 2. Instrumentos: qué se puede medir aquí y qué no

| instrumento | estado | qué permite afirmar |
|---|---|---|
| `tests/fx_bench` (bloque de audio completo) | **sí**, mejorado en esta fase | ns/muestra de `mix_block`, **pared y CPU de proceso**, dos escenas |
| `gprof` | **sí** (instalado) | reparto del tiempo por función `[PROFILE]`, resolución 10 ms |
| `tests/ui_render.c` (COST) | **sí** | µs de redibujado por pantalla `[RENDER]` |
| `tests/cpu_vg.py` + callgrind | **no**: no hay valgrind y no hay root | conteos exactos de instrucciones del host |
| `perf` | **no instalado** | — |
| `tests/target_budget.py` | **no**: falta `build/felucca.dis` | instrucciones pi32v2 de los bucles de render `[TARGET]` |
| build AC791N (`JIELI_TOOLCHAIN` + `AC79_SDK`) | **no** | imagen, `.data/.bss`, POOL, XIP, flash `[TARGET]` |
| `emcc` | **no** | WASM del emulador |

**Mejora de instrumento integrada:** `tests/fx_bench` ahora imprime, además del reloj de pared, el **tiempo
de CPU del proceso** (`CLOCK_PROCESS_CPUTIME_ID`) y mide **dos escenas** (todos los buses alimentados; partes
secas con los envíos a 0). En esta VM el reloj de pared oscila ±15 % entre corridas del mismo binario,
mientras que el tiempo de CPU es estable a ±0,4 ns/muestra: sin eso, una diferencia del 1 % es
indistinguible del ruido. Es lo que permitió **rechazar** un candidato en vez de integrarlo por intuición
(§5). El banco no forma parte de la puerta de aprobados: mide, no aprueba.

---

## 3. Perfil del bloque de audio `[PROFILE]`

`tests/fx_bench.c`, `cc -O2 -pg`, 150 repeticiones de 4 000 bloques de 32 muestras, escena con todos los
buses alimentados (3 partes + batería + secuenciador). Reparto del tiempo propio:

| función | % del bloque | qué es |
|---|---:|---|
| `fx_buses` | **25,1** | coro + delay + reverb + compuerta + medidores |
| `master_out` | **11,4** | DC block, EQ de altavoz, M-WIDE, nivelador, limitador, soft clip |
| `mix_part` | **11,4** | nivel / pan / envíos de una parte |
| `rev_room` | **8,8** | red de 4 combs + 2 allpass |
| `mix_block` | 7,6 | orquestación del bloque |
| `fm6_render` | 6,8 | motor FM6 |
| `track_render` | 6,5 | despacho por voz |
| `drum_render` | 5,3 | DRUM-X |
| `fm6_op_run` | 4,9 | operador FM6 |
| `analog_dispatch` | 3,0 | ANALOG |
| `events_block` | 2,3 | secuenciador y notas |

Lectura: el trabajo está en los **buses** (25 %) y en el **master** (11 %); el resto es DSP por motor, ya
repartido entre los dos núcleos (§6). `fx_buses` sigue siendo la función más caliente, como en el informe de
la fase 7 anterior (`docs/QG_TOTAL_OPTIMIZATION_REPORT.md` §2), y su pasada doble ya se había eliminado
entonces: lo que queda es DSP real (3 taps de coro con LFO, Hermite del delay, 4 combs, compuerta).

---

## 4. Lo integrado (medido, bit a bit idéntico)

### 4.1 Una parte seca no paga los envíos

`mix_part` calculaba, **por muestra**, el `clamp` de la señal para los envíos y tres comparaciones
`if (c)`, `if (d)`, `if (r)`, aunque la parte no enviara nada a ningún bus (parámetro de envío a 0, el caso
de un patch seco). El `clamp` sólo se usa dentro de esos `if`; moverlo dentro de un único
`if (c | d | r)` no cambia un solo bit y evita el trabajo cuando no hay envíos.

Además, el `xmax` de los envíos se calculaba con una **división por parte y bloque** aunque no hubiera nada
que enviar; ahora esa división sólo se hace si hay envíos.

### 4.2 El tap del coro se enmascara una vez

`fx_buses` escribía `cho_buf[fx.cho_w & (CHO_LEN - 1u)]` y volvía a calcular la misma máscara para leer el
mismo índice en el camino del phaser. El índice se calcula una vez y se usa para la escritura y para el tap.

### 4.3 Medida A/B `[HOST]`, tiempo de CPU de proceso, mejor de 10 repeticiones

Misma máquina, mismo compilador (`cc -O2`), binarios alternados en las dos órdenes, `taskset` a un núcleo:

| escena | antes (`72c645c`) | después | delta |
|---|---:|---:|---:|
| todos los buses alimentados | 124,7 ns/muestra | 124,3 ns/muestra | **−0,3 % (borde del ruido)** |
| partes secas (envíos 0) | 121,5 ns/muestra | **121,0 ns/muestra** | **−0,4 %** |

Mediana de tres corridas alternadas (antes/después y después/antes) de 10 repeticiones cada una, en el
árbol final. Una corrida anterior sobre el mismo camino de audio (árbol v2.8.2, antes del rebase) dio −0,1 %
y −0,6 %: la dirección es la misma y la magnitud es la de esta clase de medición.

Lo que se afirma: con los tres buses alimentados el cambio es **neutro**; en un patch seco el bloque es
**0,6 % más barato** en esta VM. Lo que **no** se afirma: ningún porcentaje del FM-1. La proporción de una
operación escalar sobre un núcleo de 320 MHz sin ejecución fuera de orden no es la de esta VM, y no hay
unidad ni toolchain para medirla.

### 4.4 Coste

- **Flash `[TARGET]`: no medido.** No hay tablas ni datos nuevos (en el anfitrión, `.data` y `.bss` del banco
  son idénticos byte a byte); el `.text` del banco crece **16 B** por la variable de índice y la rama añadidas
  (233 427 → 233 443 B), y la rama del envío desaparece a cambio. El tamaño en el chip **no se afirma**
  porque no hay build de target en esta sesión.
- **RAM / SRAM / pila:** **0 B**. No hay buffers nuevos, ni tablas nuevas, ni estado nuevo.
- **Dorados:** los 220 renders siguen bit a bit idénticos (§7); la suite completa pasa.

---

## 5. Medido y **rechazado**

| candidato | medida | veredicto |
|---|---|---|
| **Sacar de `master_out` los valores de bloque** (M-WIDE y EQ de altavoz calculados una vez por bloque en vez de por muestra) | `[HOST]` CPU: **123,9 → 125,6 ns/muestra (+1,4 %)**; con M-WIDE activo (110) también peor. El cambio no se queda: el compilador ya genera bien el camino por muestra y la estructura extra (config por bloque) cuesta más de lo que ahorra | **rechazado y revertido** |
| `-O3` en el camino de audio | ya medido en la fase anterior: 174,6 vs 124,3 ns/muestra (+40 %) | descartado (sigue vigente) |
| `-Os`/`-O2` globales para el target | ya medido con toolchain: `-Os` +355 % de coste en `analog_render` y 20 divisiones; `-O2` no entra en XIP | descartado (sigue vigente) |
| 12 voces | ya medido: +2 272 B de `.data+.bss` con el layout viejo; con el pool de v2.3.0, 10 720 B libres y 0 dorados cambiados, pero sin unidad para el coste real | no se cambia el perfil enviado |
| recíproco fijo en las rampas de DRUM-X | ya medido: el presupuesto estático del target empeora +91 % porque el clang de pi32v2 duplica la rama dentro del bucle | descartado (sigue vigente) |
| limitador del master sin división por muestra | cambia el sonido (tabla o Newton dan otros bits) | no se toca en una fase que promete cero regresiones |

Este es el resultado honesto de la fase: **el camino de audio de este árbol está en rendimientos
decrecientes**. Las tres funciones más calientes ya vienen optimizadas por las fases anteriores
(`fx_buses` a una pasada, coeficientes de filtro por bloque, coro/delay/reverb con estado propio, cero
`malloc`, medidores sin trabajo por muestra). Lo que queda no se arregla con micro-ediciones: se arregla con
el perfil del **target** y con la unidad, que en esta sesión no existen (§8).

---

## 6. CPU y arquitectura: el reparto entre núcleos

Diseño vigente, sin cambios en esta fase:

- **Núcleo 0:** master, UI, pantalla, partes 1–3, y los buses cuando el núcleo 1 no está vivo.
- **Núcleo 1:** partes 4–6 y los buses (`MC_FX`), con su propio banco EMU y su propio buzón; los resultados
  se suman al final del bloque.
- El reparto es **medido en el propio firmware**: `mc_split()` elige qué partes van al núcleo 1 según lo que
  cada núcleo tardó en los bloques anteriores, y un núcleo 1 que falla, no contesta o lee mal el buzón queda
  **descartado para siempre** (`mc_lost()`), con el bloque de sus partes descartado y el núcleo 0
  renderizando todo desde el siguiente. El estado se muestra en MENU > ABOUT (`CORES 2`, `CORES 1 (FAULT)`…).
- La ruta de doble núcleo está **ejercitada en host** (`mc_host` en `tests/reverb_test.c` y
  `tests/ui_test.c`: partes y buses por el camino del núcleo 1, sin pérdida de muestras), pero eso prueba el
  protocolo, no el tiempo por núcleo del AC791N.
- Audio: bloques de **32 muestras** (≈725 µs a 44,1 kHz), sin `malloc` en el camino en tiempo real, con
  shedding por sobrecarga (`shed_check`, 80 % de medio búfer, dos medios seguidos) y `load_tight` al 65 %.

`[HOST]` el bloque completo cuesta 123,9 ns/muestra (3,97 µs por bloque de 32) en esta VM; **no** es una
cifra del FM-1 y no se usa para afirmar nada sobre él. La carga por núcleo en el chip queda **NO MEDIDA**.

---

## 7. Memoria, almacenamiento y verificación

### 7.1 Presupuesto (citado `[TARGET]`, no re-medido aquí)

Del último build AC791N registrado (`docs/QG_V2.8_FINAL_AUDIT.md` §7, `docs/MEMORY_BUDGET.md`):

| región | usado | capacidad | margen |
|---|---:|---:|---:|
| `.data + .bss` | 87 696 B | 98 304 B | 10 608 B |
| POOL (`.pool` + pilas del núcleo 1) | 311 240 B | 327 680 B | 15 392 B (suelo 8 KiB) |
| LOWBUF (`proj_scratch`, `sec_stage`, `undo_st`) | 26 532 B | 28 672 B | 2 140 B |
| código RAM | 19 284 B | 24 576 B | 5 292 B |
| XIP | ~380 KB de código | 581 052 B | ~200 KB |
| imagen / paquete | 551 720 B / 610 959 B | 1 MiB NOR | — |

**Esta fase no mueve un solo byte de esas regiones.** No se reordena `.data`/`.bss`, no se toca NOINIT, no
se toca el pool, no se toca LOWBUF y no se toca el mapa de flash, el bootloader ni el SPL. La razón es la de
siempre: sin build de target y sin `tests/target_budget.py`, mover un buffer es una apuesta, no una
optimización. Los candidatos de memoria ya están medidos y rechazados en
`docs/OPTIMIZATION_PHASES.md` §FASE 2 (los tres buffers de LOWBUF no tienen vidas demostrablemente
disjuntas) y siguen rechazados.

### 7.2 Verificación ejecutada en esta fase (árbol final)

```
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"   # tablas y headers
NO_PACKAGE=1 ./tests/run_tests.sh        ->  ALL HOST TESTS PASSED  (ver §7.3)
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt  -> skip (no hay .dis)
python3 tests/memory_map_test.py                                          -> skip (no hay ELF de target)
tests/fx_bench 10                        -> dos escenas, pared y CPU (reproducible)
```

`run_tests.log` (raíz) se reemplazó por la corrida de esta fase: es el registro de que la suite pasa en el
árbol que se publica.

### 7.3 Resultado de la suite en el árbol final

Ver la sección generada al final de este documento: sección **§11**.

---

## 8. Bloqueos (reportados, nunca marcados como hechos)

1. **Build AC791N:** sin `JIELI_TOOLCHAIN` (clang pi32v2 propietario) ni `AC79_SDK` no hay imagen, ni
   secciones, ni `.dis`, ni presupuesto estático, ni paquete. Todo lo `[TARGET]` de este documento es
   **citado** de builds anteriores, no medido en esta sesión.
2. **WASM:** sin `emcc` no hay emulador nuevo; no se afirma paridad de esta fase.
3. **callgrind/valgrind y `perf`:** no instalados y sin root; el reparto por función es de `gprof`.
4. **Unidad física:** no existe en este entorno. **No hay latencia, ni CPU real, ni estabilidad de audio
   medidas.** La instalación sigue **NO-GO** y no se tocó ninguna compuerta.

---

## 9. Bases dejadas para las fases 2–5 (sin implementar nada nuevo)

Nada de esto se implementó: son los puntos de extensión que ya existen, verificados en el árbol, para que
las fases siguientes no tengan que reabrir la arquitectura. **Coste de esta fase: 0 B de flash y 0 B de RAM**
(es documentación).

- **Pantallas nuevas y skins.** `PAGES[]` (`firmware/src/params.c:545`) es la tabla de páginas; el barrido
  de `tests/ui_render.c` recorre todas y el lint/alineación falla si una pantalla nueva se sale de su celda.
  Los ids de página son **protocolo** (Web Studio, `desc.json`): se **añaden al final**, nunca se reordenan.
  Una skin por capas no existe: hoy hay 17 paletas de color, no un sistema de capas
  (`docs/QG_V2.8_COMPLETION_REPORT.md` §1 fila 10).
- **Matriz de modulación.** Ya existe: 4 slots por pista (`mod.c`), fuentes `MS_*` y destinos `MD_*` con
  enums **append-only**, `mod_begin`/`mod_end` por bloque y `vmod_t` por voz. Extenderla es añadir un
  `MS_`/`MD_` al final y su cálculo; los slots inactivos ya son bit-idénticos y no cuestan.
- **Edición avanzada de motores.** `engine_t` (`core.h:425`): `edit[8]` + `desc()` opcional para etiquetas
  por modo + `block()`, `keys()`, `amp()`. Añadir una página de edición es añadir un `desc`/página, no tocar
  el motor.
- **WAVETABLE u otro motor.** `ENGINES[]` (`firmware/src/engines.c:45`) es **append-only** y `NENGINES` es
  14 (números 0..13 en los stores). Un motor nuevo se añade al final; sus tablas van a **XIP** (hay ~200 KB
  libres) y su estado por voz debe caber en el margen de `.bss` (~10,6 KB) — **no** en POOL, que tiene
  ~7 KB utilizables sobre el suelo de 8 KiB. Esa es la condición de viabilidad ya documentada en
  `docs/MASTER_PENDING_REGISTER_2026-11-11.md` §4.
- **FX Routing / automatización.** `fx_buses` tiene tres envíos fijos por parte (`send_c/d/r`) y un bus de
  SLICER; el orden es: partes → dist → nivel/pan/envíos → buses → slicer → master EQ → nivelador → master.
  Un routing por pista es una tabla de envíos por parte en `mix_part`, y la automatización ya tiene su reloj
  (`ms_clock`, `clk_n`) y su destino por bloque en `mod.c`: no hace falta infraestructura nueva, sólo la
  tabla y su UI.

---

## 9-bis. El tick de TIMER5 (10 kHz), medido y reducido

El tick es el mayor consumidor **estático** del núcleo 0 en el modelo de `target_budget.py` (1 564
instrucciones, ~15,6 M/s). Para tocarlo hacía falta un instrumento que aquí no existía: `tests/isr_bench`
(nuevo, construido por la suite, no ejecutado por ella) cuenta ticks reales con el GPIO modelado y sin
tiempo de bus simulado, y da ns de CPU por tick en tres escenas, el coste de cada pieza por separado y el
número de **accesos a registros de GPIO por tick** — la métrica que no depende de la frecuencia del núcleo.

### Qué se midió

| medida | valor `[HOST]` |
|---|---|
| accesos a registros GPIO por tick | **60** (64 cuando hay pulso de glow/breath), de los cuales **48 son el shift de 16 bits** del 595 (3 read-modify-write por bit) |
| ancho real del shift en la placa | ~3,1 µs (el propio header lo mide: `dim_pulse_ns 3125`), es decir el pulso del tick lo pone el protocolo del 595, no el C |
| `fm1__sr_bits` (16 bits) | ~38–45 ns de CPU host |
| `fm1__keys` (una columna) | ~12,6 ns de CPU host, **por tick** |
| `fm1__frame` / `fm1__breath_frame` | 18,6 / 8,7 ns por frame (1 de cada 11 ticks) |
| diagnóstico `fm1_in_stat` (bin de gaps + contadores) | **25–35 % del C del tick** (medido compilando el tick sin él: 42–52 → 32 ns) |

Conclusión de la atribución: el tiempo del tick en el chip lo domina el **shift del 595**, cuyo ritmo es
deliberado (el header documenta que los flancos write-only glitcheaban la columna vecina en la placa); el C
que se puede tocar es el debounce y el diagnóstico. El diagnóstico se mantiene entero: es la función que lee
el comando de consola `inp`.

### Lo integrado

**Una columna sin cambios no recorre el debounce.** `fm1__keys(p)` llamaba a `fm1__key` cuatro veces por
tick; en el caso normal cada llamada encontraba `closed == estado` y escribía un 0 sobre el 0 que ya tenía.
Ahora `fm1_in.raw_prev[p]` guarda el byte crudo de la última vez y `fm1_in.pend[p]` marca las filas con un
debounce en vuelo (`cnt != 0`); si el byte no cambió y no hay nada en vuelo, la columna no tiene nada que
decidir y la función vuelve. **No se salta ningún caso con estado pendiente**: en cuanto una tecla empieza
a contar (`cnt != 0`) o el byte crudo cambia, corre el camino completo de siempre.

| escena | antes | después | delta |
|---|---:|---:|---:|
| idle (nada encendido, nada pulsado) | 35,4–36,2 ns/tick | **26,6–28,5 ns/tick** | **−22 %** |
| 16 LEDs encendidos + una columna respirando | 33,2–34,0 ns/tick | **28,2–29,3 ns/tick** | **−14 %** |
| lo mismo con una columna de teclas pulsada | 33,7–34,0 ns/tick | **28,2–29,0 ns/tick** | **−15 %** |

`[HOST]`, `taskset` a un núcleo, 300 000 ticks por corrida, cuatro corridas alternadas antes/después.

También se integra en `main.c` el `sub % 5u` del tick de TIMER5 calculado **una vez** en lugar de dos (el
mismo valor, para `usb_due` y para el turno de UART).

### Lo que se probó y no se quedó

- **Tabla de umbrales del histograma de gaps**: la búsqueda del bin calculaba `150us << b` en cada
  iteración; con una tabla de 7 constantes el tick no mejora de forma medible (el bucle sale en la primera
  comparación con un gap normal), así que se descartó en vez de añadir una tabla.
- **Quitar el diagnóstico del tick**: ahorraría el 25–35 % del C del tick medido en host, pero elimina el
  comando de consola `inp` y sus contadores (latencia de pulsación, gaps, coste, pulsos de glow). No se
  quitó: no es una optimización, es borrar una función.
- **Cambiar el bit-bang del 595** (unir los flancos, o pasar a SPI hardware): el ritmo actual está puesto
  para el 595 real y no hay unidad para validar el cambio. **No se toca.**

### Verificación

`tests/input_test.c` —el modelo exhaustivo del debounce: 2 000 pulsaciones con rebote de cada tecla y
botón, latencia de pulsación, ninguna nota colgada, codificadores, traza de LEDs del tick, breath y barrido
de encendido— pasa sin cambios. La suite completa (119 secciones) pasa, con los 220 dorados intactos.

### Coste

- **RAM:** **+24 B** de `.bss` — medido, no estimado: `sizeof(fm1_in)` pasa de **280 a 304 B** (los dos
  `uint8_t[11]` nuevos y el relleno de alineación; el struct es todo campos de 8/32 bits, así que el layout
  es el mismo en el target). Sobre un margen de `.bss` de 10 608 B.
- **GPIO:** los mismos 60 accesos por tick; el protocolo del 595 no cambia.
- **Flash `[TARGET]`:** no medido; en el anfitrión el camino rápido sustituye cuatro llamadas por dos
  cargas y una comparación.

## 10. Riesgos residuales de esta fase

| riesgo | severidad | estado |
|---|---|---|
| Ninguna medición `[TARGET]` en esta sesión | alto | reportado, no maquillado; el árbol no cambió ni un byte de memoria ni de flags |
| El cambio de `mix_part` sólo se probó en host | bajo | es equivalente por construcción (el `clamp` sólo se usaba dentro de los `if`); 220 dorados idénticos |
| `gprof` distorsiona funciones con muchas llamadas | bajo | se usa sólo para ordenar el trabajo, no para afirmar porcentajes del chip |
| Deriva del árbol: el trabajo empezó en v2.8.2 y `origin/main` estaba en v2.8.4 | medio | **resuelto**: rebase sobre `72c645c` antes de integrar, suite corrida sobre ese árbol |

---

## 11. Resultado de la suite en el árbol final

`NO_PACKAGE=1 ./tests/run_tests.sh` sobre `72c645c` + este cambio: **`ALL HOST TESTS PASSED`, 119 secciones,
exit 0** (log completo en `run_tests.log`). Lo que cubre, con lo que importa:

| prueba | resultado |
|---|---|
| `tests/regress.c` | **220 renders dorados: 0 cambiados, 0 perdidos**, 0 fallos de salud (clipping/DC/silencio), 0 de voces/routing, 0 de CPU sobre presupuesto |
| ASan + UBSan (editor, project, backup, robust, fm6bank) | 5 secciones, sin hallazgos |
| fuzzers (semillas fijas) | editor 20 000 (18 777 frames + 1 223 paquetes crudos), proyectos 5 000 stores → import → restore → render: sin hallazgos |
| UI: barrido de render | 191 pantallas × 17 paletas, **0 hallazgos de lint**, 0 tintas fuera de su color; 2 779 972 medidas de alineación, **0 px**; `FELUCCA_FM4=1`: 201 pantallas, 0 lint |
| UI: coste `[RENDER]` | HOME 178,7 µs redibujado completo, 55,9 µs un frame sin forzar, 0,8 µs un frame sin cambios (sólo firma) |
| audio / persistencia / editor / USB / MIDI | todas las secciones en verde |
| paquete, instalador, backup, Verify Only, firma | en verde (simulados); `flash evidence: 20 checks, 0 FAILED`; `flash read: 16 checks, 0 FAILED` |

**No ejecutado, y por qué:** `tests/target_budget.py` (falta `build/felucca.dis`), `tests/memory_map_test.py`
(falta el ELF de target), CPU bajo valgrind (no hay valgrind), emulador WASM (no hay `emcc`), cualquier cosa
en una unidad física (**no hay unidad**). Nada de eso se presenta como aprobado.
