# FASE 2/5 — Flash XIP y recursos: 4 168 B devueltos, 8 860 B libres en la ranura

Fecha: 2026-11-25. Base: `a1bc8ab6` (rama `main`, paquete v2.5.1).
Todas las cifras de firmware son **medidas con el AC791N real**, no estimaciones de documentación:
toolchain `jieli-linux-toolchains-20260730.1` (`tools/get_toolchain.sh`, SHA-256 verificado) y SDK
`AC79NN_SDK_V1.2.1_2023-12-13` (gitee, `$HOME/fw-AC79_AIoT_SDK`). `./build.sh` produce
`build/felucca.map`, `build/felucca.syms` y el paquete.

## El mapa, medido dos veces

| Región | Antes (a1bc8ab6) | Después | Cambio |
|---|---:|---:|---:|
| XIP código | 449 328 B | 449 632 B | +304 B (decodificadores) |
| XIP constantes | 124 812 B | 120 340 B | **−4 472 B** |
| XIP total desde 0x02000120 | 574 140 B | 569 972 B | **−4 168 B** |
| Imagen / ranura de app | 578 920 / 583 612 B | 574 752 / 583 612 B | **−4 168 B** |
| **Margen de la ranura** | **4 692 B** | **8 860 B** | **+4 168 B (meta ≥ 8 KiB cumplida)** |
| RAM `.data + .bss` | 74 000 / 98 304 B | 76 048 / 98 304 B | +2 048 B (WT_COEF en RAM) |
| RAM libre (`.data + .bss`) | 24 304 B | 22 256 B | −2 048 B |
| POOL (sobra) | 296 220 B (24 288 B) | 296 220 B (24 288 B) | sin cambio |
| LOWBUF | 29 256 / 32 768 B | 29 256 / 32 768 B | sin cambio |
| Código RAM | 3 940 / 24 576 B | 3 940 / 24 576 B | sin cambio |
| NOINIT | 15 572 B | 15 572 B | sin cambio |

El código crece 304 B porque los tres decodificadores nuevos son código; el neto sigue siendo
−4 168 B de flash. Los pisos no se tocaron: POOL conserva sus 24 288 B de sobra (suelo 8 KiB),
LOWBUF sus 3 512 B, y RAM libre queda en 22 256 B (antes 24 304 B), muy por encima del margen con el
que se publicó v2.5.1.

## Qué se comprimió (todo sin pérdida, enteros idénticos)

| Recurso | Antes | Después | Ahorro | Decodificación |
|---|---:|---:|---:|---|
| `FM6_EXP2_D2` (2ª diferencia de 2^(i/1024)) | 2 046 B | 384 B | **1 662 B** | su propia 1ª diferencia en 3 bits (+3), valor inicial en macro; `fm6_exp2_init` |
| `AF_S_G` + `AF_M_G` + `AF_L_G` (652 registros de glifo) | 3 912 B | 2 608 B | **1 304 B** | un `uint32`: off 13 bits, bx+1 3 bits, by/bw/bh 5 bits cada uno; máscaras en `gfx.c` |
| `WT_COEF` (amplitudes armónicas Q30 de WAVETABLE) | 2 048 B | 1 184 B | **864 B** | mapa de 64 B de los ceros + 280 valores no nulos (1 120 B); `wt_coef_init` expande a `.bss` |
| `SINE_D2` (2ª diferencia del seno) | 1 023 B | 384 B | **639 B** | 3 bits por valor (+3); `sine_init` |
| Decodificadores | — | +304 B de código | −304 B | — |
| **Neto XIP / imagen** | | | **4 168 B** | |

Los tres bloques de datos se verificaron valor por valor contra la fórmula original (Python) antes de
compilar: 1 023 diferencias del seno, 1 022 terceras diferencias de la curva exp2 y las 512 amplitudes
de WAVETABLE reconstruidas desde el mapa de bits son **exactamente** los mismos enteros. No hay
redondeo, tabla aproximada ni pérdida de resolución.

`WT_COEF` es el único cambio que usa RAM: 2 048 B de `.bss` (22 256 B libres todavía) y se expande una
vez, antes del primer `wt_gen_init`, nunca dentro de un bloque de audio. La ISR no decodifica nada
nuevo: SINE y la curva exp2 se generan al arrancar como ya hacían desde la Fase 1, y los glifos se
leen con una máscara y un desplazamiento (UI, no audio).

## Auditoría de lo que se descartó (con la medida delante)

