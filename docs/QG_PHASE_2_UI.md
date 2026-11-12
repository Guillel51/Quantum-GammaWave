# Fase 2/5 — Interfaz: HOME vivo, arranque y pieles

Estado: **parcial, integrado y verificado en host y en el emulador web**. Este documento dice qué se
cambió, cómo se midió, qué se rechazó y **qué falta**. No se avanzó a la fase 3.

Todos los números de este informe son de **host** (`ui_render`, el mismo código que enlaza el
firmware, 240×240, framebuffer real) o del **emulador web compilado desde este árbol**. No hay ninguna
medida `[TARGET]` ni `[FÍSICO]`: en este entorno no hay toolchain AC791N ni unidad física, y nada de lo
que sigue se presenta como validación en placa.

---

## 0. Lo primero: dos defectos reales encontrados antes de dibujar nada

### 0.1 El splash no estaba "lento": estaba congelado

El síntoma del pedido —«la pantalla parece quedarse detenida entre 1 y 2 segundos antes de la
animación final»— no era un temporizador largo. Era un reloj que **todavía no existía**:

`draw_splash()` se dibuja al principio de `fm1_main()` y la animación se temporizaba con `fm1_ms`,
que **sólo avanza cuando arranca TIMER5** (`timer5_start()`, mitad de la función). Entre los dos
estaban: `fm1_input_init`, `fm1_adc_init`, `panel_init`, `felucca_init` (los motores), `master_boot`,
`audio_init`, `usb_start`, `boot_leds`, `timer5_start`, el guard, el arranque del segundo núcleo,
`autosave_boot` (lectura de flash) y `reel_boot` (otra). Todo eso corría con el contador en 0: el
splash ya estaba en pantalla y se quedaba **en su primer fotograma** durante toda la inicialización.

Y después, encima, el bucle del splash volvía a contar desde cero (`t0 = fm1_ms`) y sumaba sus
5,3 s completos. De ahí los 1-2 s muertos más el splash entero.

**Corrección** (`main.c`, `ui_draw.c`):

- El reloj del splash es **TIMER4** (`fm1_micros()`), que arranca en `fm1_cstart()` antes que todo, y
  se toma **en el primer píxel** (`spl_t0_us`).
- `splash_step()` repinta el splash al terminar cada etapa lenta (panel, audio, USB, autosave, reel):
  la animación avanza **durante** la inicialización en vez de esperarla.
- El bucle final usa **el mismo reloj**: lo que la inicialización ya gastó cuenta, así que no se paga
  dos veces. Si la inicialización se comiera la escritura entera, el fundido final igual recibe sus
  fotogramas (`el + SPL_FADE`) en vez de saltearse.

### 0.2 `ui_palettes.h` estaba dos veces, y la copia vieja ganaba

`firmware/src/ui_palettes.h` está versionado y `gfx.c` lo incluye con comillas: resuelve **primero el
del propio directorio**, así que la copia versionada tapaba siempre a la generada
(`build/gen/ui_palettes.h`). Cualquier paleta nueva habría sido invisible para el build de host y para
el del SDK. Ahora las dos están sincronizadas (el generador sigue siendo la fuente de verdad) y la
verificación lo cubre: 22 paletas llegan al barrido de render.

---

## 1. HOME: el sistema planetario, vivo

El banco de HOME es 88 × 52 px dentro de la pantalla de 240 × 240. Todo lo que sigue cabe ahí y pasa
por el clip del banco (`sys_pset/sys_rect/sys_line`), así que nada se sale del marco.

### 1.1 La estrella

Antes: cinco discos concéntricos y un destello. Ahora (`gw_star`):

- **Un aliento exterior** (un cuarto disco, más ancho y tenue) que late con la energía del patch.
- **Corona que gira**: ocho rayos sobre un círculo de radio `r + k`, cada uno con su longitud (una
  senoide triple por rayo, no por píxel) y su brillo; el conjunto tarda ~13 s en dar una vuelta. No es
  un anillo: nunca está quieta.
- **Llamaradas propias**: cada ~7,3 s lanza cinco ascuas desde el borde por el pool de partículas que
  el banco ya tenía (sin asignaciones nuevas).
- **Respuesta al toque**: la estrella es ahora un blanco (antes el centro del banco era un hueco).
  Al tocarla, un pulso expansivo recorre el banco y **los cinco mundos responden**: parpadeo y
  excitación al máximo, así que los cercanos crecen un píxel y se encienden cuando la onda los pasa.

### 1.2 Cinco mundos, cinco identidades

Cada cuerpo ya tenía su patrón (rocas y mares, bandas y ojo de tormenta, horizonte y destello, anillo
doble, casquetes y grieta) y su propia reacción al toque. Faltaba lo que se ve **de un vistazo**:

