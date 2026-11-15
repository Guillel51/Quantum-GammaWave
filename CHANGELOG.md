## Sin publicar — 2026-11-15 — FASE 3: las placas son degradados, y los suelos dejan de verse tramados

Los cuadros de la interfaz estaban pintados como una placa plana con una banda dura bajo el bisel, y los dos
suelos que sí tenían degradado lo tenían en 6 y 12 bandas anchas, mezcladas al 50 %: a 240×240 eso se lee como
un damero de píxeles, no como una luz que cae.

### Interfaz

- **Toda placa es un degradado** (`gfx.c`, `cv_plate_ground` y la rama *chamfer*): de una cresta apenas iluminada
  (7–14 % de `T_TEXT` mezclado) a un pie en sombra (58 % de `T_BG`), en bandas de 3 px. Llega a las 189
  pantallas por la primitiva que ya usaban todas: las tarjetas de knob, los módulos de FX > BUSES, los paneles y
  las filas de lista. **Las filas de lista (desde 16 px de alto) reciben ahora el mismo tratamiento**; antes solo
  lo tenían las cajas de 36 px o más, y por eso las filas se veían como barras muertas.
- **El degradado de la rama *chamfer*** (GAMMA / WRAITH / JIANT / QUANTUM): esa rama dibujaba la placa lisa entre
  dos líneas de 1 px, y es la que usan las tarjetas de knob del tema por omisión. Ahora también tiene su rampa.
- **HOME y FX > BUSES sin damero**: sus suelos pasan de 12 y 6 bandas al 50 % a **40 y 24 bandas sólidas**, así
  que la luz cae en pasos de 1–4 px sin el patrón de píxeles que se veía sucio.
- Nada nuevo en colores: todo sigue saliendo de `ux_mix()` sobre los tokens de la paleta, así que GREY y MONO
  siguen neutros (0 píxeles fuera de gris en el barrido) y el contraste no cambia.

### Corregido en la misma tanda

- **El degradado no entra en el panel de la gráfica.** Ese panel (HOME y las páginas de EDIT) pinta su propio
  suelo y su propio bisel justo después, así que la rampa de placa solo asomaba por los pocos px que el suelo
  del panel no repinta: la franja bajo el bisel y los 3 px de cada costado, justo donde el osciloscopio apoya
  la traza. El panel se declara **plano** (`cv_plate_flat`, `gfx.c`) y se queda como estaba: el área de la
  gráfica de HOME vuelve a ser **píxel a píxel idéntica** a la de antes de este cambio, en las 22 paletas.

### Coste (medido)

- El barrido completo `ui_render` (189 pantallas × 22 paletas) tarda **32,9 s antes y 32,8 s después**: la rampa
  nueva usa un relleno por banda de 3 px y solo repinta cuando el color cuantizado cambia, mientras que los dos
  suelos abandonan el tramado pixel a pixel. Se compensa; no hay regresión medible.

### Verificación

- `ui_render`: 189 pantallas × 22 paletas, **0 hallazgos de lint**, 0 tintas sobre su propio color, 0 píxeles
  GREY / MONO fuera de gris, y **3 400 410 mediciones de alineación con 0 desalineaciones**.
- `theme_test`: todas las paletas pasan; el peor margen de contraste es **4,08:1** contra el mínimo de 4,00.
- Los goldens de audio (`tests/golden.txt`) no se tocan: son de sonido, no de pantalla, y siguen 293/0.

### Pendiente

- **Subir la versión y publicar no se hizo**: `FELUCCA_VERSION` sigue en `v4.1.0` a propósito. El paquete
  `.fwsc`, su manifiesto y `release/SHA256SUMS` solo se generan con el build del AC791N, y este entorno no tiene
  `JIELI_TOOLCHAIN` ni `AC79_SDK`; el emulador necesita `emcc`, que tampoco está. Subir la versión sin esos
  artefactos dejaría el árbol y el paquete publicados en desacuerdo, que es justo lo que `coherence_check.py`
  prohíbe.

