# Fase 5 — seguridad, auditoría final y optimización (v1.5.26)

Informe de lo que se corrió, lo que se encontró, lo que se cambió y lo que sigue necesitando la unidad. Clases como
siempre: **[ESTÁTICO]** leído en el código/dato, **[HOST]** medido con los runneres del repo en este contenedor,
**[AC791N]** medido en el build del target, **[HW]** requiere la FM-1 física (no hay ninguna en este entorno).

## 1. Qué se resolvió de las fases 1–4

| Pendiente | Estado en esta fase |
|---|---|
| El instalador rechazaba el registro JSON de M-Flasher como copia inválida | **resuelto** (§3): se lee, se guarda y ata el SHA-256 de la copia |
| El campo de la copia sólo aceptaba `.bin`, así que el `.json` ni se podía elegir | **resuelto** |
| El mensaje del gate no decía cuál de los dos archivos de M-Flasher cargar | **resuelto**: nombra el `.bin` que queda junto al `.json` |
| Análisis del backup V16 | **hecho** en `docs/FASE5_BACKUP_V16.md` |
| Overflow en el polo del DC blocker de los motores aditivos | **encontrado y corregido** (§2, F-1) |
| Código muerto en `firmware/src` | **barrido y eliminado** (§4) |
| `[HW]`: updater del V16, restauración, escritura real del chip | **siguen abiertos**: `docs/FASE2_ISSUES_106_107.md`, `docs/VALIDATION_MATRIX.md` |

## 2. Auditoría de seguridad

### 2.1 Lo que se ejecutó

| Runner | Cobertura | Resultado |
|---|---|---|
| `tests/groups.sh security` (exit 0) | instalador, preflight, gate, firma, custodia de claves, backup, restauración, límites y fallos de paquete, audio con ASan/UBSan, fuzz corto, matrices de corte de energía (`storage_cut`, `ota_cut`), EQ y estados del EQ | **pasa** |
| `fuzz_ed` 200 000 iteraciones, seed 11, ASan+UBSan | SysEx de editor y paquetes USB-MIDI crudos (187 569 frames, 12 431 paquetes) | **pasa** |
| `fuzz_proj` 100 000 iteraciones, seed 17, ASan+UBSan | almacenes de proyecto mutados → import → restore → render (9 625 importados) | **pasa** |
| `fuzz_proj` 100 000/17 con **`signed-integer-overflow` activado** | el mismo, con la comprobación de desbordamiento que el repo desactiva a propósito para el DSP | **encontró F-1** (abajo) |
| `node web/test_v16_path.mjs` | caminos V16 + 20 000 entradas basura contra el lector de registros | **62 checks, 0 fallos** |
| `node web/test_web.mjs`, `test_backup.mjs`, `test_pkg_bounds.mjs`, `test_pkg_faults.mjs`, `python3 tests/install_test.py` | instalador, backup, límites y fallos del paquete | **pasa** |
| `node web/emu/emu_test.mjs` | emulador completo, 65 536 words del panel contra la conversión de pantalla | **all ok** |

### 2.2 Hallazgos

**F-1 · Overflow entero con signo en el polo del DC blocker (`[HOST]`, UBSan).** `eng_additive.c:197` y
`eng_wavetable.c:167` hacían `(dcy * 32744 + 16384) >> 15`. El intermedio no está acotado por la muestra: 96 000
(un pico de voz tras el filtro) por 32 744 pasa de `2^31`, así que el extremo del rango **normal** ya era
comportamiento indefinido, y el fuzz con estado mutado lo alcanzó (seed 17). Corregido con `dc_pole()` en `dsp.c`:
`v * 32768` es múltiplo exacto de 32768, así que la suma se parte sin pérdida en `v + ((16384 - 24v) >> 15)`, con
un clamp que sólo toca estados que ninguna ruta de muestra produce. **Verificado**: idéntico al original en 299 594
valores (todos los múltiplos de 7 del rango) y el producto es 4096× menor. Sin coste de RAM ni de flash medible
(imagen +16 B, §5) y sin cambio de sonido: los goldens de audio pasan.

