# PUNCH-IN FX — auditoría de implementación (2026-11-03)

Este documento describe el estado real de la rama `perf/security-hardening`. No declara verificados el LCD,
el audio, el USB/MIDI ni la recuperación de un FM-1 físico: esas pruebas requieren hardware y no se hicieron.

## Estado vigente — 2026-11-04 (reemplaza el mapa de abajo)

La pantalla ya **no tiene diez tarjetas ni una rejilla 5×3**. El mapa completo es el teclado: 16 teclas
blancas de 14 px (141 px de alto) y 11 negras (66 px), desde F3 hasta G5. Las tarjetas superiores FILTER,
CRUSH y THROW se retiraron; DEPTH queda en el encabezado. El banco, LATCH, `LIVE/CAP/n/3`, `TGT` y `SEL`
ocupan dos líneas. El panel mide 170 px y se dibuja en dos bandas (48 + 122 px) para respetar `CV_MAX`.

La captura que publica el portal (`web/site/assets/emulator-punch-in-fx.png`, 1040×650) se regeneró desde
Chromium con la capa FX abierta y una tecla sostenida, así que muestra el mapa nuevo y no la rejilla 5×3.

La copia servida en `build/site/emulator` estaba un commit atrás (`b358b3f`) respecto de `build/emu`
(`ab3da86`), aunque el WASM tuviera el mismo SHA. Ahora se sincroniza, `web/test_site.py` compara los
manifiestos y `web/emu/index.html` recarga con `?v=<commit>` cuando el HTML cacheado no coincide con
`manifest.json`. La causa no era un Service Worker: no hay ninguno.

## Mapa histórico (2026-11-03; reemplazado)

La capa PUNCH-IN FX se abre manteniendo **FX**. **A#4 mantenido** alterna el banco; un toque corto de A#4
sigue cambiando el destino de los efectos MIDI (`ALL`, `SYN`, `DRM`). El mapa muestra siempre las diez
asignaciones, el teclado completo F3–G5 y los seis mutes de pista.

| Banco | Tecla | Nombre | Ruta real |
|---|---|---|---|
| 1 | F3 | RPT | REPEAT del master en el buffer del SLICER (1/16) |
| 1 | C5 | STU | STUTTER de notas a 1/8 |
| 1 | C#5 | GAT | compuerta de 1/16 del master, con rampa |
| 1 | F#5 | DIV | DIVE de pitch de dos octavas por beat |
| 1 | F5 | RND | RANDOM de notas (MIDI, no audio) |
| 2 | F3 | REV | lectura inversa del buffer de audio |
| 2 | G3 | FRZ | freeze/repeat del buffer |
| 2 | A3 | TAP | tape-stop: la velocidad de lectura cae a cero |
| 2 | B3 | GLC | saltos de slice y dirección aleatoria |
| 2 | C4 | RTR | retrigger de un loop corto |
| 2 | F5 | CRS | **CRUSH independiente**: `perf_block` fuerza el crush a 100 mientras F5 está retenido |

`CRS` ya no es solamente el macro KNOB 2. En el banco 2, F5 se retiene y produce el crush de audio; al
soltarlo vuelve a seco con la misma rampa que los demás efectos. En el banco 1, F5 conserva RANDOM y la
tarjeta muestra `RND`. No se añadió un buffer, una voz ni un parámetro nuevo: los cinco efectos del banco 2
reutilizan el loop de 8192 frames estéreo del SLICER.

### Teclas negras y mutes

- Las **teclas negras 1–6** ya no son mutes momentáneos de `PF_M1`: alternan el estado latch `P_MUTE` de las
  pistas 1–6. Una pulsación mutea, la siguiente desmutea, y el estado se conserva al soltar FX.
- El LED de cada mute se dibuja desde `trk[i].p[P_MUTE]`, no desde una variable de pantalla. Los mutes no
  tocan los bancos, los efectos MIDI ni la lane grabada.