## Sin publicar — 2026-11-15 — FASE 1: 1 KB de RAM de vuelta, con las tablas idénticas

La campaña de optimización de esta fase empieza por el margen que estaba más apretado. En la entrega anterior
el `.bss` quedó en 97 380 de 98 304 B: **924 B libres**. `wt_gen_init` gastaba ahí un array de 1 024 B que solo
usaba una vez, al arrancar, para normalizar las tablas del WAVETABLE al mismo pico.

### RAM

- **`wt_buf` desaparece** (`eng_wavetable.c`): ahora se recorre cada frame dos veces, la primera para hallar su
  pico y la segunda para escribir los valores normalizados. El array era `.bss` permanente para un trabajo de
  arranque; el `.bss` de la unidad de compilación baja **1 024 B** (1 056 B en el binario, con la alineación) y
  los 924 B libres pasan a unos **1 948 B**.
- **Las tablas salen idénticas**: es la misma aritmética entera, solo repetida. Medido: el hash FNV de
  `WT_GEN` + `WT_L5` es `c1b4453e` antes y después, y los **293 renders golden siguen con 0 cambios**, los 22
  del WAVETABLE incluidos.

### Coste

- `wt_gen_init` en host x86: **172–183 µs → 325–337 µs** por llamada (+~0,15 ms), una sola vez en el arranque y
  nunca dentro de un bloque de audio. La cifra del AC791N no está medida (falta `JIELI_TOOLCHAIN`); a 320 MHz se
  espera del orden de 1,5 ms, y eso es una estimación, no una medición.

### Lo que se midió y no se tocó

- El regress da **ns por preset** en host. Las filas más caras son del WAVETABLE (90–118 ns) y superan a ANALOG
  (50–70 ns) en lo que cuestan sus dos frames por muestra; revisado el bucle, las lecturas ya están hoisteadas
  y no queda trabajo redundante que quitar sin cambiar el redondeo (y con él el sonido).
- El ruido del VM compartido es de ±10 % entre corridas: cualquier microoptimización por debajo de ese umbral no
  es demostrable aquí, así que no se declara.
- La medición de instrucciones del target (`tests/target_budget.py`) sigue bloqueada: necesita
  `build/felucca.dis`, y sin `JIELI_TOOLCHAIN` / `AC79_SDK` no hay build del AC791N.

### Estabilidad

- **`mflasher_test.py` dejó de ser intermitente** (fallaba 1 de cada 5 corridas). La causa no era la prueba sino
  el reloj: el nombre del archivo que escribe el importador lleva la hora UTC con granularidad de segundos, así
  que dos importaciones seguidas a veces caían en nombres distintos y no había nada que rehusar. El importador
  acepta ahora `--stem` para fijar ese nombre, y la prueba lo usa en las dos importaciones: sigue exigiendo el
  rechazo (`code == 1`, "already exists") y el hash intacto, pero ya no depende del segundo en que corra.
  10 corridas seguidas, 10 en verde.

Pruebas: `NO_PACKAGE=1 tests/run_tests.sh` completo; `wavetable_test` (13 ns por muestra y voz en host, dentro
de presupuesto); regress 293/0/0.

## Sin publicar — 2026-11-15 — modulación de fase en dos motores, sin parámetros nuevos

Dos motores estrenan una interacción nueva entre sus fuentes sin añadir un control que no suene y sin tocar el
formato guardado: los valores nuevos van al final de cada enum, así que un sonido viejo carga igual.

### Síntesis

- **ANALOG: WAVE = PM** (onda 11, `eng_analog.c`). El oscilador 2 (una sierra) pliega la fase del 1 antes de la
  mezcla: FM con los dos osciladores que el motor ya tiene. El índice es **MORPH** más ENV / LFO → **SHAPE**
  (0,78 rad con MORPH en 0 hasta ~4 rad) y la relación entre ambos, **INT** / **DTUNE**, como SYNC y RING (mismo
  despacho, `analog_ext`). Sin parámetro nuevo: lo que en SYNC es el reinicio y en RING el producto, en PM es la
  fase.