**F-2 · `Truncated identification reply` en una unidad stock (documentado, no es un defecto).** El comando `0x12`
es propio de este firmware; una V16 no lo implementa. El transporte lo reporta como tal y la página no inventa una
identificación. Los dos mensajes del reporte tienen causas distintas — transporte y gate — y están explicados en
`docs/INSTALL_FROM_V16.md` §7. **No se debilitó ninguna comprobación.**

**F-3 · El registro de M-Flasher entra como entrada no confiable.** Es un JSON que cualquiera puede escribir y que
no necesita unidad. Se trata como **metadato**: se valida la forma (modelo `FM-1`, SHA-256 de 64 hex, ids en
`0x…`), se guarda en `localStorage`, y su único poder es **rechazar** una copia que no coincida con el digest que
nombra. El gate del V16 sigue exigiendo la copia de 1 MiB, la confirmación de restauración y el paquete oficial
V16, y la parte se sigue leyendo en la unidad antes de borrar. 20 000 entradas basura: no lanza y sólo acepta lo
que trae digest.

**F-4 · `mulq15()` envuelve por diseño (residual, documentado).** Con `signed-integer-overflow` activado, un estado
de voz corrompido desborda el producto de `mulq15` (dsp.c). El repo excluye esa comprobación para el DSP
(`tests/groups.sh`), junto con `shift`, `bounds`, `object-size` y `pointer-overflow`, porque el DSP usa aritmética
envolvente; ASan sigue cubriendo memoria. Queda anotado como **supuesto**, no como memoria corrupta, y no se
cambió el helper caliente sin medición de CPU en el target (regla §8 del `AGENTS.md`).

**Lo que no se tocó**: los gates, la allowlist de JEDEC (`0x856014`, `0xB36014`), la verificación de firma, el
`Verify Only`, ni una sola comprobación de escritura.

## 3. Instalador y compatibilidad V16

`web/index_pkg.html` (fuente del instalador; `site/webapp/installer/` se regenera con `web/make_site.py` en el
release):

- `readMflasherRecord()`: lee el JSON de M-Flasher; modelo, digest obligatorio, parte y *chip key* en hex,
  bootloader, firmware y herramienta.
- El `.bin` de 1 MiB se comprueba **contra el registro**; si el SHA-256 no coincide, `v16DumpMismatch` y no se
  envía nada.
- Estado del bloque V16: muestra parte · *chip key* · prefijo del digest cuando hay registro y todavía no hay copia.
- `accept=".bin,.json,…"`, y `v16NeedDump` nombra el `.bin` que queda junto al `.json`.
- Textos en los tres idiomas del instalador (inglés, español, chino).
- **El gate no cambia** y la copia sigue teniendo que estar atada a la identidad verificada de la unidad (v1.5.21).

## 4. Auditoría final del proyecto

- **Código muerto**: barrido de `firmware/src` por funciones `static` sin una sola referencia. **20 funciones
  eliminadas**; el enlazador ya las descartaba con `--gc-sections`, así que la imagen **no** baja: el cambio es de
  limpieza, no de tamaño (el paquete mide lo mismo con y sin ellas). Dos correcciones que el propio build y la
  propia suite forzaron, y que se dicen porque costaron un build cada una: `fm1_fault()` **se conservó** — la llama
  el manejador de crash del HAL (`firmware/hal/fm1_irq.h`), una referencia que el primer barrido no veía porque no
  está en `firmware/src` — y **ocho funciones volvieron** porque los runneres de host las llaman (`dice_all`,
  `proj_pack_vb`, `slicer_reset`, `voices_clear`, `gen_power_on`, `perf_buf_state`, `motion_replace_track`,
  `vz_flares`): el enlace del firmware las tiraba igual, pero `tests/groups.sh ui` no compilaba sin ellas.
