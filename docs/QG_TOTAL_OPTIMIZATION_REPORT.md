# QG — Informe de optimización total (fase 7)

**Fecha:** 12 de noviembre de 2026 · **Base:** `main` tras la fase 5 (`10646f5`)
**Regla de este informe:** cada número dice de qué clase es. Host x86 ≠ AC791N ≠ unidad. Lo que no se midió, se
dice.

---

## 1. Qué se pudo medir y qué no

| Herramienta | Estado | Qué permite afirmar |
|---|---|---|
| `tests/fx_bench` (nuevo) | **sí** | ns/muestra del bloque completo, host x86 -O2, repetible |
| `gprof` | **sí** (instalado) | reparto del tiempo por función, host |
| `tests/hostsim` (TRACKS) | **sí** | ns/muestra por motor con 8 notas, host |
| `tests/cpu_vg.py` + callgrind | **no**: no hay valgrind y **no hay root** para instalarlo (`sudo` bloqueado por `no_new_privileges`) | — |
| `perf` | **no instalado** | — |
| Build AC791N (`JIELI_TOOLCHAIN`/`AC79_SDK`) | **no** | nada de CPU/RAM/flash reales del chip |
| `emcc` | **no** | el WASM del emulador no se construye aquí |

Todo lo que sigue marcado "host" es una medida comparativa en esta máquina, útil para decidir entre dos versiones
del mismo código y **no** extrapolable al FM-1: el núcleo del dispositivo es otro procesador, otra frecuencia y
otro compilador (`-Os` por función, `tools/build.py:236`).

## 2. Estado inicial

- Suite: `ALL HOST TESTS PASSED` (94 secciones), 202 presets auditados, 220 renders dorados.
- Perfil del bloque de audio completo (host, gprof, `preset_audit_test` con `-pg`): **`fx_buses` 45 %** del tiempo,
  `mix_part` 10 %, `mix_block` 10 %, `rev_room` 10 %, `fm6_op_run` 10 %, `master_out` 5 %, `events_block` 5 %.
- Coste por motor, 8 notas sostenidas, preset más pesado de cada uno (host -O2, ns/muestra):

| Motor | Preset más pesado | ns/muestra |
|---|---|---|
| ANALOG | TUBE KEYS | **293,4** |
| WHEEL | TIDAL ORGAN | 248,7 |
| LOFI | FLOAT PAD | 238,7 |
| NOISE | REACTOR | 237,8 |
| PHASE | CRYPT BELL | 214,5 |
| VOICE | BLACK CHOIR | 183,0 |
| FM6 | AURORA FM | 171,6 |
| DRUM | TRAP 808 | 100,0 |

ANALOG es el más caro por voz; su filtro ya calcula los coeficientes **una vez por bloque** (`tsvf_coef` fuera del
bucle de muestras, `eng_analog.c:110`), que era la optimización grande que se podía esperar ahí y ya estaba hecha.

## 3. La optimización aplicada

**`fx_buses` recorría los tres buses dos veces.** La primera pasada tomaba los picos de chorus, delay y reverb
(para los VU); la segunda volvía a leer los mismos tres arrays para calcular `any` (¿algo enviado a los buses?), que
gobierna la compuerta de los buses (`fx_gate`). Pero `any` no necesita la segunda pasada: **|x| > 0 exactamente
cuando x ≠ 0**, así que `any = vin0 | vin1 | vin2` con los picos ya calculados da el mismo valor.

`fx_buses` es la función más caliente del camino de audio (45 % del bloque en el perfil de host), así que quitarle
un tercio de sus lecturas es donde más rinde un cambio de esta clase.

### Medida A/B intercalada (misma máquina, mismo binario de banco, 6 rondas, mejor de 3 por corrida)

| Ronda | antes (ns/muestra) | después |
|---|---|---|
| 1 | 135,7 | 126,1 |
| 2 | 125,5 | 125,7 |
| 3 | 126,3 | 126,5 |
| 4 | 158,4 | 125,6 |
| 5 | 127,0 | 125,9 |
| 6 | 127,5 | 126,4 |
| **mediana** | **127,2** | **126,0** |

**−0,9 % en host.** Es pequeño y lo es por una razón concreta: la pasada eliminada sólo hacía un OR por muestra.
Lo que sí se gana además es **una lectura menos de cada bus por muestra** (tres arrays × 32 muestras × 34 500
bloques/s) y menos presión sobre la caché, algo que en un núcleo más lento y con memoria más justa pesa
proporcionalmente más — pero eso **no está medido en target** y por eso no se afirma.

**Reproducible:** `./tests/fx_bench 6` (se construye en la suite, no se ejecuta en ella: mide, no aprueba).

## 4. Lo que se midió y se **descartó**