- **PHASE: LINE = PM** (modo 3 de LINE, `eng_phase.c`). La línea 2 pliega la fase de la 1 antes de la forma de
  onda; el índice sigue a **DCW**, así que el knob y ENV → DCW lo abren hasta ~1,5 rad. La línea 2 se calcula
  ahora antes que la 1: MIX y RING dan los mismos samples que antes.
- **La pantalla no miente**: la vista previa de MORPH dibuja la fase plegada y la marca del modo W2 lee `PM`.

### Pruebas (clase: host x86; no es una medición del AC791N ni del FM-1)

- **`tests/pm_test.c`** (nuevo, en la suite): sobre el DSP real, 8 comprobaciones. ANALOG PM difiere de sí mismo
  con MORPH 0 y 127 en **1,53× la señal** y su brillo (RMS de la diferencia primera sobre RMS de la señal) sube
  de **0,048 a 0,345**; PHASE PM difiere de MIX en **1,31×** con brillo **0,144 → 0,799** al mismo DCW; ninguno
  coincide con SIN, MIX ni RING; picos **13 946** y **11 764**, muy por debajo del bus (2^26).
- **`tests/six_live_golden.txt` regrabado** (61 hashes, `SIX_LIVE_ACCEPT=1`), y el motivo es uno solo: la
  escena barre `trk[0].p[P_E0]` de 0 a 127 y el motor lleva al máximo del enum todo lo que lo pasa; ese máximo
  es ahora PM y antes era FOLD. Dos corridas seguidas dan 61/61. El render de imágenes no se movió.
- `web/editor.html`: las tablas del editor llevan `PM` en WAVE y en LINE (la suite las compara con el
  firmware y fallaba con 2 diferencias hasta corregirlas).
- `engine_param_test`: 216 knobs × 3 presets, 0 inertes. `morph_audible_test`: 21 casos, 0 fallos.
  `phase2_test`: PASS.
- `tests/regress.c`: **293 renders golden, 0 cambiados** (ninguna página ni preset de fábrica usa PM todavía),
  0 fallos de salud, 0 de voces/routing, 0 cuelgues; CPU **PASS** (mediana del host 1,06, límite agregado
  +20 %, 0 filas fuera de presupuesto).

### Pendiente y límites

- La imagen del target **no se midió**: `JIELI_TOOLCHAIN` y `AC79_SDK` no están en este entorno, así que el
  `.fwsc` y su tamaño quedan sin comprobar. El margen documentado tras la entrega del WAVETABLE es de 5 852 B.
- Ningún preset de fábrica usa PM todavía: la onda existe y se elige en EDIT > WAVE, pero agregar un sonido de
  la casa sin poder escucharlo sería declarar un timbre que nadie oyó.

## Sin publicar — 2026-11-15 — el flash que faltaba: la tabla del WAVETABLE se genera en el arranque

La región XIP estaba en 583 600 de 583 612 B: doce bytes. La vía de comprimir assets se midió y estaba
cerrada (ver `docs/FLASH_RECLAIM.md`), así que esta entrega cambia el modelo: guardar menos y calcular más.

### Memoria

- **WAVETABLE: dos de sus tres límites de banda se construyen al arrancar** (`wt_gen_init`,
  `eng_wavetable.c`). Lo guardado eran 8 formas × 256 puntos por nivel; ahora se guardan las amplitudes
  armónicas de esas 8 formas en Q30 (2 048 B) y la síntesis —64 MACs por punto, suma de senos con la tabla
  `SINE`— corre una sola vez, antes de la primera nota, también desde `felucca_init`. El nivel de 5 armónicos
  (el que leen las notas agudas) sigue guardado tal cual.
  - Flash: **imagen 583 600 → 577 760 B, 5 840 B liberados** (código +292 B, constantes −6 132 B).
  - RAM: `.bss` 88 148 → 97 380 B de 98 304 (8 192 B de tabla generada + 1 024 B de buffer).
  - Sonido: la onda generada difiere de la guardada en **3 LSB de 32 000 (0,0094 %, ≈ −90 dB)**, medido en
    host x86 comparando ambos enteros punto por punto. Cambiaron exactamente los **22 goldens de WAVETABLE**;
    ningún otro motor se movió. `tests/golden.txt` queda rebasado con esos 22.

