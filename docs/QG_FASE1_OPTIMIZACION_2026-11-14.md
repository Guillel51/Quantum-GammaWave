# QG — FASE 1: optimización global (medida sobre el árbol real)

**Fecha:** 14 de noviembre de 2026 · **Base:** `main` = `0f69fca` (v3.0.1) · **Rama:** `freebuff/175b9279ea697cf08b8dec0c`
**Clase de cada número, siempre:** `[TARGET]` build AC791N (toolchain y SDK fijados), `[HOST]` x86-64 de esta VM
(`cc -O2`), `[RENDER]` coste de dibujo en host, `[PROFILE]` reparto por función en host. **No hay `[FÍSICO]`:
en este entorno no existe ninguna unidad FM-1.** Ninguna cifra de este documento es una medición de hardware.

---

## 0. Veredicto en una página

1. **El bloqueo que arrastraban las fases anteriores está cerrado en esta sesión.** La toolchain JieLi
   `jieli-linux-toolchains-20260730.1` (clang 4.0.1, `pi32v2`) se descargó y su SHA-256 coincidió con el pin
   `tools/toolchain.sha256`; los tres blobs del SDK `AC79NN_SDK_V1.2.1_2023-12-13` coincidieron con
   `SDK_SHA256` de `tools/build.py`. **Hay build `[TARGET]` real**, `.dis`, ELF, presupuesto estático, mapa de
   memoria y paquete. Ya no hay que citar cifras de builds anteriores.
2. **Los cuatro presupuestos que pedía el encargo se re-midieron con el toolchain actual y coinciden con lo
   declarado.** `phase_render` 84/84, `wheel_block` 32/86, `noise_render` 219→**218**/201, `fm6_render` 292/322.
   `tests/target_budget.py` termina en **0** (todas las líneas dentro del +10 % declarado, ninguna baseline
   tocada).
3. **`noise_render` baja 1 instrucción y no llega a 201.** No es viabilidad: es que el bucle ya está en su
   suelo de codegen (§3). Se documenta el motivo con el desensamblado, no se maquilla la cifra.
4. **Memoria: 0 B movidos.** No se añadió ni se reordenó ningún buffer. El mapa completo está medido (§4) y
   hay un dato que merece atención: **NOINIT tiene sólo 124 B libres**.
5. **UI: no se tocó.** El árbol ya evita el redibujado completo por firma (`0,84 µs` un frame sin cambios) y
   las animaciones que quedan son reactivas a audio/interacción. El coste medido está en §5; recortar más
   exigiría quitar información de pantalla, que no es una optimización.
6. **Suite de host completa: `ALL HOST TESTS PASSED`, exit 0**, 291 dorados, 0 cambiados (§7).

**Lo que no se hizo y por qué:** no hay unidad física (ni latencia, ni CPU real, ni estabilidad de audio
medidas), no hay `emcc` (no hay WASM nuevo), y no se cambió ningún sonido, ninguna baseline ni ninguna
compuerta de instalación. La instalación sigue **NO-GO**.

---

## 1. El árbol de partida: pendientes revisados antes de optimizar

Regla de entrada del encargo: resolver lo pendiente antes de optimizar. Revisado, uno por uno:

| Pendiente heredado | Estado real en este árbol | Acción |
|---|---|---|
| `run_tests.log` con una corrida fallida | **no**: el log versionado es de una corrida verde (`ALL HOST TESTS PASSED`, `EXIT=0`) | nada |
| `git status` con trabajo a medias | **limpio** al empezar | nada |
| Build AC791N / `.elf` / `.dis` / presupuesto | **bloqueado** por falta de toolchain y SDK | **cerrado en esta sesión** (§2) |
| Cifras `[TARGET]` citadas de builds viejos | **sí**, en `docs/` y en el README | re-medidas aquí (§2–§4) |
| `noise_render` sobre su presupuesto estricto | **sí**: 219/201 (+9 %, dentro del +10 % de la herramienta) | intento real, −1 instrucción (§3) |
| README: imagen «569 048 B, 14 564 B de room» | **no coincide** con el árbol: la imagen actual mide **574 572 B** (9 040 B de room). 569 048 B es la imagen de v2.9.7 | **corregido** en este commit |
| NOINIT con poco margen | **124 B libres** (medido, §4) | reportado, no tocado |
| `emcc` / WASM / navegador | no instalado | reportado como no ejecutado |
| Unidad física | no existe | reportado; nada se declara verificado en hardware |
| **`tools/coherence_check.py` en rojo** | **sí, 3 problemas** (versión/artefactos, ver §10) | **reportado, no maquillado**: cerrarlo exige un build de release + WASM, fuera del alcance de una fase de optimización |