| Candidato | Medida / razón | Veredicto |
|---|---|---|
| `-O3` en el camino de audio | host: **174,6 ns/muestra** contra 124,3 de `-O2` (**+40 %**) | peor: se descarta. El target además compila `-Os` por función porque `-O2` no entra en XIP (`tools/build.py:46-48`) |
| División por muestra en el limitador del master (`fx.c`, `g = (LIM_T << 15) / lim_env`) | es una división entera por muestra, y sólo cuando el limitador actúa | **cambiaría el audio** (una tabla o Newton da otros bits): no se toca en una fase de optimización que promete cero regresiones. Candidato para una entrega que acepte cambio de sonido con dorados nuevos |
| Saltar el núcleo de reverb cuando la compuerta está cerrada | ahorraría CPU en patches secos | el estado interno quedaría congelado y al reabrir la compuerta aparecería una cola vieja: riesgo de click. Necesita un vaciado medido, no una optimización rutinaria |
| LTO / `-ffast-math` | sin build de target no hay forma de verificar tamaño ni comportamiento | no verificable aquí: **no se aplica** |
| Buffers de audio más grandes | bajaría el overhead por bloque | sube la latencia, y la latencia es un requisito (2,9 ms por mitad): no |
| Fusionar buses de FX que comparten línea | chorus, delay y reverb tienen líneas y estados distintos | sólo se podría compartir el buffer de *entrada*; el ahorro es de unos cientos de bytes y el riesgo de aliasing entre buses es real: no |

## 5. UI y controles

Sin cambios en esta fase: la UI ya está construida alrededor de no redibujar lo que no cambió
(`graph_signature`, `ui.head_sig`/`foot_sig`/`col[]`, la animación apagada en LOAD LITE/ECO con 50/70 ms de
mínimo entre redibujos, `drift_on()` excluyendo las rejillas densas). Los números de `ui_render` en la última
corrida: **187 pantallas × 17 paletas, 0 hallazgos de lint, 0 px de desalineación en 2 777 796 medidas**, y el
barrido completo corre en ~25 s en host con las 17 paletas.

Lo que **no** se hizo y por qué: no se redujo ninguna frecuencia de refresco ni se quitó información, porque sin
medición de fluidez en la unidad cualquier recorte de ese tipo sería cosmético o una regresión disfrazada de
optimización.

## 6. RAM, flash y regiones

**No se modificó ninguna distribución de memoria** en esta fase: no se puede verificar el efecto de mover datos
entre `.text`, `.rodata`, `.data`, `.bss`, RAMTEXT, NOINIT o los pools sin el build de target y su
`tests/target_budget.py`. Los únicos números de memoria de este repositorio siguen siendo los del último build
AC791N registrado en `docs/MEMORY_BUDGET.md` (imagen 531 204/581 632 B; `.data+.bss` 96 088/98 304 B; pool spare
9 248 B), y no se presentan como medidos en esta sesión.

El cambio de esta fase **no agrega RAM** (una variable local de más en `fx_buses`, que ya existía) y **agrega 0 B
de flash** (menos código, de hecho: un bucle eliminado).

## 7. Seguridad y compatibilidad

- Las protecciones de los PR #86–#89 no se tocaron: detección de los dos JEDEC (`0x856014` Puya, `0xB36014` UCUN),
  validación de paquetes, comprobaciones antes de borrar y regiones protegidas siguen verificadas por la suite
  (`flash evidence: 20 checks, 0 FAILED`; `flash read: 16 checks, 0 FAILED`; `package bounds: all checks passed`).
- No se modificó el bootloader, el SPL ni el mapa de flash.
- Ambos JEDEC siguen en la allowlist y sus pruebas pasan; ninguna optimización de esta fase los roza.

## 8. Regresiones

**Ninguna**: el cambio es equivalente por construcción (`|x| > 0 ⟺ x ≠ 0`) y la suite completa —incluidos los
**220 renders dorados**, `reverb_test` (ROOM bit a bit), `dly_echo_test`, `fx_probe`, `fx_meter_test`,
`preset_audit_test`— pasa.

## 9. Deuda de las fases anteriores (documentada, no acumulada en silencio)

| Pendiente | Dónde está escrito | Qué falta |
|---|---|---|
| **KTR** de ANALOG (A1) | `docs/QG_ENGINE_AUDIT.md` §9 | implementar y medir con build de target |
| **STRING / ADD / MATH** | `docs/QG_NEW_ENGINES.md` | STRING espera la decisión de memoria (piso de 8 KiB o 8 voces) |
| **Flanger**, **DJ filter**, compresor | `docs/QG_FX_AUDIT.md` §5 | implementación (flanger es la más barata) |
| **Vistas de HOME** performances/mixer/modulación/engine | `docs/QG_HOME_VIEWS.md` §5 | cuatro entregas |
| **Unificar medidores** HOME ↔ FX | `docs/QG_FX_AUDIT.md` §4 | decisión de diseño |
| **Revisión de presets** con las herramientas nuevas | `docs/QG_FX_AUDIT.md` §6 | depende de las entregas de arriba |
| **Escucha real** de los 202 presets | — | unidad + oído; nunca se afirmó |

## 10. Límites de este informe

1. **No hay números de AC791N en esta sesión.** CPU, RAM, flash y latencia del chip no se midieron: no hay
   toolchain. Cualquier cifra de dispositivo citada aquí es de un build anterior registrado en `docs/`.
2. **El −0,9 % es host x86.** En un núcleo escalar más lento la proporción de una pasada de memoria sobre el total
   suele ser mayor, pero eso es una expectativa, no una medida, y por eso no se traduce a un porcentaje del FM-1.
3. **No hay valgrind/callgrind** (sin root), así que no hay conteos exactos de instrucciones: el reparto por
   función es de `gprof`, con la resolución de 10 ms que eso implica.
4. **No se optimizó lo que no se pudo verificar**: ni memoria, ni flags del compilador, ni el limitador, ni la
   reverb en reposo. Quedan como candidatos con su razón escrita, no como mejoras inventadas.
