# PUNCH-IN FX — auditoría de implementación (2026-11-03)

Este documento describe el estado real de la rama `perf/security-hardening`. No declara verificados el LCD,
el audio, el USB/MIDI ni la recuperación de un FM-1 físico: esas pruebas requieren hardware y no se hicieron.

## Mapa final

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