- Las teclas negras restantes conservan VIB, GATE, RTRG y DIVE. `A#4` es el selector de banco/destino y `G5`
  limpia la lane.

### SWEEP

`SWEEP` no se pudo conservar en el mapa de teclas dentro de la palabra de 32 bits de `PF_*`: al añadir los
cinco bits del banco 2 no queda un bit libre. No se eliminó su DSP de los filtros: el barrido de cutoff sigue
disponible a través de LPF/HPF y KNOB 1. D#5 se reutiliza para RTRG. La alternativa sería añadir otro estado
de banco o migrar la lane a más de 32 bits; ninguna se implementó en esta etapa para no romper el formato
guardado ni el presupuesto. `SWEEP` no se presenta como una tecla implementada.

## Interfaz y páginas

- `firmware/src/ui_layer.c`: mapa de diez tarjetas (5 + 5), teclado F3–G5 de 16 blancas y 11 negras, fila de
  mutes T1–T6 y fila VIB/GATE/RTRG/DIVE. Las tarjetas son etiquetas de teclas físicas; no hay botones falsos.
- `firmware/src/ui_graph.c`: la página FX tiene cuatro canales limpios, medidor vertical real desde `t->peak`,
  peak-hold, valor de send y barra fina. Sin señal, el medidor queda vacío; no hay animación decorativa.
- `firmware/src/seq.c`: `perf_key()` devuelve `PF_N` para las negras 1–6; el latch de mute vive en la capa UI.
- `firmware/src/ui_input.c`: fuerza el redibujo cuando cambia el banco.
- `web/editor.html`: botón **PUNCH** y pantalla LCD 240×240 con las diez asignaciones, teclado, banco y los
  mutes leídos de `dev.mix.tracks[i].mute`. Es un monitor de Web Studio, no sustituye la prueba en navegador.
- `web/emu/felucca_web.c` y `web/emu/emu_test.mjs`: export de motor y matriz de KNOB 1–4 por motor.

## Pruebas ejecutadas

| Prueba | Resultado real |
|---|---|
| `./tests/groups.sh ui` | **PASS**: 172 pantallas × 17 paletas; 0 lint; 2.647.728 medidas de alineación, 0 fuera por ≥1 px; 0 tintas sobre su propio color. |
| `./tests/groups.sh presets` | **PASS**. |
| `./tests/groups.sh web` | **PASS**: `web tests passed`. |
| `./tests/groups.sh target` | **PASS**: app 551224 B; paquete 610897 B; identidad `FM-1_982`; RAM `.data+.bss` 94668/98304; pool spare 9248 B; RAMTEXT spare 2968 B; NOINIT spare 124 B. |
| `target_budget.py` | **PASS**: `perf_begin` 28/28; `perf_block` 374/522; sin voces ni calidad reducidas. |
| `build/perform_test` | **PASS**, incluido `bank 2 F5: CRUSH...` y los seis mutes combinados. |
| `node web/emu/emu_test.mjs build/emu/felucca.wasm` | **PASS**; WASM 1.025.406 B; 1 s de la canción pesada en 22,8 ms (2,3 % de tiempo real). |
| `python3 tests/install_test.py` | **PASS** (simulación de transporte del instalador; no es una instalación física). |
| `node web/test_backup.mjs` | **PASS**. |
| `node web/test_pkg_bounds.mjs` | **PASS**. |
| `git diff --check` | **PASS**. |

No se ejecutó `run_tests.sh` completo en un solo proceso por el límite de cinco minutos del entorno; los
grupos se ejecutaron por separado. Un timeout no se cuenta como aprobado.

## Límites que impiden declarar V1.8

- No hay navegador real ni automatización de navegador: la pantalla PUNCH de Web Studio pasó la sintaxis
  JavaScript (`node --check`), pero no se verificó en un navegador real.
- No hay FM-1 físico: LCD, encoder, audio, USB/MIDI, JEDEC, backup y recuperación no están verificados.
- El host no tiene `/dev/snd/seq`; ALSA responde `Unknown SEQ default` y no se puede ejecutar la instalación
  virtual con `fm1-live`. El instalador simulado pasa, pero eso no es una instalación real.
