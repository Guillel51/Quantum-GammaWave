# Arranque: el pop de encendido y la pantalla — v1.4.11 (2026-11-18)

**Clase de cada número:** build AC791N (pi32v2 + GNU ld) para tamaños; host x86 para los renders, el barrido
de UI y los tests de audio; emulador WASM (Emscripten 6.0.11) para la página del emulador. **Nada de esto es
una medición sobre la FM-1**: no hay unidad en este entorno.

## 1. El pop al encender

### Causa

`firmware/hal/fm1_audio.h` mantiene el estado de salida (pc6) en mute durante todo el arranque del códec y lo
desmutea recién al final de `fm1_audio_init`, 25 ms después de habilitar el motor sobre el buffer en ceros (la
secuencia de 1.0.2/1.4.2.1). El fade de encendido de `audio.c`, en cambio, arrancaba en el **primer bloque**:

- `BOOT_FADE` = 1 200 muestras ≈ 27 ms, avanzando 864 por bloque de 32;
- el desmute ocurre a los ~25 ms de la habilitación ⇒ la rampa ya estaba en ~93 % cuando el altavoz se abrió;
- lo que llegaba al parlante era el escalón que el fade existía para esconder.

Segundo camino, del mismo origen: el arranque del segundo núcleo corre con interrupciones apagadas hasta 20 ms
(`main.c`, `fm1_guard_unlock_top` … `fm1_mc_start`), siete medias de 2,9 ms. Con la salida desmuteada y la
rampa ya consumida, esas medias perdidas son un hueco en la señal — otro click.

### Corrección

- `audio.c`: `boot_armed` separa "armado" de "corriendo". Desarmado, `audio_block` entrega `fx_boot_gain(0)`,
  que multiplica el master por 0: **cero exacto**, no un valor pequeño.
- `main.c`: `audio_fade_in()` se llama **después** de las etapas con interrupciones apagadas (segundo núcleo
  incluido). Hasta ahí, el desmute cae sobre ceros y las medias perdidas se pierden sobre ceros.
- La rampa 0 → 32768 sigue siendo de ~27 ms y ocurre dentro del hold del splash: **no agrega tiempo de
  arranque** (medido en el código, no en la unidad: el splash dura SPL_WRITE + SPL_HOLD).

### Evidencia (host x86)

`tests/boot_fade_test.c` (cada render en su propio `fork()`, como `regress`, para que los sembrados de ruido
no cambien los bytes):

| Chequeo | Resultado |
|---|---|
| ganancia 0 es silencio digital exacto | ok (pico 0, ningún sample distinto de 0) |
| la rampa nunca vuelve atrás | ok (0 → 2 809 → 5 578 → 10 847) |
| el techo de la rampa no altera la señal | ok (32768 = bytes idénticos a un render sin fade) |

Lo que **no** se midió: el click analógico del propio código de audio al desmutear (pc6), el del amplificador
y el del DSP del panel. Sobre eso el firmware sólo puede garantizar que la señal digital sea cero en ese
instante, que es lo que ahora hace.

## 2. La pantalla

### Lo que ya estaba y se volvió a verificar

- `lcd_init` (`lcd.c`): SWRESET, SLPOUT, COLMOD, MADCTL, PORCTRL, GCTRL, VCOMS, LCMCTRL, VDVVRHEN, VRHS,
  VDVS, FRCTRL2, PWCTRL1, PVGAMCTRL y NVGAMCTRL, `0x21` y `0x13`, **clear de toda la GDRAM 240×320** y recién
  entonces `0x29` + backlight: el panel no puede escanear una fila que nadie escribió.
  `tests/panel_table_test.py` falla si la tabla se aparta del driver publicado, pierde un comando o
  reintroduce brillo/CABC — **ok** en este árbol.
- El splash pinta por etapas desde el primer píxel (`splash_step`), con la misma tabla de arranque en FULL,
  SHORT y OFF.
- El barrido de UI (host x86) renderiza 191 pantallas × 22 paletas + el barrido de páginas y valores: 0
  hallazgos de lint, 0 tintas sobre su propio color, 0 píxeles GREY/MONO fuera de gris/neutro, 0 px de
  desalineación en 3 777 997 medidas.
- El emulador WASM de esta versión (Emscripten 6.0.11) arranca y dibuja con la misma tabla: el registro del
  emulador y el build coinciden (coherence_check).

### Límite honesto

No hay unidad: una pantalla "casi negra" en la FM-1 puede ser el panel, el flex, el backlight (GPIO) o el
contraste óptico, y ninguna de esas cosas se puede leer desde este entorno. Lo que se puede afirmar es que el
firmware escribe la secuencia completa, limpia la GDRAM entera antes de encender la luz y que ninguna de las
191 pantallas ni de las 22 paletas queda plana o ilegible en el render medido.
