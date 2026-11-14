# Fase 2 — HOME visualizer: los knobs, el espectro y los mundos

Fecha: 2026-11-14 · firmware v3.0.0 (árbol) · sin unidad física en esta sesión

## Regla de entrada

La Fase 1 está cerrada: toolchain y SDK verificados, build AC791N real, presupuestos re-medidos,
`ALL HOST TESTS PASSED`, commit `1d64cbd`. Queda abierto **un solo** pendiente de la Fase 1, el de
coherencia de release (`tools/coherence_check.py`: falta `release/felucca-v3.0.0.fwsc` y su manifiesto,
y el emulador dice v2.9.7 mientras el firmware dice v3.0.0). Ese pendiente es de **publicación**, no de
código: no bloquea esta fase y no se maquilla aquí (ver "Lo que no está cerrado").

## 1. El desplazamiento horizontal que nadie pedía

`gw_draw_forge_bay` dibujaba las 28 bandas leyendo **una sola muestra por banda**, a un retardo fijo de
la muestra más nueva:

```c
uint32_t idx = (scope_w - 128u + (uint32_t)i * 4u) & (SCOPE_N - 1u);
int32_t  s   = scope_buf[idx] - scope_buf[(idx + 2u) & (SCOPE_N - 1u)];
```

`scope_w` avanza con el audio (`dac_scope()`), así que cada banda mostraba el pasado reciente en un
retardo proporcional a su índice: **una forma de onda recorriendo las barras de derecha a izquierda
durante todo el tema**. Eso es el scroll autónomo del panel. No había FFT ni nada que se le pareciera.

Ahora cada banda mide **su propia porción** de la ventana más nueva — el pico sobre `WF_SLICE` (4)
muestras, sin diferencia entre muestras — y el patrón no tiene forma que pueda viajar:

```c
idx = (scope_w - 128u + (uint32_t)band * WF_SLICE) & (SCOPE_N - 1u);   /* band = i + wf_shift() */
for (j = 0; j < WF_SLICE; j++) { v = |scope_buf[idx + j]|; if (v > s) s = v; }
```

Las envolventes (ataque inmediato, caída de un cuarto por frame, **aterrizando exactamente en el
objetivo**) se avanzan **una vez por frame** (`vz_qframe()`): dibujar dos veces el mismo frame da el
mismo cuadro, que es lo que necesitan los tests de píxeles y el barrido de renders.

## 2. KNOB 1 es el único que desplaza

```c
static int32_t wf_shift(void)
{
    int32_t k = bg_knob(0, 0);                       /* 0..256, el valor REAL del control */
    return (clamp(k, 0, 256) - 128) * (WF_BANDS / 2) / 128;    /* ±14 bandas */
}
```

- Es una **función pura** del valor de KNOB 1 (`home_param()` → `bg_knob()`): el mismo valor dibuja
  siempre el mismo sitio; girar a un lado y volver deja la imagen **exactamente** igual.
- No hay `ui.frame`, ni temporizador, ni acumulador: con la mano quieta no hay desplazamiento.
- KNOB 2/3/4 no lo tocan. Sus parámetros, sus tarjetas y su efecto sobre el bay siguen igual
  (`test_home_knobs` recorre las nueve máquinas y comprueba que cada knob mueve **sólo** lo suyo).

## 3. Estética del bay

- **Cinco paradas térmicas** por barra: violeta profundo (pie), rojo, naranja, dorado y amarillo solar
  (cresta), con la mezcla subiendo hacia arriba: la barra tiene volumen en vez de tres tercios planos.
- **Cresta de 1 px** en el color de pico: la definición de cada banda.
- **Brasa de 1 px** a cada lado de las bandas fuertes (`wf_lvl[i] > 168`), sólo cuando caben: las
  activas se leen primero.
- **Cuadrícula punteada** (una fila de cada tres) y horizonte/horizonte de -6 dB con mezclas suaves:
  escala sin competir con el dato.
- **La curva del filtro sigue al knob**: el valor dibujado avanza un tercio del camino por frame y
  aterriza exacto en el parámetro (la transición se ve como un barrido, no como un salto; en reposo la
  curva está donde está el parámetro, no cerca).
- El nodo de corte conserva su anillo y su núcleo; el coste no crece (ver medidas).

## 4. Los planetas

### HOME (la pantalla principal)

De los cinco mundos **sólo el océano dibujaba su reacción**. `gw_body_react` tenía ramas para
PK_RIPPLE (dos anillos), PK_FROST (escarcha + abanico) y PK_SLING (una línea de 4 px); PK_SPIN y
PK_SWELL no tenían nada. Ahora:

| cuerpo | reacción |
| --- | --- |
| PK_SPIN (rocoso) | un arco gira por el limbo, brillante en la cabeza y apagándose, y una mota sale del terminador |
| PK_SWELL (gigante) | un halo que se ensancha y se adelgaza, con dos destellos deslizando por sus bandas |
| PK_RIPPLE (océano) | dos frentes de onda desfasados + cresta (ya existía) |
| PK_SLING (anillado) | la estela se ensancha mientras el cuerpo vuelve, con brasas en el borde del anillo |
| PK_FROST (helado) | anillo de escarcha + abanico de copos (ya existía) |

Y el tap de HOME **en el panel** (que no tiene puntero) ya no inventa posiciones con el reloj
(`fm1_ms * 71 % 128`) ni escribe en `pl_react[]`, un arreglo que **ningún** sitio leía: llama a
`gw_planet_poke(NPL)`, el gesto de la estrella, y todo el sistema responde. El estado muerto
(`pl_react[]`) se eliminó.

Medido con `tests/capture_home_pokes.c` (`UI_NO_DRIFT=0`, contraste contra un control dibujado en los
mismos frames): **78, 142, 62, 85 y 36 píxeles**, cinco respuestas distintas, reiniciables, y un tap
fuera del bay no cambia nada. Antes: 0, 0, ~62, 0, 36.

### Visualizador (segunda pantalla de HOME)

Los dos mundos de fondo eran dos discos de 7 y 5 px, sin superficie y sin gesto. Ahora son los cuerpos
de HOME —el gigante con bandas y el rocoso— a **11 y 8 px**, con cinturones y cráteres dibujados con la
misma ley de terminador, mezclados más oscuros para quedar detrás del dato, y con una reacción propia
al toque (un anillo que sale del limbo y una mota), más suave que la de HOME. `ui_tap` ya no se retira
con `ui.home_viz`: pregunta `gw_viz_hit()` y un tap en cielo vacío sigue sin hacer nada.

## 5. Rendimiento (medido, no estimado)

| medida | antes | ahora |
| --- | --- | --- |
| `home`, redibujo completo (host `cc -O1`) | 173,6 µs | **155,3 µs** |
| `home`, frame sin forzar | 54,2 µs | **46,1 µs** |
| imagen | 574 572 B | **574 388 B** (−184) |
| `.data+.bss` | 87 956 B | 88 116 B (+160) |
| RAM code | 19 720 B | 19 584 B |
| XIP | 554 088 B | 554 024 B |
| paquete | 610 959 B | 610 959 B |

Sin buffers nuevos, sin asignación dinámica, sin trigonometría por píxel (la única que hay es la de
siempre: `bg_sin()` en un arco del rocoso, 5 puntos). `tests/target_budget.py` termina en 0.

## 6. Tests

- `tests/ui_test.c` → `test_home_viz()` (y `UI_ONLY=home_viz` para correrlo solo):
  - **una banda por rebanada**: una única espiga en un scope en silencio enciende **exactamente una**
    banda en seis posiciones distintas de la ventana del audio (la cadena de retardos encendía dos);
  - **nada se arrastra**: 200 frames con la misma música dejan niveles, offset y banda encendida
    idénticos;
  - **el offset es el knob**: girar KNOB 1 cambia la imagen y volver deja el cuadro **idéntico**
    (a un reloj fijo, para que el fondo no participe); `wf_shift()` no cambia con `fm1_ms`, `ui.frame`
    ni `scope_w`;
  - KNOB 2/3/4 cambian su parámetro y su lectura y **no** mueven el espectro;
  - los cinco cuerpos registran el toque y el hit-test los encuentra; el tap del panel pulsa el sistema;
  - los dos mundos del visualizador se dibujan, se aciertan y responden.
- `tests/capture_home_pokes.c` (también en `tests/run_tests.sh`, con `UI_NO_DRIFT=0`): los píxeles de
  las cinco reacciones, distintas entre sí, un fallo es nada y un segundo toque reinicia.
- `tests/ui_render.c`: 189 pantallas × 22 paletas, **0 hallazgos de lint, 0 píxeles GREY/MONO fuera de
  gris, 0 desalineaciones ≥ 1 px**; captura visual revisada (bay, VISUALIZER y los dos mundos ampliados).
- `tests/planets_test.c`: un tap llega a cada cuerpo y sólo a él.
- Build de target con toolchain JieLi + `target_budget.py` (0) + suite de UI completa (todo pasa).

## 7. Lo que no está cerrado

- **Sin unidad física**: nada de esto está verificado en hardware. Lo que hay es el build real AC791N,
  los presupuestos, los renders del código que enlaza el firmware y los tests de host.
- **Coherencia de release** (Fase 1): sigue abierta tal cual; exige decidir versión, `./build.sh
  --release`, reconstruir el WASM (`emcc`, no instalado) y sincronizar `site/`.
- El emulador no se regeneró por lo mismo: sin `emcc` no hay WASM nuevo, así que el navegador sigue
  mostrando la versión anterior del visualizador.
