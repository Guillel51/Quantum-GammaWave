# Fase 4.5 — segunda ronda: lo medido, lo arreglado y lo que quedó en rojo

## 1. Medición de partida y de llegada

| | Fase 1 (v4.0.0, `1852126`) | Ahora | Δ |
|---|---|---|---|
| Imagen AC791N | 583 600 B | **583 584 B** | −16 B |
| XIP código | 412 976 B | 412 956 B | −20 B |
| XIP constantes | 150 232 B | 150 236 B | +4 B |
| RAM `.data+.bss` | 88 148 B / 98 304 | 88 148 B / 98 304 | 0 |
| Pool | 305 096 B, spare 15 392 | idem | 0 |
| Código en RAM | 19 596 B / 24 576 | idem | 0 |
| Paquete `.fwsc` | 610 959 B | 610 959 B | 0 |
| `target_budget` (41 funciones) | 0 divides, ISR 26 923/27 338 | **sin cambios** (no se tocó el DSP) | 0 |

Clase de todas esas cifras: build del AC791N (`./build.sh`) con el toolchain de `$JIELI_TOOLCHAIN`,
no medición en la unidad.

## 2. Lo arreglado en esta ronda

**a) PAPER no pasaba su propio contraste.** La suite completa de host estaba en rojo, y la primera causa
era el tema PAPER: `T_MID` sobre `T_SURF` daba 3.87:1 contra 4.00 exigidos, y al corregirlo apareció
`T_ACCENT` sobre `T_SURF` a 3.90:1 contra 4.50. Corregido en el generador (`tools/gen_ui_palettes.py`),
que es de donde sale la tabla: superficie de PAPER de `(226,218,202)` a `(232,224,208)` (levanta el "labels
on a surface" a 4.89) y acento de `(172,56,8)` a `(150,44,0)` (acento sobre superficie por encima de 4.5).
`tests/theme_test` pasa ahora entero: los 22 pares de contraste, en las 22 paletas.

**b) `tests/settings_test` afirmaba algo que 3.5 cambió.** Desde que AMBER, ICE y VIOLET se retiraron,
un id de paleta jubilado no hace round-trip: se migra al tema vivo que heredó su lugar
(`palette_from_stored` → `UI_PAL_LIVE[UI_PAL_SLOT[i]]`). El test seguía exigiendo
`from(to(i)) == i` para los 22 índices. Ahora comprueba lo que el firmware hace: round-trip exacto para
las 19 paletas vivas, y para los ids viejos (< 20) la migración al tema vivo correspondiente. No se relajó
ninguna comprobación: se cambió la que describía un comportamiento que ya no existe por dos que describen
el actual. `tests/settings_test` pasa.

**c) Los knobs sin función ya no muestran su "K1"–"K4"** (venía de la fase 4, está en `ui_draw.c`
`draw_column_tall`): la tarjeta vacía retorna antes de la píldora. Esos −16 B de la tabla de arriba son
suyos.

## 3. Lo que quedó en rojo (reportado, no escondido)

La suite `NO_PACKAGE=1 tests/run_tests.sh` **todavía no termina en verde**. Además de lo arreglado quedan:

| Test | Fallo | Diagnóstico |
|---|---|---|
| `tests/persistence_test.c` | `persist: repeating an unchanged editor preference succeeds without erasing` | `editor_preferences.c:53` compara `palette_from_stored(persist_saved.palette) != settings.palette`; con un id jubilado en el registro guardado la comparación no coincide y la preferencia se reescribe. Hipótesis a confirmar, no verificada. |
| `tests/editor_test.c` | `MENU_DESC` (ids 0..19), `MENU_DESC (1.0.5)`, `MENU_SET`, `MENU_SET` clamp | `editor_menu.c` describe la lista de MENU al editor; la retirada de tres paletas cambió lo que MENU ofrece y el descriptor no la siguió. |

Estos dos no son consecuencia de esta fase: el árbol ya estaba en rojo en `1852126` por la retirada de
paletas de 3.5.0. No los toqué porque el arreglo correcto es cambiar el protocolo del editor con su
documentación (`docs/EDITOR_PROTOCOL.md`) y su gemelo en `web/`, y eso merece su propia verificación
contra el editor real, no un parche al final de una ronda de optimización.

## 4. Pruebas ejecutadas en esta ronda

* `./build.sh` (AC791N): cifras de §1, `register access: hal/ only`, sin avisos.
* `build/host/regress tests/golden.txt tests/cpu_baseline.txt`: 293 renders dorados, 0 cambiados, 0 fallos
  de salud, 0 de voz/ruteo, 0 entradas de CPU sobre presupuesto.
* `build/host/ui_test`: all passed.
* `build/host/ui_render`: 189 pantallas × 22 paletas, 0 lint, 0 tintas sobre su propio color, 454 textos
  elididos (9 distintos), 0 px de desvío en 3 394 196 medidas de alineación.
* `build/host/theme_test`, `build/host/settings_test`: en verde tras los arreglos.
* `NO_PACKAGE=1 tests/run_tests.sh`: corrida completa, **roja** por los dos casos de §3.

## 5. Lo que no hay

* Medición en la unidad: sin placa no hay ciclos por muestra, ni carga por núcleo, ni temperatura.
* Un cambio de DSP: no se hizo ninguno, y por eso `target_budget` sale idéntico. Con el ISR a −2 % de su
  presupuesto y 0 divisiones en bucles, la fase 1 ya había dejado el DSP sin grasa medible.