- La versión sigue siendo **`v1.7-QG`**. No se cambia `FELUCCA_VERSION` ni se anuncia V1.8 mientras esas
  verificaciones sigan pendientes.

## Capturas nuevas

Generadas desde el framebuffer real del WASM recién compilado, no desde un dibujo aparte:

- `artifacts/stage4-home.png`
- `artifacts/stage4-fx.png`
- `artifacts/stage4-punch-bank1.png`
- `artifacts/stage4-mutes.png`
- `artifacts/stage4-punch-bank2.png`

Las capturas `artifacts/stage3-home.png` y `artifacts/stage3-punch-bank2.png` se conservan como registro de
la etapa anterior y **no** representan el mapa final.
# PUNCH-IN FX — auditoría de implementación y estado (2026-11-03)

Este documento separa lo que está conectado al DSP de lo que solo cambia la pantalla. La regla de esta
pasada es simple: un nombre en un pad no cuenta como efecto. Un efecto existe solo si su tecla, su
parámetro y su ruta de audio o de notas están implementados en el firmware.

## Clasificación

| Estado | Significado |
|---|---|
| IMPLEMENTADO | La tecla y su ruta DSP/MIDI existen en el código. |
| TESTEADO EN HOST | Las pruebas nativas ejercitan la ruta; no sustituye hardware. |
| COMPILADO PARA TARGET | `tests/groups.sh target` genera el binario AC791N y pasa sus comprobaciones. |
| SIMULADO | Se ejecutó el WASM y se capturó su framebuffer real; no es una prueba física. |
| VERIFICADO EN HARDWARE | Requiere una unidad FM-1, audio, LCD, USB/MIDI y recuperación. **No se hizo.** |

## Efectos solicitados: qué hay realmente