- **`ui_shapes.h` (10 895 B de puntos) no ocupa flash.** Los 17 arrays `SH_PLATES_*_ILL` y `og_ill`
  no aparecen en `build/felucca.elf` ni en el mapa: el linker los descarta porque ningún camino los
  llama (`ui_organic.c` se incluye en `ui_graph.c`, pero `og_ill` no tiene llamadas). Comprimirlos no
  habría devuelto un solo byte. Es el hallazgo que evitó una ronda de trabajo inútil.
- **Fuentes e iconos ya están comprimidos con Huffman** (S 7 294 B frente a 8 432 B en nibbles; los
  iconos suman 7 747 B). No hay acceso aleatorio por glifo que permita otra pasada sin descomprimir
  todo en RAM; se quedan como están.
- **Keycaps** (2 787 B) siguen con su Huffman y sus arcos de perilla: no se vuelven a tocar, como pedía
  la fase.
- **`UAC_FIR` (3 840 B) ya explota la simetría del prototipo** (80 fases de 160; la otra mitad se lee
  hacia atrás). Cada fase no es simétrica, y DPCM/Rice sobre los coeficientes Q30 sale *peor* que los
  int16 crudos (medido: 16 691 B con Rice k=7). Se deja intacto para no arriesgar el remuestreador.
- **Tablas de presets (10 188 B)**: el `preset_t` mide 36 B y está denso (sin relleno, sin registros
  duplicados). Comprimirlo exigía decodificar en cada acceso o mover 10 KB a RAM; no entra en esta
  fase.
- **LOWBUF y POOL**: no hay un solapamiento de vida útil nuevo que demostrar. La unión de reverb de la
  ronda anterior sigue siendo la única; POOL mantiene 24 288 B de sobra y LOWBUF 3 512 B. No se movió
  ningún staging ni se hizo ninguna copia completa nueva.
- **Generación al arranque**: ya está usada donde tocaba (SINE, exp2 de FM6, límites de WAVETABLE).
  Esta fase comprime sus semillas, no añade trabajo de arranque: la expansión de WT_COEF son 512
  pruebas de bit y la de SINE/exp2 ya existía.
- Descartados por instrucción explícita: diccionario de cadenas, recompresión de keycaps, recorte de
  FIR sin simetría real y cambios globales de `-Os`/`-O2`.

## Verificación

- Reconstrucción exacta de las tres tablas contra la fórmula (script de la fase): `True` en SINE_D2,
  FM6_EXP2_D2 y WT_COEF.
- `tests/theme_test`: 22 paletas, contraste, mezcla y métricas de fuente, con el registro de glifo
  empaquetado; pasa.
- `tests/text_ref_test`: 6 732 cadenas × fuentes × paletas contra el renderizador de referencia,
  **0 posiciones de pen y 0 píxeles distintos**.
- `tests/regress`: 311 goldens de audio, **0 cambiados**; `six_track_live` 61/61 s idénticos.
- Suite completa `tests/run_tests.sh` (incluye UI, proyecto, USB, loader, FM4, SINE/exp2): todo pasa
  salvo `text spacing`, que da 0,258 px en un par de S (límite 0,25). Se reprodujo **igual** con el
  generador y el test de HEAD (`git show`, sin estos cambios): es deriva del rasterizador de fuentes
  de este entorno (Pillow/FreeType), no del empaquetado. El empaquetado conserva los cinco campos de
  cada glifo y `text_ref_test` no ve ni un píxel distinto.
- Emulador WebAssembly con Emscripten: arranque, teclas, MIDI, pantalla, LEDs, PLAY y un guardado
  entre instancias, todo ok; la misma canción contra el build nativo, **bit a bit** (88 200 frames), y
  1 s de canción pesada en 27,8 ms (2,8 % de tiempo real).
- Emulador: `web/emu/build.sh` genera `build/emu/{felucca.wasm,index.html,worklet.js,fonts}` desde las
  mismas fuentes y tablas; las pruebas de arranque/teclado/MIDI y la paridad con el build nativo se
  ejecutan sobre ese wasm.

## Release automático

El repositorio ya tenía la cadena: `tag-and-publish.yml` corre en cada push a `main`, lee
`FELUCCA_VERSION`, verifica `release/felucca-<v>.fwsc` y su manifiesto, etiqueta el commit y publica el
release (con `release.yml` construyendo y attestando la etiqueta). Esta entrega usa esa cadena: la
versión sube a v2.5.2, `tools/make_release.py --sync` deja el paquete, el manifiesto, las sumas, el
sitio y el zip del emulador coherentes, y el push a `main` dispara tag y release sin intervención. No
hace falta un workflow nuevo: cada mensaje que suba la versión y el paquete se publica solo.