No se encontraron regresiones abiertas, ni tests en rojo, ni buffers muertos demostrables.

---

## 2. Instrumentos: cómo se ha medido `[TARGET]`

```sh
sh tools/get_toolchain.sh                       # pin sha256 a1545cdd…7647bd verificado
# 3 blobs del SDK, hash comprobado contra SDK_SHA256 de tools/build.py
ulimit -n 9000 && ./build.sh                    # imagen, loader, .fwsc, manifest, .elf, .dis
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
python3 tests/memory_map_test.py
python3 tools/ram_report.py
```

| pieza | valor medido |
|---|---|
| toolchain | `jieli-linux-toolchains-20260730.1`, `pi32v2/bin/clang` = **clang 4.0.1** |
| SDK | `AC79NN_SDK_V1.2.1_2023-12-13`; `uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin` **coinciden** con `SDK_SHA256` |
| avisos de SDK | **0** (el build no avisa de que el SDK difiera del fijado) |
| perfil de compilación | `-Oz` + `minsize` en todo el TU de audio (`attributes #0` incluye `minsize`); **no** es un árbol a `-Os` |

---

## 3. DSP y CPU: lo que se midió y lo que se cambió

### 3.1 Presupuesto estático del target, re-medido (`-Oz`, `pi32v2`) — `[TARGET]`

`tests/target_budget.py` cuenta las instrucciones dentro de los bucles de cada función, con peso ×4 por nivel
de anidamiento y ×9 por división hardware. Es un **conteo estático, no de ciclos**.

| función | antes (declarado) | medido hoy | presupuesto | estado |
|---|---:|---:|---:|---|
| `phase_render` | 84 | **84** | 84 | ok (clavado) |
| `phase_render_noise` | 103 | **103** | 103 | ok |
| `phase_harm` | 23 | **23** | 23 | ok |
| `wheel_block` | 32 | **32** | 86 | ok (−63 %) |
| `noise_render` | 219 | **218** | 201 | +8,5 % (dentro del +10 % de la herramienta) |
| `fm6_render` | 292 | **292** | 322 | ok |
| `formant_render` | 238 | **238** | 229 | +3,9 % (dentro del +10 %) |
| `analog_render` | 228→138 | **138** | 148 | ok |
| `slicer_bus` | 806 | **806** | 801 | ok |
| `fm1_alnk0_irq` (ISR de audio) | 291 | **291** | 270 | ok (+7,8 %, dentro del +10 %) |
| `fm1_timer5_irq` (teclas/LEDs, 10 kHz) | 26 811 | **26 811** | 27 338 | ok |
| `perf_block` / `rev_room` / `DRUM_PATH` | 254 / 243 / 1153 | **254 / 243 / 1153** | 253 / 243 / 1153 | ok |
| `uac_tap48` | 465 | **465** | 504 | ok |

`python3 tests/target_budget.py …` → **exit 0**. Ninguna línea de `tests/target_budget.txt` se reescribió
(`BUDGET_UPDATE` no se usó).

### 3.2 `noise_render`: por qué 201 no es alcanzable hoy, con el desensamblado en la mano

El bucle es **un único bloque recto de 218 instrucciones, sin bucles anidados** (todo el coste pesa ×1). El
compilador **ya hizo** el trabajo que se suele proponer a mano:

- Las divisiones constantes están fuera del bucle: `crsh / 8`, `crsh / 11`, `banda * 258`, `hpa * 258`,
  `bpg * 3` y `co2 = clamp(g - banda*20, …)` se calculan **una vez por bloque** y viven en slots de pila
  (`[sp+36]`, `[sp+40]`, `[sp+56]`, `[sp+60]`, `[sp+68]`, `[sp+76]`).
- `mulq15` está inlineado (shifts en el bucle, no llamadas).
- Sólo quedan **2 llamadas por muestra** (`soft_knee`, `voice_amp`), ambas `static inline` que este clang
  decidió no inlinear a `-Oz`.

Lo que queda son **etapas opcionales con guarda constante por bloque** — COLR, BAND, el DC-blocker de META,
CRUSH y HPF — más el despacho de MODE. La herramienta cuenta **la unión de todos los caminos**: una
instrucción detrás de `if (crsh)` cuenta igual que una del SVF, se ejecute o no con el patch cargado. Bajar de
201 exigiría una de estas dos cosas, y ninguna es una optimización:

1. **Duplicar el bucle** por combinación de etapas activas: quitar las 5 guardas y el despacho de MODE pide
   hasta 16 versiones del cuerpo (~150 instrucciones cada una) en XIP; es crecimiento de firmware, no ahorro.
2. **Quitar una etapa** (BAND, HPF, CRUSH o EVOL son funciones del motor NOISE): cambia el sonido y/o elimina
   una función, que es exactamente lo que el encargo prohíbe.

**Lo integrado (único cambio de DSP de esta fase), exacto por construcción:**

- **LFSR: la prueba de desbordamiento sin guardar la fase anterior.** `o = ph; ph += inc; if (ph < o)` pasa a
  `ph += inc; if (ph < inc)`. En aritmética unsigned, `ph + inc` desborda exactamente cuando
  `ph_nuevo < inc`; es la misma condición, mismo estado, mismos bits.
- **META: la tabla del periodo se lee una vez por bloque**, no por muestra (`seqw`, `seqlen`, `seqhi`,
  `seqlo` fuera del bucle), en lugar de indexar `NOISE_SEQ[sel]` en cada iteración.

| medida `[TARGET]` | antes | después |
|---|---:|---:|
| `noise_render`, instrucciones en el bucle | 219 | **218** |
| `.text` (XIP code) | 404 196 B | **404 192 B** (−4) |
| constantes XIP | 149 908 B | **149 896 B** (−12) |
| imagen | 574 588 B | **574 572 B** (−16) |

**Lo que esto es y lo que no es:** es una reducción real y verificada, **no** una solución al presupuesto
estricto. 218/201 sigue siendo +8,5 %. No se reclama ninguna mejora de CPU del chip: no hay unidad.

### 3.3 Caminos que se perfilaron y no se tocaron

- `mix_block` / `mix_part` / `fx_buses` ya vienen de fases anteriores con una sola pasada, coeficientes por
  bloque, estado propio por bus y cero `malloc`; el reparto de trabajo de `mix_block` es el correcto:
  **primero se lanza el núcleo 1** (`mc_split` + `mc_run` con las partes 4–6 y los buses), **después** el
  núcleo 0 hace sus partes, y **sólo al final** `mc_wait()`. La espera es la cola del trabajo, no una espera
  gratuita. Un núcleo 1 que falla o lee mal el buzón se descarta para siempre (`mc_lost`) y el bloque sigue en
  un núcleo.
- Los `clamp()` del SVF (±150 000) son la garantía de estabilidad del filtro: **no se quitaron**. Demostrar
  que nunca disparan exigiría cotas que el código no tiene.
- Inlinear `soft_knee`/`voice_amp` **reduciría ciclos reales** (2 llamadas por muestra) pero **subiría** el
  conteo estático del bucle y por tanto rompería la puerta: es el punto ciego de la métrica, y falsearla para
  "mejorar" no es una opción; se deja anotado como candidato para cuando exista unidad.

---

## 4. Memoria — `[TARGET]`, medida sobre `build/felucca.elf`

**No se movió ni un byte.** Tabla del build de este commit:

| región | usado | capacidad | margen |
|---|---:|---:|---:|
| `.data` | **764 B** | — | — |
| `.data + .bss` | **87 956 B** | 98 304 B | **10 348 B** |
| POOL (`pool_top` → `POOL_END`) | 305 096 B | 327 680 B | **15 392 B** (suelo 8 KiB) |
| LOWBUF (`proj_scratch`, `sec_stage`, `undo_st`) | 26 532 B | 28 672 B | 2 140 B |
| Código en RAM (`.ram_text` + `.c1_text` + `.hot_text`) | **19 720 B** | 24 576 B | **4 856 B** |
| XIP (código + constantes) | 404 192 + 149 896 = **554 088 B** | 581 052 B | 26 964 B |
| NOINIT | **15 572 B** | 15 696 B | **124 B** ← aviso |
| `app.bin` | **574 572 B** | slot 583 612 B | **9 040 B** |
| `ota.bin` (loader) | 7 803 B | — | — |
| paquete `.fwsc` | **610 959 B** (identidad `FM-1_965`) | 1 MiB NOR | imagen 602 112 B + wrapper 8 847 B |

