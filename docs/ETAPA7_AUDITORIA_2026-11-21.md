# Etapa 7/8 — auditoría, bug hunt, seguridad, regresiones y stress

Clases de medida: **`[HOST]`** = x86 (`tests/*`, `groups.sh …`, ASan/UBSan); **`[AC791N]`** = el build del
destino; **`[EMU]`** = el WASM del emulador; **`[HW]`** = la FM-1 física. En esta etapa no hubo `[HW]`.

Todo lo que sigue se ejecutó en este árbol: cada PASS corresponde a una corrida que está en el log citado.

## 1. Problemas encontrados y corregidos

### P1 — `[HOST]` El arnés de audio corría sobre un instrumento sin inicializar (y la suite estaba roja)

`tests/audio_test.c` llamaba a `delay_edge()` **antes** de cualquier `fresh()`. Desde la Etapa 2 la senoide vive
en `.bss` y se decodifica con `sine_init()` (llamado por `voices_boot()`), no en una tabla de flash: el arnés
empezaba con memoria a cero, `osc_sine()` devolvía 0, el barrido de MOD del delay medía `mmax = 0` y el check
`(1.2.1-GW) the delay's farthest read …` fallaba (`audio: 1 FAILED` en `groups.sh security`). Además las otras
pruebas del archivo corrían sobre un instrumento muerto.

- **Corrección**: `fresh()` al principio de `main` (es el boot: defaults, ventanas del pool, tablas) y un check
  nuevo — *the sine the drift rides is decoded at boot (sine_init)* — para que la clase no vuelva.
- **Resultado**: `audio: all passed` (ASan/UBSan, `-O1`).

### P1 — `[HOST]` El test del EQ llevaba tiempo fallando y su afirmación era la equivocada

`eq_response_test` exigía **medio gain en la esquina** de la high shelf a 20 kHz. Medido con el propio camino de
audio: `+12.0 dB` a 20/100/500 Hz, `+24.0 dB` desde ~2 kHz. No es un filtro roto: la shelf es un diseño
bilineal, y al acercarse la esquina a Nyquist el punto medio sube hacia el gain pleno. La afirmación venía de la
época en que la ley de esquina era otra.

- **Corrección**: el punto medio se mide a **495 Hz** (donde la deformación es despreciable) y la esquina de
  20 kHz se contrasta con lo que el diseño sí promete (entre medio y gain pleno, nunca por encima).
- **Resultado**: `eq response: passed (0 failed)`, `eq states`, `eq dc`, `eq sweep` también.

### P2 — `[HOST]` El `stress` fallaba por medir tiempo en una máquina cargada

`tests/stress_run.sh` corría `regress` con el baseline de **ns** de este host mientras el propio script es la
carga: reportó «1 CPU entries over budget (0 notes)» con una escena *idle*, cuando tres corridas en reposo dan 0.

- **Corrección**: en el lazo de stress, `CPU_NS_BASE=/dev/null` (sin baseline de ns: «no baseline here»), con el
  comentario del porqué. El presupuesto de CPU se sigue midiendo donde es válido: las corridas de host en reposo
  y `target_budget.py` sobre el binario del destino.
- **Resultado**: `stress: regress 2 full passes ok`, `stress: all ok`.

### P2 — `[HOST]` División del LFO fuera de orden (bug de la Etapa 5)

Al agregar dos divisiones al final de `N_LSYNC` quedaron **1/2D y 1/16D**, más lentas que 1/32: girando RATE
más allá de 1/32 el LFO saltaba a una velocidad lenta, y el primer test nuevo (`etapa7_audit_test`) lo detectó
como «las divisiones no están en orden de velocidad».

- **Corrección**: las dos agregadas son ahora el extremo rápido y **en orden**: `1/32T` (U=2) y `1/64` (U=1), en
  `params.c`, `voice.c` (tabla U) y el editor web. Sin cambiar el significado de ningún valor guardado.
- **Resultado**: `etapa7_audit_test` y `etapa5_tools_test` en verde; `target_budget` exit 0.

### P3 — `[HOST]` Un `for` que comparaba contra la tabla y no contra su tamaño

En la Etapa 6 la banda de MOD MAP recorría `for (d = 1; d < N_MDST; d++)`; `N_MDST` es la tabla, así que con
`-w` se compiló como comparación de puntero y el barrido de UI se colgaba. Corregido entonces con
`sizeof N_MDST / sizeof N_MDST[0]`; ahora está **fijado por test** (`etapa7_audit_test`, grupo MOD).

