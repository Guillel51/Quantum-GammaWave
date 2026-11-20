## v1.5.13 — GLO MONITOR: la fila que se escribía fuera de su buffer, y la polifonía medida

- **GLO MONITOR, la causa del texto que se superpone después de una secuencia.** No era un problema de refresco
  ni de formato de los campos: la página construía cada fila concatenando con
  `str_cpy(b + str_len(b), …, sizeof b - str_len(b))` alrededor de un `fmt_int` **sin límite**. La fila
  `POLYPHONY` crece mientras el secuenciador corre (STEALS y DENIED cuentan y ganan dígitos) y con los contadores
  altos mide 54 caracteres sobre un `char b[48]`: `fmt_int` escribía más allá del final del array, `str_len()`
  recorría memoria que no era la cadena, y `sizeof b - str_len(b)` se daba la vuelta a un `size_t` enorme, así que
  el siguiente `str_cpy` escribía **detrás** del buffer. Lo que se veía como campos encimados era memoria vecina
  pisada. Ahora las siete filas se arman con `rowb.h`, que lleva su longitud y nunca escribe más allá de la
  capacidad: una fila que no entra se corta, pero sigue siendo una cadena. `char b[64]` cubre la más ancha (63).
- **…y la fila `POLYPHONY` imprime el presupuesto que rige, no `NVOICE`.** Decía `12` aun con LOAD ECO puesto, que
  es exactamente la cifra contra la que un instrumentista compara un acorde. `voice_budget()` (voice.c) devuelve el
  tope del asignador — el ajuste del usuario recortado por ECO — y el monitor lo muestra.
- **La polifonía, medida y con números.** `tests/poly_budget_test.c` (host x86-64, las fuentes reales) mide voces
  *que suenan* (activas y con gate), no un contador dibujado:

  | caso | resultado medido |
  |---|---|
  | 8 / 10 / 12 notas sostenidas en un part (ANALOG) | 8 / 10 / 12 con gate, 0 robos, 0 notas denegadas |
  | ajuste VOICES en 10 / 8 | el tope aplicado es 10 / 8, no 12 |
  | LOAD ECO | **6 voces en total y 4 por part** (`ECO_POLY`), que es el "6–7 voces" del reporte |
  | FM6 (6 operadores por voz) | 4 por part (su `engine_t.poly`), por diseño |
  | cuatro tracks, 3 notas cada uno | 12 voces, ninguna denegada, el bloque las renderiza |
  | acorde de 6 + 6 con la mitad soltada | 6 suenan, 6 en release, el acorde nuevo entra sobre las colas |
  | cambio de preset con 8 notas sostenidas | el bloque sigue sonando (energía > 0), el acorde no se recorta, 0 voces quedan |
- **Lo que este release NO arregla, y por qué.** El parpadeo de HOME 2–5 y los nombres de preset sobre GLO MIXER
  siguen abiertos. Lo que hay, medido o leído en el código, es esto: (a) las tres pantallas de HOME pintan el
  cuerpo entero en tres blits (26 + 88 + 88 filas) cuando **cualquier** cosa de la firma se mueve, y la fase de las
  brasas (`ui.frame / 8`) está en esa firma mientras la animación está en FULL: el cuerpo completo se reenvía al
  panel ~7,5 veces por segundo sin que haya nada nuevo que leer, y con el secuenciador andando la tasa de frames
  baja, que es el "parpadeo más lento" del reporte; (b) en GLO MIXER el nombre de cada columna ya está recortado a
  su ancho (`track_col6`, primer palabra y letras que entran) y el pop-up de preset es una placa opaca de 1,5 s con
  su propia invalidación al cerrar: lo que queda por identificar es qué camino de dibujo deja tinta sobre el panel
  sin invalidarlo. Las dos son de la clase "hardware/emulador con captura de frames": el lint de layout y los 3056
  goldens de esta suite pasan, así que no hay una prueba de host que las reproduzca todavía, y prefiero decirlo a
  inventar un arreglo que no puedo verificar en el panel.
- **Pruebas nuevas en la suite:** `ROWB` (con ASan/UBSan, filas al peor caso y guardas detrás del buffer) y
  `POLY BUDGET` (los casos de la tabla). Ambas en `tests/run_tests.sh`. La suite completa pasa; los goldens de UI se
  regeneraron sólo por la cadena de versión (`tests/ui_pixels.txt`, 48 líneas de 3056 frames).
- **Flash:** imagen **582 536 B** de 583 612 (1 076 B libres), `.data+.bss` **91 648 B** de 98 304 (6 656 libres),
  paquete 610 933 B, identity `FM-1_965`. El builder acotado ocupó 148 B menos que las concatenaciones que reemplazó.

## v1.5.12 — Las cinco ventanas de RAM, medidas y vigiladas