- **Color propio**: todos tomaban el mismo punto de la rampa térmica (`heat + i*18`), así que eran
  cinco discos ámbar. Ahora cada uno tiene su tono (`ORK_HUE`): rocoso cálido, gigante gaseoso dorado,
  anillado rosa, oceánico violeta, congelado azul hielo.
- **Clima propio** (`gw_body_life`): cada cuerpo tiene un período distinto (5,4 s … 10,6 s) y un
  evento distinto — el rocoso sopla polvo del limbo, el gaseoso saca su ojo de tormenta, el oceánico
  destella, el anillado hace brillar su anillo, el congelado tira escarcha del polo. Un `if` cuando no
  toca; un puñado de píxeles cuando sí.
- **Un píxel de espacio** (`gw_sphere`): una hairline del fondo en `r + 2`. Dos órbitas que se cruzan
  ya no funden dos cuerpos en una sola silueta.

### 1.3 Fondo y profundidad

- **Dos mundos que no orbitan** (`gw_far_world`): giran sobre su eje a su ritmo, se quedan en su sitio
  y están mezclados muy lejos del fondo (32-40 %), con terminador propio. La órbita exterior pasa por
  delante de uno: el banco tiene un delante y un atrás. A `r ≤ 2` se dibujan como una mota de un solo
  tono (con fase se leían como una mancha de dos colores).
- Polvo, estrellas fijas con parpadeo y nebulosas ya existían; no se duplicaron.

### 1.4 Coste

Medido con `ui_render` (host, por fotograma de pantalla completa, paleta QUANTUM):

| | antes (`fb4e0f8`) | después |
|---|---:|---:|
| HOME, fotograma forzado | **180,0 µs** | **174,9 µs** (−2,8 %) |
| HOME, fotograma sin forzar (tiras perezosas) | 56,2 µs | 55,1 µs |
| vista VISUALIZER de HOME (HIGH / LOW) | 99,7 / 94,8 µs | 93,5 / 89,5 µs |

O sea: **más dibujo y menos tiempo**. La corona son ocho senos y ≤ 48 píxeles; el clima, un `if`; los
mundos lejanos, dos discos de 2-3 px por fotograma. MENU > VISUALIZER QUALITY y MENU > ANIM siguen
mandando sobre el polvo y el movimiento.

---

## 2. Arranque: 0,8 s menos

- `SPL_HOLD` 2800 → **2000 ms**. `SPL_WRITE` (2,5 s, la firma escribiéndose) y `SPL_FADE` (380 ms, el
  traspaso a HOME) **no se tocan**: se acorta la espera, no la animación.
- Con el reloj arrancando en el primer píxel, el splash completo son **4,5 s desde la primera luz**
  (antes: 5,3 s *después* de la inicialización, más los 1-2 s congelados).
- Verificado en el emulador: el splash aparece escribiéndose y llega a HOME sin pantalla negra ni
  parpadeo (capturas en `build/cap/emu_splash_a.png`, `build/cap/emu_home2.png`).

---

## 3. Pieles

Seis pieles, cada una con **su propia rampa térmica** (`heat_col`) además de su paleta: sin eso, las
curvas, los medidores, la estrella y los planetas seguían en la rampa anterior y la piel sólo cambiaba
el borde.

| piel | identidad |
|---|---|
| **QUANTUM** | índigo de vacío, cian Cherenkov, ámbar del Hearth (la de fábrica) |
| **NEBULA** | índigo profundo, violeta/orquídea, cian para lo único activo |
| **PLASMA** | casi negro, magenta caliente, oro para lo que suena |
| **ORGANIC** | musgo y verde vivo sobre tierra oscura, miel en el control activo |
| **MINIMAL** | grafito y blanco papel, un solo acento cálido |
| **RETRO** | fósforo ámbar sobre marrón cálido, cursor verde |

Se eligen en **MENU > COLOR** (que ya lista, guarda y aplica la paleta; la elección persiste con el id
etiquetado `UI_PAL_TAG + índice`), y volver a QUANTUM es el valor de fábrica. La intensidad y la
velocidad de animación siguen siendo **MENU > VISUALIZER QUALITY** y **MENU > ANIM**: no se duplicó un
segundo juego de controles para lo mismo.

Contraste: las seis pasan las comprobaciones WCAG del generador (texto, valores, selección, keycaps,
REC); ninguna piel toca el audio, los parámetros ni la memoria de la música.

---

## 4. Indicadores de página