Reparto del código en RAM (13 símbolos, 19 716 B nombrados): `mix_block` 6 928 B, `mix_part_m0` 3 826 B,
`fx_run0` 3 044 B, `fl_prog_ram` 1 172 B, `fl_erase4k_ram` 1 100 B, `perf_block` 932 B, `slicer_bus` 762 B, …
NOINIT son 8 símbolos, y `proj_slot` solo son **15 360 B**.

**Lo que se revisó y no se tocó, con motivo:**

- No hay símbolos grandes sin referencia: `cloud_buf` (32 KiB), `sl_buf` (32 KiB), `dly_buf` (128 KiB),
  `cv_buf` (59 520 B), `comb_buf`, `rev_comb`, `reel` los usan motores y buses que están en el build.
  `--gc-sections` ya está activo en el enlace: una sección sin referencia no llega al `.bin`.
- `POOL` tiene 15 392 B pero el suelo de 8 KiB es contractual: el margen utilizable real es ~7 KiB.
- LOWBUF son exactamente los tres buffers de scratch y sus vidas no son demostrablemente disjuntas.
- **NOINIT: 124 B libres** es el margen más fino del mapa. No se cambió nada (mover `proj_slot` sería un
  rediseño de la recuperación tras crash, no una optimización); queda **reportado** como riesgo.

---

## 5. UI y coste de render — `[RENDER]` (host; sólo valen las proporciones dentro de la tanda)

De `build/ui_new/report.txt` (barrido completo de esta sesión):

| pantalla | redibujado completo (µs) | textos+iconos | px de glifo leídos |
|---|---:|---:|---:|
| home | 173,6 | 34 | 6 276 |
| about_credits | **212,5** | 48 | 51 422 |
| mixer | 150,8 | 50 | 8 512 |
| fx | 147,4 | 49 | 10 174 |
| drumx | 136,4 | 34 | 6 561 |
| menu | 129,8 | 27 | 8 953 |
| presets / step / roll_chords / drum | 120–122 | 24–38 | 5 026–9 929 |
| reverb2 / master | 117–118 | 25–31 | 5 294–6 542 |
| confirm_project | 106,3 | 12 | 5 570 |

Y, lo que importa para el encargo, **el coste por frame ya no es un redibujado completo**:

| caso | medida |
|---|---:|
| HOME, un frame sin forzar (strips perezosos) | **54,2 µs**, 3 textos |
| frame sin cambios (sólo firma) — roll / drum | **0,84 / 0,33 µs** |
| frame de roll con BPM y TUNE cambiados (sólo strips) | 10,45 µs |
| el mismo cambio redibujando cabecera + 4 tarjetas enteras | 38,35 µs |
| splash, un frame escrito | 31,5 µs |
| visualizador de HOME | 92,6 µs HIGH / 87,2 µs LOW |

Lectura: **la optimización de UI que pedía el encargo ya está hecha y medida** (`head_sig`, `foot_sig`,
`ui.roll[].sig`, `ui.force`, `MENU > ANIM OFF`): un frame idéntico cuesta una firma, no un dibujo. No se
encontró cálculo repetido por frame que no estuviera ya cacheado por firma, y **no se recortó ninguna
animación**: las que quedan responden a audio, a la interacción o al transporte. Recortarlas sería
cosmético, no rendimiento. No se hizo ningún cambio de UI en esta fase.

---

## 6. Reparto entre núcleos

Sin cambios (el diseño vigente ya es el correcto y está probado en host):

- Núcleo 0: master, UI, pantalla, partes 1–3 y los buses si el núcleo 1 no está vivo.
- Núcleo 1: partes 4–6 y los buses, con su banco EMU y su buzón; los resultados se suman al final del bloque.
- Orden dentro del bloque: **lanzar el núcleo 1 → trabajar el núcleo 0 → esperar**. `mc_split()` decide el
  reparto con los tiempos de los bloques anteriores; `mc_lost()` retira para siempre un núcleo 1 que falla.
- La carga por núcleo **en el chip sigue NO MEDIDA**: el protocolo está ejercitado en host, no el tiempo real.

---

## 7. Verificación ejecutada en esta fase

