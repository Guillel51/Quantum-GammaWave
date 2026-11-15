## v4.0.0 — 2026-11-15 — el color por tabla, y el instalador arranca el instrumento

Optimización medida del dibujado y corrección del arranque en el instalador.

### Rendimiento gráfico

- **`heat_col` es una tabla de 257 entradas, no una mezcla por punto.** Se le pide un color por punto: cada
  curva, cada banda de degradado, cada vúmetro, las nubes del splash y los cinco mundos de HOME. Terminaba en un
  `ux_mix` (paleta con rampa propia) o en siete más dos divisiones (rampa derivada). La tabla se reconstruye sólo
  cuando cambia la paleta o alguno de sus tokens y después es una carga indexada.
- **Medido, sobre el mismo banco:** el barrido completo de render pasó de **79,4 s a 25,7 s** para las mismas 189
  pantallas × 22 paletas (**3,09×**), y por pantalla `home` 4,44×, `mixer` 4,77×, `roll_chords` 4,65× (medidor de
  host, x86, `tests/ui_render.c`). El informe del barrido es idéntico: **0 hallazgos de lint, 0 tintas sobre su
  propio color, 0 píxeles fuera de gris/neutro, 454 textos elididos (9 distintos)**, y los fotogramas renderizados
  son **byte a byte iguales** (sólo difieren las líneas que llevan la hora y la versión del compilador).
- Coste en RAM del firmware: 514 B de `.bss` (la tabla). No se quitó ninguna función, voz, motor, efecto ni
  animación.

### Instalador: la secuencia de arranque, no una imagen

- **La página del instalador enciende el instrumento.** Carga el mismo `felucca.wasm` que publica el emulador, lo
  arranca con las mismas llamadas que su worklet (`web_nor_erase`, `web_master`, `web_seed`, `web_boot`) y dibuja
  el panel de 240 × 240 con `web_render(n)`, que mueve el reloj del equipo muestra a muestra: el splash, el
  barrido de LEDs y la pantalla a la que llega, a velocidad real. Antes esta página no dibujaba el panel en
  absoluto.
- **Comprobado en el binario que se publica:** arrancado en Node, ocho instantes del arranque (0, 400, 1200, 2500,
  3500, 4530, 5000 y 6500 ms) dan **ocho fotogramas distintos**, y los 7 s de arranque se ejecutan en 117 ms de
  reloj real (**60× tiempo real**), que es lo que hace que corra sin cargar el equipo.
- El reloj se detiene al llegar a HOME, se puede **repetir**, y no corre si la tarjeta está fuera de pantalla o la
  pestaña oculta. La CSP de la página suma `'wasm-unsafe-eval'`: sin eso Chromium no compila el módulo y la
  tarjeta quedaba oculta en silencio.

### Pendiente, dicho

- El reequilibrio de niveles del banco de presets y el relleno degradado de EDIT > FILTER 4/6 siguen sin hacer: no
  hay unidad ni salida de audio real en este entorno y no quise declarar niveles medidos sin medirlos.