HOME son **ocho páginas** (ecosistema, visualizador, overview, mezcla, performance, motor, matriz,
sistema) y el pie las llamaba a todas «HOME»: era el único sitio del instrumento sin indicador. Ahora
dice **«HOME 3/8»**, igual que el resto de las familias («DLY 2/2»). Verificado en el emulador: al
tocar HOME pasa a 2/8, 3/8…

---

## 5. Pantallas nuevas: qué hay y qué no

Inventario real de lo que ya existía, para no duplicar:

- **MOD MATRIX** → existe como HOME 7/8 (`HV_MOD`, `hv_mod`) y como página `MOD` del motor (slots,
  fuente/destino/cantidad).
- **PERFORMANCE** → existe como HOME 5/8 (`HV_PERF`, `hv_perf`).
- **PRESET BROWSER** → existe la familia PRESETS con favoritos y filtro (`favorites`, `favorites.filter`).
- **FX ROUTING** → **no existe**. Hoy los envíos se ven por partes en MIXER/EDIT, no como recorrido.
- **SEQUENCER TOOLS** → **no existe** como tal; `TOOLS` es una página de utilidades globales.

No se implementaron FX ROUTING ni SEQUENCER TOOLS en esta pasada. Preferí no inventar pantallas a
medio hacer ni botones sin función: el requisito «ningún control ficticio» pesa más que la lista. Es
la deuda declarada de esta fase, con el plan en §7.

---

## 6. Verificación ejecutada

- **Barrido de render** (`ui_render`, el dibujo real): **191 pantallas × 22 paletas** más el barrido de
  páginas y valores. **0 hallazgos de lint**, 0 textos cortados, 0 tintes fuera del eje gris en GREY y
  MONO, **0 desalineaciones** en 2 863 797 medidas de tinta (antes 2 779 972 con 17 paletas).
- **Toques de HOME** (`capture_home_pokes`, con `UI_NO_DRIFT=0`): las cinco reacciones siguen siendo
  cinco (78 / 116 / 62 / 85 / 36 píxeles de cambio, cada una distinta), un toque fuera del banco no
  cambia ni un píxel, y un segundo toque reinicia el gesto. La reacción del oceánico se había quedado
  en 5 px al añadir la hairline (la onda nacía *dentro* del cuerpo): ahora nace en el limbo, medido.
- **Estrella**: toque en el centro → índice 5, los cinco `pl_flash` a 256 y pulso activo; una esquina
  vacía del banco no hace nada.
- **Emulador web**, compilado desde este árbol con `emcc 4.0.7` (`web/emu/build.sh`: build
  `fb4e0f867ba6`, wasm 1 192 714 B, sha256 `63b7e41c…`): arranque, splash escribiéndose, HOME,
  toque en la estrella, cambio de vista y pie «HOME 2/8». **Desviación**: el build pinea emcc 6.0.11 y
  aquí hay 4.0.7; el emulador avisa y no falla.
- Suite de host completa en el árbol final: **`ALL HOST TESTS PASSED`, 119 secciones, `EXIT=0`** (log
  en `run_tests.log`). Incluye 220 renders dorados de audio sin cambios (esta fase no toca el sonido) y
  el barrido de UI de las 22 paletas. La única prueba que hubo que tocar es la del ciclo de paletas de
  MANU > COLOR, que enumeraba las 17 anteriores: ahora enumera también las seis pieles y sigue
  comprobando cada nombre y el retorno a GREY.

### Lo que no se pudo medir

- **Nada de placa**: sin toolchain AC791N no hay `.bss`/flash reales, ni `build/felucca.dis`, ni
  ciclos por fotograma del panel. El coste de `ui_render` es tiempo de host, no del chip.
- **No hay `[TARGET]`**: el ancho del envío SPI al panel y el tiempo real de `splash_step()` en el
  FM-1 quedan sin medir.
- El emulador usa `emcc` distinto del pineado (§6).

---

## 7. Deuda de esta fase, con plan

1. **FX ROUTING** (la más útil de las cinco): una vista de HOME que dibuje el recorrido real —
   parte → insert → envíos a bus → cadena master — leyendo los parámetros que ya existen
   (`mix_part`, envíos por parte, buses). Sin controles nuevos: sólo lectura del camino.
2. **SEQUENCER TOOLS**: variación/edición sobre `seq_steps` (acortar, duplicar, rotar, humanizar) con
   undo, en la familia SEQ.
3. **PRESET BROWSER**: categorías y comparación A/B sobre la familia PRESETS que ya tiene favoritos y
   filtro.
4. **Presentación de las páginas de FX**: cada efecto con sus medidores reales y su animación.
5. **Pieles**: más allá de paleta+rampa (planetas, densidad de partículas, distribución de HOME)
   requiere campos nuevos en `settings_persist.c` y su migración.
6. **Medición en placa** de todo esto cuando haya toolchain y unidad.
