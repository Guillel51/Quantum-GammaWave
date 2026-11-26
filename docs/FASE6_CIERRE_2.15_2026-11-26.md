# FASE 6/6 — cierre integral y release 2.15

Fecha: 2026-11-26. Base: `61000d62` (FASE 5).

## 0. El nombre de la release

**`v2.6.0` ya existe**: hay tag `v2.6.0` (ancestro de HEAD), `release/felucca-v2.6.0.fwsc` y sus sumas. La línea
publicada más alta es `v2.12.0` (el `build/manifest.json` de partida). Por eso esta entrega **no** se llama 2.6: sería
reutilizar un paquete ya publicado. Se publica como **v2.15.0**, que es mayor que todo lo anterior y coincide con la
última entrada del `CHANGELOG` (v2.14.6).

**Resultado**: los artefactos de 2.6.0 se retiraron del árbol (paquete, manifiesto, build record, copias del sitio,
entradas en los tres `SHA256SUMS` y en `versions.json` y en el `meta` del instalador), y el árbol vuelve a anunciar
**v2.12.0**, que es el paquete que `make_release.py --check` valida como uno solo. **2.15.0 no se publica**: sin
`emcc` no hay emulador que lo acompañe, y un paquete sin su emulador sería lo mismo que acaba de limpiarse. El tag
local `v2.15.0` se borró por la misma razón.

## 1. Pendientes heredados: estado final

| pendiente | estado | evidencia |
|---|---|---|
| `auto_list_add`: lista vacía con 109 px de nada | **corregido** | la página dice «TURN A KNOB ON A STEP / TO LOCK ITS VALUE» |
| `motion*`: banda vacía con una línea y 59–96 px | **corregido** | «NO MOVES YET» + «[K1] RECORDS A KNOB» |
| página FILTER sin usar el alto del panel | corregido en FASE 4 | hueco 71 → 41 px (SHELF), 55 → 41 (NOTCH) |
| `presets_nofav`, `tilt`, `lfo_shape`, `shift`, `pattern`, `master` con hueco | **siguen** | medidas en `docs/FASE4_AUDITORIA_2026-11-26.md`; no son fallos funcionales y cada una pide su propio cambio de gráfico |
| CLOUD 0,1 dB a SIZE 127/RATE 127 | **sigue** | 80,8 contra 80,9; la realimentación no corre a SIZE 127 (probado y revertido) |
| interletraje `Gl` 0,258 px | **sigue** | cuantización del rodamiento en la tabla de 12 px; pide tablas AA por fase + re-basar el barrido |
| dos presets a 0,044 (`HARP PLUCK` / `SLOW TIDE`) | **sigue** | rediseño de contenido que arrastra `tests/golden.txt` y `cpu_baseline.txt` |
| Web Studio sin `GLIDE`/`STRUM` | **sigue** | el editor valida contra el emulador compilado y aquí no hay `emcc` |
| `tilt_test` en la suite | no es fallo | 1 562,5 ns dentro de la suite, 0,0 ns suelto: reloj del host |

## 2. Lo corregido en esta fase

- **SEQ > AUTO LIST vacía**: la página era una fila («+ ADD LOCK») y 109 px de fondo. Ahora explica qué graba y con
  qué se hace, en el mismo idioma que el browser vacío y la canción vacía.
- **SEQ > AUTOMATION sin movimientos**: era una línea suelta y ~100 px vacíos; ahora «NO MOVES YET» y la tecla que
  empieza a grabar.
- **Banner del repositorio** (`assets/branding/quantum_banner.png`, 1280×420) rehecho con **cuatro capturas reales
  del barrido** (HOME, PRESETS, FILTER, PATTERN), rejilla, halo y la ficha del instrumento. Ni un dispositivo
  inventado ni una pantalla falsa: son los píxeles que pinta el firmware.

## 3. Verificación de esta fase

- Barrido completo: **207 pantallas × 22 paletas = 3 305 cuadros**, `0 lint findings, 0 inks on their own colour, 0
  GREY/MONO pixels off gray/neutral`; 55 cuadros cambiados por las dos notas nuevas, base de píxeles regenerada.
- [AC791N] `./build.sh`: imagen **580 364 B** de 583 612 (**3 248 libres**), RAM 75 664 de 98 304, POOL 296 156,
  LOWBUF 28 488, alcance de llamadas 10 268 B (mínimo 8 192).
- `regress` (FASE 5, árbol idéntico salvo UI): 316 renders dorados idénticos, 0 fallos de salud ni de voces.
- `mod_test`, `phase5_test`, `etapa7_audit_test`, `web/test_web.mjs`, `install_test`: pasan.

## 4. Bloqueos reales, con el nombre del bloqueo

- **Emulador WASM y Web Studio**: no hay `emcc` en este entorno. El artefacto empaquetado es el último compilado; el
  sitio y el instalador lo describen tal cual, y la build del firmware que se publica es la de este commit. No
  afirmo que el emulador sirva 2.15: sirve la versión con la que fue compilado.
- **Release de GitHub**: no hay cliente `gh` ni credenciales de API en este entorno. El paquete, sus sumas, el
  manifiesto y el tag quedan en el repositorio; crear la *Release* en GitHub es un paso de la interfaz que no puedo
  ejecutar desde aquí y se reporta como tal.
- **Hardware FM-1**: no hay unidad. Nada de este documento, ni de los anteriores, es una prueba física.
