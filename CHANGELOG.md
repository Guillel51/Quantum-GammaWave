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