- **La auditoría, con números del linker real** (`build/felucca.map`, build AC791N de esta versión):

  | ventana | usado | tamaño | libre |
  |---|---|---|---|
  | LOWBUF | 30 564 B | 32 768 | 2 204 |
  | RAM (.data+.bss) | 91 628 B | 98 304 | 6 676 |
  | POOL | 311 256 B | 327 680 | 16 424 |
  | RAMTEXT | 3 792 B | 24 576 | 20 784 |
  | **NOINIT** | **15 572 B** | **15 696** | **124** |

  La ventana más ajustada no era la flash (928 B) sino **NOINIT, con 124 B**, y no tenía ninguna regla que la
  protegiera. `.noinit` guarda lo que sobrevive a un reinicio (bootguard, motivo de reset) y está pegada a los
  guard words y a las pilas: un buffer que creciera enlazaba bien y después pisaba memoria viva en la unidad.
- **`firmware/app.ld`**: NOINIT gana el `ASSERT` que LOWBUF tiene desde 1.4.5. Un buffer que la desborde ahora
  **falla en el link**, con la unidad intacta.
- **`tools/mem_budget.py` + `tests/mem_budget.json`**: se lee el mapa del linker tras cada build y cada ventana
  conserva su piso (LOWBUF 1 KiB, RAM 4 KiB, POOL 4 KiB, RAMTEXT 1 KiB). Se suma a la suite cuando existe
  `build/felucca.map`; sin toolchain se saltea con una línea. Un cambio que se coma la holgura tiene que decirlo.
- **Lo que NO cambió:** ni un byte de DSP, motores, polifonía, FX, UI ni audio. En esta versión no hay una
  optimización de código nueva, y lo digo claro: las candidatas que quedaban (aliasing de los buffers de reverb,
  sustituir el hash del canvas, tablas de color) o cambian el sonido, o sólo se pueden validar midiendo en la
  unidad, o cambiarían los 3056 goldens sin forma de verificar el resultado en hardware. Preferí no vender como
  optimización algo que no puedo medir; lo que sí traje es la medición de las cinco ventanas y la regla que
  faltaba.

## v1.5.11 — El banco de presets, medido y nivelado

- **La medición.** `tests/preset_levels` renderiza los 285 presets de fábrica uno por uno (C4 a
  velocidad 100, ataque, cuerpo y el primer tramo del release, por el mix real) y da RMS y pico en dBFS.
  Antes de esta versión: mediana **−18,5 dB**, **28 presets fuera de ±3 dB**, dispersión **25,8 dB**, y los
  extremos eran `CRYSTAL PLUCK` a −36,8 dB y `QUANTUM BEAT` a −11,0 dB.
- **El problema no era el fader.** La tabla `firmware/src/preset_level.c` (generada por
  `tools/gen_preset_levels.py` a partir de esa misma medición) ya pone el nivel al cargar cada sonido, y
  **25 presets seguían fuera de alcance con el fader a tope**: su contenido era demasiado bajo, no su nivel.
- **Seis sonidos corregidos en su patch, no en el número:** `CRYSTAL PLUCK` y `METAL CLUSTER` (ADDTV) ganan
  cuerpo y SUB — el cluster tenía el suboscilador mudo —, `AURORA FM` baja el envío de reverb/delay que lo
  ahogaba, `ABYSSAL DRONE` baja la reverb de 96 a 72, y `STATIC HAZE` / `GEIGER CNT` (NOISE) recuperan densidad
  y dejan de esconderse en su propia reverb.
- **Después, medido con la misma herramienta:** **19 presets** fuera de ±3 dB, dispersión **12,6 dB**, el más
  fuerte `HEARTH KIT` a −16,9 dB (antes −11,0). La tabla de niveles se regeneró desde la nueva medición, así
  que cada preset carga al nivel que le corresponde con el sonido nuevo.
- **Lo que queda fuera a propósito:** `AURORA FM`, `STATIC HAZE`, `GEIGER CNT`, `ARCADE`, `ABYSSAL DRONE`,
  `CRYSTAL PLUCK`, `GLASS BELL` y `POLAR AURORA` siguen 4–11 dB por debajo de la mediana. Son texturas, colchones
  y drones: su RMS bajo es su carácter (un geiger contando chispas, una aurora que se abre en 30 ms de ataque) y
  subirlos más habría sido cambiar el sonido, no nivelarlo. Está documentado en
  `docs/PRESET_BALANCE.md` con la tabla completa antes/después.
- **Un golden que dependía del calendario.** ABOUT imprime la fecha de compilación (`__DATE__`), así que una
  corrida de la suite que cruzaba la medianoche encontraba 32 frames cambiados — la página about en cada
  paleta — y fallaba por un reloj, no por un dibujo. `tests/ui_render.c` fija esa fecha **sólo en el harness**:
  los píxeles de referencia dependen del dibujo, no del día; el dispositivo sigue mostrando su fecha real.
- **Sin tocar:** motores, algoritmos, polifonía, FX, routing, UI y gates. Sólo datos de preset y la tabla de
  niveles. Goldens de audio regenerados por el cambio audible (63 renders) y verificados por la suite: 0 fallos
  de salud, 0 de voces, 0 presupuestos de CPU excedidos.