## 2. Pruebas nuevas

`tests/etapa7_audit_test.c` (registrada en `tests/run_tests.sh`, compilada con **ASan/UBSan**):

| Grupo | Qué fija |
|---|---|
| ARP | los 10 modos de índice devuelven siempre `idx < len` para `len` 1..64 y 256 pasos (AM_PEDAL divide por `len-1`) |
| PHASE | con valores de WAVE 0..15 guardados (9..15 fuera de rango) y 5 ajustes de DCW: ninguna muestra no finita ni fuera de ±4 |
| SCL | `N_SCALE`, `SCALE_MASK`, `N_SCALELONG` y `N_SCLFAM` tienen la misma longitud, el rango del knob coincide, cada escala tiene 5..12 notas, HIJAZ y HUNG MINOR exactas |
| LFO | el piso de RATE es la última división sincronizada, las 14 están en orden de velocidad y todas se formatean |
| MOD | `N_MDST` son 27, el rango de DEST coincide, ninguna entrada vacía, y la tabla de fuentes es la que espera el formato |

## 3. Corridas ejecutadas en esta etapa

| Grupo | Resultado |
|---|---|
| `groups.sh security` (instalador, preflight, gate, firmas, custodia, backup, restore, pkg bounds/faults, fuzz 20 000 + 5 000 con ASan/UBSan, matrices de corte de corriente de storage y OTA, eq ×4, audio ASan) | **todo PASS** (`/tmp/sec7c.log`) |
| `groups.sh stress 2` (project, persistence, robust, midi_control ×2 con ASan; regress 2 pasadas completas) | **all ok** |
| `groups.sh presets` | 287/287, una categoría primaria cada uno |
| `groups.sh web` (sitio, i18n, instalador i18n, editor vs firmware, MIDI export) | **0** |
| `groups.sh ui` (191 pantallas × 22 paletas, lint, alineación al píxel, GREY/MONO, LARGE) | ver §5 |
| `regress` | 307 goldens, **0 cambiados**, 0 fallos de salud/voz/routing, 0 crashes; 0 CPU fuera de presupuesto en reposo |
| `target_budget.py` / `resources_test.py` | exit 0 / 13 de 13 |
| build `[AC791N]` | 581 296 B de 583 612 (**2 316 B libres**), `.data+.bss` 93 612 B (4 692 libres) |

## 4. Seguridad: qué se revisó y qué quedó como riesgo

- **Instalador / updater**: `install_test`, `preflight_test`, `gate_test`, `sign_test`, `key_custody_test`,
  `restore_test` y las matrices `test_pkg_bounds` / `test_pkg_faults` sobre el `.fwsc` construido: PASS. Ningún
  gate se relajó.
- **Integridad**: SHA-256 y tamaño por paquete, manifest y metadatos, allowlist de JEDEC (`0x856014`,
  `0xB36014`) — sin cambios en esta etapa; los tests pasan.
- **Cortes de energía**: `storage_cut_test` (escritura A/B de cada objeto persistente) y `ota_cut_test` (área de
  staging de la OTA) PASS.
- **Entradas externas**: fuzz de proyecto/editor 25 000 casos con ASan/UBSan, sin hallazgos.
- **Riesgo que queda**: la **coherencia de release** (`groups.sh sync`) sigue en rojo: el manifest del WASM
  empaquetado dice commit `78aeb324` y HEAD es otro. No es un fallo de seguridad, es un artefacto viejo: hay que
  reconstruir el emulador con `web/emu/build.sh` (emsdk) para que firmware, paquete y emulador describan el mismo
  commit. Queda anotado, no se declaró PASS.

## 5. Pendientes y riesgos restantes

1. **`[EMU]` WASM desactualizado** (arriba): reconstruir y volver a correr `groups.sh sync`.
2. **`[HOST]` `groups.sh ui`**: corrida en curso; su resultado se anota en el commit.
3. **`[HW]`**: nada de esta etapa se probó en la FM-1 física; todas las clases son host o build del destino.
4. **Issues/PR**: desde este entorno `api.github.com` responde 403, así que no se pudo leer el estado de los
   issues; no se cerró ninguno por suposición. Lo que sí está verificado en el código es lo de arriba.