- **Duplicaciones**: las 67 tablas `const` del firmware no tienen dos copias idénticas; los literales repetidos que
  un escaneo ingenuo marca son falsos positivos de expresiones multilínea.
- **Sincronización**: firmware → paquete → WASM → manifiesto → sitio, verificada por `make_release --check`.
- **Issues**: **#106** sigue abierta por hardware (no hay unidad). No se cerró nada sin evidencia.
- **Documentación**: `docs/INSTALL_FROM_V16.md` §7 nuevo, `docs/FASE5_BACKUP_V16.md` nuevo, `CHANGELOG.md` y
  `README.md` con v1.5.26.

## 5. Optimización (la tercera, distinta de las dos anteriores)

Las dos anteriores fueron de **recursos** (Fase 1: RAM/XIP) y de **dibujo en el panel** (Fase 4: píxeles que no
cambian). Esta es del **camino de presentación del emulador**: la expansión RGB565 → RGBA de cada frame.

`web/emu/index.html` expandía los 57 600 píxeles de un frame con `* 255 / 31` y `* 255 / 63`: **57 600 divisiones
y 57 600 multiplicaciones en coma flotante por frame para 96 valores posibles**. Ahora tres tablas (`R5` 32, `G6`
64) se construyen una vez con **las mismas expresiones**, en `Uint8ClampedArray` — el mismo tipo y el mismo
redondeo (mitad a par) que usaba la asignación al `ImageData` —, así que **cada byte en pantalla es el que era**.

| Medida | Antes | Después |
|---|---|---|
| `[HOST]` conversión de un frame (node, 20 000 frames, mejor de 3) | 0,564 ms | **0,180 ms** (**3,13×**) |
| `[HOST]` división por píxel | 57 600 | **0** |
| `[HOST]` bytes distintos en los 65 536 words del panel | — | **0** (`web/emu/emu_test.mjs`) |

Presupuestos del build `[AC791N]` de esta versión (`tools/mem_budget.py --show`, todas las ventanas por
encima de su piso) (sin cambios de calidad ni de funcionalidad):

| Ventana | Uso | Libre |
|---|---|---|
| XIP / imagen de app | 582 724 B de 583 612 B | **888 B** |
| RAM `.data+.bss` | 86 892 B de 98 304 B | 11 412 B |
| POOL | 311 256 B de 327 680 B | 16 424 B |
| NOINIT | 15 572 B de 15 696 B | 124 B |
| LOWBUF | 30 564 B de 32 768 B | 2 204 B |

La imagen crece **16 B** respecto de v1.5.25 (el helper del DC blocker menos el código muerto): se dice, no se
esconde. No se quitó ninguna funcionalidad ni se bajó ninguna calidad: la única prueba de esa afirmación es la de
arriba — los 65 536 words dan los mismos bytes y los goldens de audio y de render pasan.

## 6. Lo que sigue pendiente, y es hardware

| Afirmación | Clase |
|---|---|
| El registro de M-Flasher se lee y ata la copia | `[HOST]` `node web/test_v16_path.mjs` (62) |
| El polo del DC blocker no desborda y da el mismo valor | `[HOST]` equivalencia sobre 299 594 valores + fuzz 100 000 |
| Fuzz de editor y de proyectos bajo ASan/UBSan | `[HOST]` 200 000/11 y 100 000/17 |
| La pantalla del emulador da los mismos bytes | `[HOST]` `web/emu/emu_test.mjs`, 65 536 words |
| **El updater del V16 acepta el paquete y arranca el loader** | `[HW]` no medido |
| **La restauración de la copia sobre el V16** | `[HW]` no medido |
| **Escritura del chip por el loader de este proyecto** | `[HW]` no medido |
| **Sonido, panel y DAC de la unidad** | `[HW]` no medido |

Nada de esta fase se escribió, borró ni restauró en la FM-1 física. El backup del dueño no se guardó en el
repositorio, ni en `site/`, ni en `release/`, ni en tests: el fixture de los tests es inventado en el propio test.