### EDIT

- **DETUNE estrena página** (VOICE 3): `P_DETUNE` repartía las voces del unisono hasta ±40 cents en
  `voice.c` desde siempre y ninguna página lo ofrecía — la v4.1.0 lo documentó como imposible por falta de
  flash. Con los 5 840 B liberados entra: cuesta **24 B** y se lee con el asignador (ALLOC) de VOICE 2.
  Barrido de renders: 189 pantallas × 22 paletas, 0 hallazgos de lint, 0 desalineaciones; 293 goldens sin
  cambios (el valor por omisión, 40, ya estaba en efecto).

## v4.1.0 — 2026-11-15 — un solo espacio de paleta, y la suite en verde

Cierra lo que dejó abierto la retirada de AMBER, ICE y VIOLET, expone un control que el DSP leía sin que
ninguna página lo ofreciera, y deja `NO_PACKAGE=1 tests/run_tests.sh` y `web/test_web.mjs` pasando enteros.

### EDIT

- **VOICE 2 estrena ALLOC** en el cuarto knob, que estaba vacío (`0xFF`): `ROT` rota la voz más vieja,
  `REUSE` retriggerea la misma. El asignador lo leía `voice.c` desde siempre y ninguna página lo ofrecía.
  Coste: **0 B de flash y 0 B de RAM** — ocupa un hueco ya declarado.
- **DETUNE sigue sin página**: `P_DETUNE` existe y los motores lo leen, pero exponerlo pide una página
  nueva y la región XIP no tiene lugar (medido: el linker rechaza una página más con 12 B libres).

### Interfaz

- **Un knob que no hace nada ya no se anuncia.** En el layout de tarjetas altas, una tarjeta vacía dibujaba
  igual la píldora `K1`..`K4` en `T_DIM`: una etiqueta para un control inexistente. Ahora retorna antes de
  la píldora, y además **libera 16 B de XIP** (imagen 583 600 → 583 584 B en su momento).
- **PAPER pasa su propio contraste.** `T_MID` sobre `T_SURF` daba 3.87:1 donde el proyecto exige 4.00, y al
  corregirlo apareció el acento a 3.90 sobre 4.50. Superficie `(226,218,202)` → `(232,224,208)` y acento
  `(172,56,8)` → `(150,44,0)`, en el generador. `tests/theme_test` recorre los 22 pares en las 22 paletas.

### Paletas: un solo espacio entre MENU, el editor y la Web Studio

- `menu_items.c` habla en **ranuras de la lista viva** (`UI_PAL_LIVE`) y `editor_preferences.c` asignaba el
  **índice crudo** de la tabla. Con dos espacios conviviendo, `UI_SET` podía dejar el instrumento en un tema
  retirado y la comparación de "preferencia sin cambios" no coincidía nunca: el editor reescribía la flash
  en cada pasada.
- Ahora `UI_SET` (id 0) toma la ranura y **guarda el índice de tabla**; `UI_STATE` devuelve **la ranura**,
  nunca un id retirado. La lista de respaldo del Web Studio pasa a las 19 paletas vivas y su estado inicial
  usa la ranura de QUANTUM; `web/EDITOR_PROTOCOL.md` lo documenta para los dos comandos.
- `tests/persistence_test`, `tests/editor_test`, `tests/settings_test` y `tests/theme_test` actualizados a la
  especificación de hoy (ranura ≠ índice, 19 paletas vivas, PAPER con su contraste).

### Pruebas

- `NO_PACKAGE=1 tests/run_tests.sh`: **ALL HOST TESTS PASSED**.
- `node web/test_web.mjs`: **web tests passed**.
- `tests/run_tests.sh` crea `build/capture` antes de escribir ahí: en un `build/` limpio el primer error era
  un `sed` sin directorio.