| Efecto pedido | Estado | Qué hace el firmware | Límite honesto |
|---|---|---|---|
| Reverse | **NO IMPLEMENTADO** | No hay captura inversa ni reproducción hacia atrás. | Necesita un buffer separado y una ruta de lectura inversa; no se añadió. |
| Glitch | **PARCIAL** | `RND` desplaza notas/octavas y `ST8`/`ST16` repiten notas; hay texturas rotas, pero no un efecto llamado Glitch. | No es un buffer de glitch dedicado ni un efecto de audio nuevo. |
| Freeze | **NO IMPLEMENTADO** | No hay freeze del bus master. El freeze de reverb pertenece a otro motor y no es PUNCH-IN FX. | No se debe anunciar como freeze. |
| Tape Stop | **NO IMPLEMENTADO** | `DIVE` (F#5) baja la afinación de las voces dos octavas durante un beat. | Es una caída de pitch, no un varispeed de cinta ni un tape-stop del master. |
| Stutter | **IMPLEMENTADO (notas)** | `ST8` y `ST16` repiten las últimas notas del track a 1/8 y 1/16, con gate a la mitad; la lane puede grabarlos. | Repite eventos de nota, no el audio ya mezclado. El stutter de audio es `REPEAT`. |
| Repeat | **IMPLEMENTADO (audio)** | `1/8`, `1/16`, `1/32` graban y repiten el master en el buffer del SLICER: 8192 frames estéreo a 22.05 kHz (371 ms), cuantizado al siguiente 1/16. | Si el loop pedido no cabe en el buffer al tempo actual, el rate aparece atenuado. |
| Retrigger | **PARCIAL** | `STUTTER` y `ARP` vuelven a disparar notas; `ARP` además sube dos octavas. | No existe un retrigger independiente de audio ni de una sola nota. |
| Gate | **IMPLEMENTADO** | C#5 aplica una compuerta de 1/16 al master, con rampa; se puede soltar sin clic. | Es una compuerta rítmica del master, no una compuerta por track. |
| Pitch Drop | **IMPLEMENTADO (DIVE)** | F#5 baja todas las voces dos octavas, lento y luego rápido, durante un beat; al soltar vuelve de inmediato. | No es un tape-stop; no baja la velocidad del transporte. |
| Bit Crush | **IMPLEMENTADO (macro)** | KNOB 2 `CRUSH` reduce bits y retiene muestras en el master (4..11 bits efectivos; 0 = apagado). | Es un macro, no una tecla de la lane; no se añadió otro crush. |
| Lo-fi | **IMPLEMENTADO (motor)** | El motor LOFI tiene sus propios BIT/FLOAT, sample-and-hold, corrupción y filtros. | Es un motor de sonido, no un efecto PUNCH-IN; no se presenta como efecto nuevo de esta capa. |

## Los 15 pads y las teclas negras

Los 15 pads blancos son los efectos ya existentes, en este orden:

`1/8`, `1/16`, `1/32`, `LPF`, `HPF`, `OCT-`, `OCT+`, `1/2`, `DEC-`, `DEC+`, `ST8`, `ST16`, `ATK+`, `ARP`, `RND`.

Las cuatro teclas negras inferiores conservan:

- `G#4 VIB`: vibrato de ±1/2 semitono a ~5.4 Hz en todas las voces.
- `C#5 GATE`: compuerta de 1/16 del master.
- `D#5 SWEEP`: barrido del LPF de un compás, de abierto a cerrado y de vuelta.
- `F#5 DIVE`: caída de pitch de dos octavas por beat, no tape-stop.

`A#4` cambia el destino de los efectos MIDI (`ALL`, `SYN`, `DRM`) y `G5` borra la lane. Los seis mutes
siguen siendo los tracks 1..6.

Macros con FX:

- KNOB 1 `FILTER`: LPF hacia la izquierda, HPF hacia la derecha.
- KNOB 2 `CRUSH`: 0..100.
- KNOB 3 `THROW`: mezcla seca hacia delay/reverb.
- KNOB 4 `DEPTH`: nivel de los efectos de buffer.

## Por qué no se añadieron Reverse/Glitch/Freeze/Tape Stop

El banco con buffers adicionales se midió en **662/522** en `perf_block` y dejaba sin margen el
`RAMTEXT`; por eso se descartó. No se redujeron voces, calidad ni presupuestos para hacerlo caber.
La lane usa códigos de 4 bits y guarda como máximo 1..12; añadir efectos nuevos exigiría también
migrar el formato guardado. En esta pasada **no se añadió DSP nuevo**: el cambio es de presentación y
del emulador.

Consecuencia: `DIVE` no se renombra a Tape Stop, `RND` no se renombra a Glitch y el freeze de reverb
no se anuncia como Freeze. Los pads conservan los 15 efectos que ya existían.

## Rediseño visual

- Cinco columnas por tres filas para los 15 pads, 42 × 18 px, con separación de 20 px.
- Pads redondeados, LED de estado, línea superior y riel lateral para el efecto activo.
- La leyenda `PUNCH-IN FX · A#4 TGT · G5 CLR` ocupa la línea libre entre los pads y los mutes.
- Los mutes de las teclas negras quedan en su propia fila, sin invadir la lane.
- El texto de los pads se dibuja directamente, no con el ancho medido de `cv_text_r`.

## Emulador: causa de que no mostrara el cambio

Había dos causas distintas:

1. **Artefacto/cache del navegador.** El WASM se regeneraba en `build/emu`, pero la página podía
   seguir usando una copia anterior. El fetch ahora pide `felucca.wasm?v=@VERSION@`; el build sustituye
   `@VERSION@` y el HTML servido apunta a `v1.7-QG`.
2. **Etiquetas invisibles solo en WebAssembly.** En el build WASM, `cv_text_r` dependía de un ancho
   medido que no producía el texto esperado; los pads se dibujaban pero sus nombres no. El host sí los
   mostraba, lo que explica que la captura del firmware pareciera correcta y la del emulador no. La
   celda de 18 px usa ahora dibujo directo con `cv_text_on`, y el color se elige por el relleno para
   que funcione en todas las paletas.

El WASM final mide **1.019.205 B** y declara `v1.7-QG`. No se commitea `build/emu`; se regenera con
`sh web/emu/build.sh`.

## Capturas

Las capturas se generaron desde el framebuffer real del WASM y desde el render del firmware, no con
automatización de navegador:

- `artifacts/emulator-home.png`
- `artifacts/emulator-punch-in-fx.png`
- `artifacts/emulator-fx-zoom.png`
- `artifacts/firmware-home.png`
- `artifacts/firmware-home-mini-knobs.png`
- `artifacts/firmware-punch-in-fx.png`

## Pruebas de esta pasada

| Prueba | Resultado |
|---|---|
| `tests/groups.sh ui` | **OK**: 172 pantallas × 17 paletas, 0 lint, 0 desalineaciones, 0 tintas sobre su propio color. |
| `node web/emu/emu_test.mjs build/emu/felucca.wasm` | **OK**: botones, breath, presets, canción pesada y tiempo real (2.2 % de un segundo de audio). |
| `tests/groups.sh target` | **OK**: app 549220 B; paquete 610897 B; identidad `FM-1_982`; `target_budget` y `memory_map` pasan. |
| `target_budget.py` | **OK** con `perf_block` 569 en bucles; sin DSP nuevo. |
| `memory_map_test.py` | **OK**: RAM spare 3716 B, pool 9248 B, RAMTEXT 2404 B, NOINIT 124 B; app 549220 ≤ slot 581564. |
| `sha256sum build/felucca.bin` | `616db4d6ed191e171721c7cdc1f7ea9fde80705ef1911f0dad51c1918c206f8d` |
| `sha256sum build/felucca.fwsc` | `106b5f41bd8f93fa22142e9c9669375dc784f7e65a19de2b8183d7e19acedab4` |

No se ejecutó `run_tests.sh` completo en un solo proceso por el límite de cinco minutos del entorno.
Los grupos se ejecutaron por separado. Un timeout no se cuenta como aprobado.

## Riesgos y estado remoto

- **NO VERIFICADO EN HARDWARE.** No hay FM-1 físico: no se probaron LCD, encoder, audio, USB/MIDI,
  JEDEC real, backup ni recuperación USB_KEY. **No se declara instalación física segura.**
- El emulador prueba la lógica y el dibujo en el framebuffer; no prueba la pantalla, el reloj ni el
  códec del dispositivo.
- El target se compiló, pero no se flasheó. La identidad `FM-1_982` y los límites de memoria son
  comprobaciones de build, no una validación eléctrica.
- La rama local puede estar por detrás de `origin/perf/security-hardening`; no se hace force-push ni
  se reescribe el remoto. Cualquier integración debe hacerse con un merge normal y revisar el diff.


## Actualización 2026-11-03 — tres cabezas reales

- `perf_press()` limita a tres los efectos musicales activos; el cuarto se rechaza sin modificar los tres
  que ya están sonando. Los mutes T1–T6 no cuentan para el límite.
- Las seis capturas (REPEAT, REV, FRZ, TAP, GLC y RTR) comparten una única grabación de 32 KB, pero tienen
  **tres cabezas de lectura independientes** (`pb[3]`), cada una con su modo, longitud, cursor y ganancia.
  La captura se toma una vez y cada cabeza lee su ventana; la mezcla húmeda es su promedio ponderado.
- Se probaron las seis individualmente, las 15 parejas y las 20 ternas; cada combinación produce audio
  distinto de sus componentes. También pasan tres FX sobre la canción de seis pistas. No se duplican
  buffers, voces ni polifonía.
- `LATCH:NO` mantiene el efecto solo mientras la tecla está pulsada; `LATCH:YES` lo enciende en la primera
  pulsación y lo apaga en la segunda. La pantalla muestra `CAP %` o `n/3` y una barra de captura/lectura.