| prueba | clase | resultado |
|---|---|---|
| `tools/get_toolchain.sh` | — | sha256 `a1545cdd…` = pin; `clang 4.0.1` |
| SDK (3 blobs) | — | 3/3 hashes = `SDK_SHA256`; 0 avisos de SDK en el build |
| `./build.sh` | `[TARGET]` | **ok**: imagen, loader, `.fwsc` `FM-1_965`, manifest v3.0.0 |
| `tests/target_budget.py` | `[TARGET]` | **exit 0**, 41 líneas, 0 fuera de tolerancia |
| `tests/memory_map_test.py` | `[TARGET]` | **passed**: regiones, NOINIT, pool, pila, slot |
| `tools/ram_report.py` | `[TARGET]` | RAM código 19 716 B nombrados, NOINIT 15 572 B |
| `NO_PACKAGE=1 ./tests/run_tests.sh` | `[HOST]` | **ALL HOST TESTS PASSED, exit 0** |
| `tests/regress` (tras el cambio) | `[HOST]` | **291 dorados, 0 cambiados**, 0 salud, 0 voces, CPU en presupuesto |
| `tests/noise_test` (tras el cambio) | `[HOST]` | **all checks ok** (LFSR, META, DC, clipping, retrigger) |
| `tests/ui_render` | `[RENDER]` | 189 pantallas × 22 paletas, **0 hallazgos de lint**, 3 390 046 medidas de alineación, **0 px** |

**No ejecutado, y consta como tal:** WASM (`emcc` no instalado), navegador, y absolutamente nada en una unidad
física. **No hay latencia, ni estabilidad de audio, ni CPU real del AC791N medidas.**

---

## 8. Archivos modificados

| archivo | cambio |
|---|---|
| `firmware/src/eng_noise.c` | LFSR: prueba de desbordamiento equivalente sin fase previa; META: tabla del periodo leída una vez por bloque. −1 instrucción en el bucle, −16 B de imagen |
| `README.md` | `noise_render` 219→218; imagen y margen del slot corregidos a lo medido (574 572 B de 583 612 B → 9 040 B), porque el texto citaba la imagen de v2.9.7 dentro de las notas de v3.0.1 |
| `docs/QG_FASE1_OPTIMIZACION_2026-11-14.md` | este informe |

Nada más: ni presets, ni tablas, ni baselines, ni `tools/`, ni el instalador, ni el sitio, ni el bootloader,
ni el mapa de flash. Cero bytes de RAM nuevos.

---

## 9. Riesgos residuales

| riesgo | severidad | estado |
|---|---|---|
| Ninguna medición física | alto | reportado; la instalación sigue NO-GO |
| `noise_render` +8,5 % sobre su presupuesto estricto | bajo | dentro del +10 % de la herramienta; el motivo está en §3.2 y no se subió la baseline |
| NOINIT con 124 B libres | **medio** | reportado; cualquier variable `.noinit` nueva lo rompe |
| El conteo estático no es un conteo de ciclos | medio | se usa para detectar crecimiento de bucles, no para afirmar carga del chip |
| `soft_knee`/`voice_amp` no inlineados (2 llamadas/muestra) | bajo | candidato real de CPU, pero subiría el conteo estático: se medirá con unidad |

---

## 10. Pendiente de release encontrado y **no** cerrado (evidencia exacta)

Regla de entrada del encargo: no acumular pendientes. Éste es el único que apareció y no se cierra aquí, y se
razona por qué:

```
$ python3 tools/coherence_check.py
coherence: FAILED (3 problem(s), 947 files scanned)
  - release/felucca-v3.0.0.fwsc is missing: the firmware claims v3.0.0 and release/ has no package for it
  - release/build-manifest-v3.0.0.json is missing
  - emulator record version 'v2.9.7' != firmware v3.0.0
```

Los tres son de **coherencia de release**, no de DSP, memoria ni UI, y **ya estaban en `0f69fca`**: no los
introduce ningún cambio de esta fase (mi cambio sólo toca `eng_noise.c`, `README.md` y este documento). El
árbol se contradice a sí mismo en la versión: el firmware define `FELUCCA_VERSION "v3.0.0"`, el README y el
último commit hablan de **v3.0.1**, y el último paquete real de `release/` es **v2.9.7**.

Cerrarlo de verdad exige: (a) decidir la versión, (b) `./build.sh --release X.Y` para producir paquete y
manifest, (c) reconstruir el emulador WASM (`emcc`, no instalado aquí) para que `versions.json` nombre la
misma build, y (d) sincronizar `site/`. Eso es **publicar una release**, no optimizar, y una release firmada
con un emulador que no se ha podido reconstruir sería exactamente el tipo de artefacto que `AGENTS.md` §4
prohíbe presentar como verificado. Se deja escrito, con el comando y la salida, para que se cierre en la fase
que corresponda. **No se tocó ninguna versión ni ningún artefacto de `release/`.**