## v4.0.0 — 2026-11-15 — el instalador enciende el instrumento

La página del instalador arranca el firmware y lo dibuja a su propio reloj; el estudio de optimización del
dibujado queda medido, y se explica por qué no entra todavía.

### Instalador: la secuencia de arranque, no una imagen

- **La página del instalador enciende el instrumento.** Carga el mismo `felucca.wasm` que publica el emulador, lo
  arranca con las mismas llamadas que usa su worklet (`web_nor_erase`, `web_master`, `web_seed`, `web_boot`) y
  dibuja el panel de 240 × 240 con `web_render(n)`, que empuja el reloj del equipo muestra a muestra: el splash,
  el barrido de la tira de LEDs y la pantalla a la que llega, a velocidad real, con control para repetirlo. Antes
  esta página no dibujaba el panel en absoluto: no había canvas ni imagen animada, sólo una cabecera.
- **Comprobado sobre el binario que se publica:** arrancado en Node, ocho instantes del arranque (0, 400, 1200,
  2500, 3500, 4530, 5000 y 6500 ms) dan **ocho fotogramas distintos** — el splash se mueve y termina en HOME — y
  los 7 s de arranque se ejecutan en 117 ms de reloj real (**60× tiempo real**), que es lo que permite correrlos
  sin cargar el equipo.
- El reloj se detiene al llegar a HOME, se puede **repetir**, y no corre si la tarjeta está fuera de pantalla o
  la pestaña oculta. Si el wasm no está junto a la página, la tarjeta no aparece: no hay imagen de reemplazo.
- La CSP de la página suma `'wasm-unsafe-eval'`: sin eso Chromium no compila el módulo y la tarjeta quedaba
  oculta sin decir nada. Sigue siendo una única script inline con su hash.
- No se tocó el splash del firmware ni el del emulador: es el mismo binario, dibujado en otro sitio.

### Optimización del dibujado: medida, y por qué no entra

- El trabajo más caliente de la pantalla es `ux_mix` (cada degradado, cada glifo con antialias, cada vúmetro) y
  redondeaba tres canales por llamada con una **división por 100**; el AC791N no tiene instrucción de división.
  Reescrito como producto y desplazamiento — **exacto para todo |d| ≤ 43 599**, verificado valor por valor — el
  barrido completo de render baja de **79,4 s a 26,5 s** (26,5 s y 26,7 s en dos pasadas) para las mismas 189
  pantallas × 22 paletas, y `home` pasa de 584,7 µs a 131,8 µs (4,4×) con el informe del barrido idéntico (0
  lint, 0 tintas sobre su propio color, 0 píxeles fuera de gris/neutro) y sin cambiar un píxel.
- **No está en esta versión, y con razón: la medición era del banco, no del chip.** El pi32v2 **sí tiene
  división por hardware** (`r0 = r0 / r10 (u)` en `build/felucca.dis`), así que en el firmware el compilador
  nunca llamaba a un ayudante de división: los 3× venían de que el banco de render se compila con `-O1`, donde
  la división sí es una llamada. Sobre el objetivo el cambio **cuesta 16 B de XIP** (con 12 B libres, el link
  pasa a 4 B de más) y no quita ni una instrucción. La tabla de 257 entradas pide 244 B. Ninguna de las dos
  entra, y ninguna de las dos era la mejora que parecía: ver `docs/PERFORMANCE_BASELINE.md` §3.

### Pendiente, dicho

- El reequilibrio de niveles del banco de presets y el relleno degradado de EDIT > FILTER 4/6 siguen sin hacer:
  no hay unidad ni salida de audio real en este entorno y no se declaran niveles medidos sin medirlos.
- `release/SHA256SUMS` no listaba la v4.1.0 (estaba hasta v4.0.0). Corregido: se agregaron sus tres archivos con hash verificado.
- Attestation de GitHub: implementada en `release.yml` y `tag-and-publish.yml` (workflow reutilizable de `zvenson/fm1-build` fijado a un commit). No verificable aquí: `gh` 2.45 no tiene `attestation` y no hay sesión.

