## v3.5.0 — 2026-11-15 — la pantalla, redistribuida; los temas, separados

Refinamiento de la interfaz y de las gráficas sobre 3.4.0.

### Distribución

- **La barra superior cede dos filas** (22 → 20 px) y **los cuatro cuadros de knobs ganan cuatro** (46 → 50 px): el
  valor sube con su indicador y la barra del gauge baja al pie del cuadro, con el dial redondo intacto. El cambio
  es de la maqueta compartida, así que alcanza las 189 pantallas, y el barrido de render lo mide: **0 hallazgos de
  lint, 0 desalineaciones en 3 398 185 medidas, 0 tintas sobre su propio color**.
- **HOME, pantallas 3 a 5: fuera la tira K1..K4.** Mostraba cuatro valores que nada en esas páginas cambiaba.
  Cada pantalla lleva ahora su propio título en esa fila (OVERVIEW, MIXER, PERFORMANCE) y el contenido respira.

### Gráficas y temas

- **Los planetas cambian con el tema.** Una paleta sin rampa propia (GREY, GREEN, ROSE, PAPER, HI-CON, NIGHT,
  MONO) caía en la térmica de GAMMA, así que los cinco mundos de HOME eran los mismos colores con cualquier
  tema; la rampa se deriva ahora de los tokens de cada paleta, y los cinco cuerpos ocupan cinco puntos de ella
  (60, 110, 180, 245 y 256 sobre una base de 210).
- **El osciloscopio derecho del visualizador ya nunca falta**: estaba detrás de un umbral de pico, así que un
  pasaje suave dejaba la retícula vacía; se escala a su propia señal y en silencio muestra el punto quieto del
  centro.
- **La traza de fondo de HOME va debajo de los datos**, no encima: se dibuja antes del suelo, de las bandas y de
  las dos curvas, con dos píxeles en la rampa del tema, así que no cruza la gráfica principal ni la desplaza
  (la curva sigue respondiendo solo al knob 1).
- **AMBER, ICE y VIOLET se retiran.** Dejan de ofrecerse en MENU > COLOR y en el Web Studio; una unidad que los
  tuviera guardados los lee como el tema vivo más cercano (ROSE, NORDIC, NEBULA) y los guarda como ese, sin
  correr los índices de las demás paletas guardadas.
- **FX > DELAY página 2:** la etiqueta "PITCH" deja cuatro píxeles libres antes del dial del knob 2 en vez de
  terminar contra su cuerpo.

### Comprobado

- `ui_test`: **all passed**; barrido `ui_render`: 189 pantallas × 22 paletas sin lint ni desalineaciones.
- Firmware AC791N compilado y empaquetado; el paquete, su manifiesto, el emulador WASM, el instalador y el sitio
  se publican juntos y `coherence_check` los verifica entre sí.

### Pendiente, dicho

- El reequilibrio de niveles del banco de presets y el relleno degradado de EDIT > FILTER 4/6 no están en esta
  versión: quedan para la siguiente, medidos con `tools/preset_audit.py --levels`.

