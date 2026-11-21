# Etapa 8/8 — cierre, publicación y compatibilidad V16

Clases: **`[HOST]`** x86 (`tests/*`, `groups.sh`, node), **`[TARGET]`** build del destino AC791N,
**`[EMU]`** el WASM del emulador, **`[STATIC]`** lectura del árbol, **`[HW]`** la FM-1 física.
En esta etapa **no hubo `[HW]`**: no hay unidad conectada a este entorno.

## 1. Versión final

| | |
|---|---|
| Versión | **v1.6.0** (`FELUCCA_VERSION`, `firmware/src/felucca.c`) |
| Identidad | **`FM-1_965`** (sin cambios: `docs/IDENTITY.md`) |
| Commit del paquete | **`11b9b9edebdd`** (el build dice `v1.6.0 at 11b9b9edebdd`, sin `dirty`) |
| Paquete | `release/felucca-v1.6.0.fwsc` — **610 933 B** |
| SHA-256 del paquete | `f6aa3b15e4805d94447360694aacd7c6f62fdde5ac2c71a8a9ccae7bf66506f2` |
| `felucca.bin` | 582 884 B, `450fc110…` según el registro del build |
| loader `ota.bin` | 7 777 B |
| Emulador | `site/emulator/felucca.wasm` — 1 291 917 B, `5ba835f5…`, build `11b9b9edebdd`, `[EMU]` |
| Paquete del emulador | `release/emu/qg1-emulator-v1.6.0.zip` |

`python3 tools/make_release.py --check` → **ok**: `release/`, `site/`, el instalador, el editor y los dos
`SHA256SUMS` describen un solo paquete. `tools/build_manifest.py --verify` → **ok** (versión, commit, config,
artefactos y emulador coinciden); `tests/manifest_test.py` → **passed** (12 checks).

## 2. Compatibilidad e instalación desde el V16 oficial

La investigación es de la Fase 5 y esta etapa la **re-verificó ejecutando sus pruebas**, no leyéndolas.

### 2.1 El error reportado, con su causa

> `Chip: this unit did not answer its JEDEC Truncated identification reply: nothing was sent.`

Era una frase **compuesta** por dos defectos distintos, los dos en la página, ninguno en el protocolo:

1. **La etiqueta era única para todos los desenlaces.** El botón CHIP escribía el motivo del fallo pegado al
   texto fijo, así que un **eco** del propio frame o un cuerpo de 3 bytes se informaban como «no respondió» —
   afirmando silencio en una unidad que sí había contestado algo.
2. **La versión se le pasaba a mano.** La página daba `"15"` como firmware a cualquier unidad stock, así que una
   `FM-1_016` se rechazaba con «the device is FM-1_016, the package is for FM-1_965».

Hoy: `stockVersionOf("FM-1_016")` → `"16"` (la versión la dice la unidad) y `probeChip()` separa **eco**, **cuerpo
de tamaño distinto de 24**, **silencio**, **CRC**, **negativa del NOR**, **JEDEC desconocido** y **geometría**,
cada uno con su frase. `web/test_v16_path.mjs` verifica que la composición del informe **ya no se puede
producir** y que las tres etiquetas (`chipNo`, `chipOther`, `chipOk`) existen en los tres idiomas con separador.

### 2.2 Qué identifica a una unidad V16 y qué no

- **`FM-1_016` es identidad MIDI/app**, no identificación de parte. El `0x12` es un comando **de este
  firmware**; una unidad stock no lo implementa, así que **no hay JEDEC que leer** y la página no lo inventa.
- El id de parte lo lee el **loader del paquete** en la unidad antes de borrar (`firmware/loader/loader.c`,
  `ldr_flash_known()`), contra la lista explícita de dos partes: **`0x856014` (Puya P25Q80H)** y
  **`0xB36014` (UCUN UC25HQ80)**. Cualquier otro id se rechaza; no se agregó ninguno.
- El registro de M-Flasher de una unidad V16 real (`docs/FASE5_BACKUP_V16.md`) dice `flash 0x856014`,
  `chip key 0x980F`, `bootloader 2.3`, `firmware FM-1 016 (stock)`: la **misma familia de parte** que este
  proyecto ya escribe, en la generación de bootloader que el camino V16 documenta.

### 2.3 El camino V16, tal como está implementado

1. Copia **completa de 1 MiB** de esa unidad con M-Flasher (o su registro `.json` + el `.bin`, que se atan por
   SHA-256 en el navegador).
2. Confirmación de que la copia está guardada y se sabe restaurar.
3. `FM-1.fwsc` **oficial V16** cargado como camino de vuelta (fijado por digest).
4. Preflight + INSTALL. Se escribe **`0x4000–0x92FFF`**; el head `0x0000–0x3FFF` (bootloader/SPL del V16) **no se
   toca**, y nada por encima de `0x93000` (en un V16 eso deja intactos el final de `DX7.bin`, `VM`, `SEQ64`,
   `BTIF`).

Sin copia, sin confirmación o sin el V16 oficial **no se envía nada**, y la página dice cuál falta. El gate está
en **GO CONDITIONAL**: `gate.backup` pass, `gate.recovery` **conditional** (el procedimiento existe, la
restauración sobre un layout V16 **no se ha observado**), `gate.chip` at-install.

### 2.4 Qué está verificado y qué no

| Afirmación | Clase | Resultado |
|---|---|---|
| La página toma la versión de la identidad de la unidad | `[HOST]` node | pass (`web/test_v16_path.mjs`) |
| Eco / cuerpo corto / silencio / CRC / negativa / id desconocido / geometría se distinguen | `[HOST]` | pass |
| El mensaje del informe no se puede componer; tres desenlaces, tres frases, tres idiomas | `[HOST]` | pass |
| V16 es donante con su propia versión y se rechaza con otra | `[HOST]` | pass (`test_donor`) |
| Una copia sin unidad, de otra unidad o que no coincide con el registro no pasa el gate | `[HOST]` | pass |
| El paquete QG cabe en la ventana del loader | `[TARGET]` (`tools/fm1pkg_make.py` falla si no) | pass |
| Verify Only: 175 checks del informe de la página | `[HOST]` | pass (`web/test_verify_only.mjs`) |
| El updater del V16 acepta el paquete y arranca el loader | **`[HW]`** | **no medido** |
| El SPL del V16 arranca la app de QG | **`[HW]`** | **no medido** |
| Restaurar la copia sobre un layout V16 | **`[HW]`** | **no medido** |

**No se declara «instala desde V16» sin unidad.** Lo que sí se declara: el software ya no compone el mensaje
erróneo, distingue cada desenlace, toma la versión de la unidad y no manda nada sin copia y vuelta atrás
verificadas.

## 3. Verificación ejecutada en esta etapa (`[HOST]` salvo donde se indique)

| Grupo / prueba | Resultado |
|---|---|
| `groups.sh sync` | **0** — emulador v1.6.0, build `11b9b9edebdd`, 1 291 917 B, página y manifiesto de acuerdo |
| `groups.sh presets` | **0** — 287/287, una categoría primaria cada uno |
| `groups.sh web` | **0** — sitio, i18n, instalador, editor vs firmware, MIDI export (64 checks) |
| `groups.sh target` | **0** — build AC791N + `target_budget` + `memory_map_test` (RAM spare 4 692 B, pool 9 248 B, RAMTEXT 4 848 B, NOINIT 124 B) |
| `groups.sh security` | **0 todo PASS** — instalador, preflight, gate, firmas, custodia, restore, `pkg_bounds`/`pkg_faults`, fuzz ASan/UBSan, cortes de storage y OTA, eq ×4, audio |
| `groups.sh ui` | **0** — 191 pantallas × 22 paletas, 0 lint findings, 3 787 294 medidas de alineación y 0 fuera de 1 px, GREY/MONO sin píxeles fuera de gris |
| `tests/install_test.py` | **0** |
| `node web/test_web.mjs` / `test_v16_path.mjs` / `test_verify_only.mjs` | **0 / 0 / 0** (175 checks) |
| `tools/make_release.py --check` | **ok** |
| `tools/build_manifest.py --verify`, `tests/manifest_test.py` | **ok / passed** |
| `groups.sh stress` | **0** — `regress` 2 pasadas completas, project/persistence/robust/midi_control |
| `regress` | 307 goldens, 0 cambiados |

## 4. Optimización: medidas y decisión

| Recurso | Al empezar la Etapa 1 | Final (v1.6.0, `[TARGET]`) | Margen |
|---|---|---|---|
| Slot de app (XIP) | 583 612 B, libre ≈ 2 054 B | **581 296 B usados, 2 316 B libres** | +262 B recuperados en las etapas 1–6 |
| `.data+.bss` | 98 304 B de presupuesto, ≈ 4 700 B libres | **93 612 B, 4 692 B libres** | piso 8 KiB del pool intacto (9 248 B) |
| code RAM (`.ram_text`) | 24 576 B | **19 728 B, 4 848 B libres** | — |
| Constantes XIP | 133 796 B | **130 880 B** | −2 916 B |
| Coste por muestra (`[HOST]`, no AC791N) | — | `mix` max 1 574 instr/muestra; el motor más caro, PHASE 1 558 | el bloque de 32 muestras sigue bajo el presupuesto de CPU del destino |

Lo ya aprovechado en las etapas anteriores: la curva `exp2` a 2 054 B en `.bss` en vez de 4 100 B de XIP, la
senoide como 1 023 B de diferencias, el cache de rampas de texto construido una vez, el `CHANCE` que dibuja el
patrón entero en vez de un valor suelto. **Esta etapa no encontró una optimización nueva que se pueda tomar sin
arriesgar audio, polifonía o calidad**; con 2 316 B de slot libre el cambio que quedaría (mover otra tabla a
RAM) compraría ~1 KB a cambio de RAM de audio, y el recurso más limitado del proyecto pasó a ser **RAM**
(4 692 B libres contra un piso de 4 096 B para el pool). Se documenta la medida y **no se toca el DSP por
corazonada** (AGENTS.md §8).

## 5. Bugs corregidos en esta etapa y estado de los tests largos

- **`tests/groups.sh target` sólo funcionaba con el toolchain copiado a `/tmp`.** El grupo comparaba contra
  `/tmp/jieli/toolchain` y `/tmp/ac79` ignorando `JIELI_TOOLCHAIN`/`AC79_SDK`, así que el build del destino no se
  podía correr donde el entorno está exportado (AGENTS.md §6). Ahora las variables mandan y `/tmp` queda como
  respaldo; el grupo corre y pasa.
- La auditoría de la Etapa 7 (cuatro defectos: arnés de audio sin `sine_init`, afirmación del EQ, baseline de ns
  en `stress`, orden de las divisiones del LFO) está en `docs/ETAPA7_AUDITORIA_2026-11-21.md`, corregida y
  re-verificada aquí en el paquete final.

Los dos grupos largos (`security`, `ui`) y `stress` se ejecutaron sobre este árbol y terminaron con **exit 0**.

## 6. Publicación

- Paquete, manifiesto, registro del build y `SHA256SUMS` en `release/`; sitio, instalador, editor y emulador en
  `site/`, todos describiendo el mismo paquete (`make_release --check`).
- El tag `v1.6.0` y la GitHub Release los crea `tag-and-publish.yml` al pushear `main`: verifica el paquete
  contra su propio manifiesto antes de crear el tag, lo attesta y despacha `release.yml`. Es idempotente: un tag
  o release existente se informa y no se mueve.
- GitHub Pages sirve `site/` desde el workflow del repositorio; el portal, el instalador y el emulador se
  actualizan en el mismo push.

## 7. Pendientes reales

1. **`[HW]`** las tres afirmaciones de §2.4 que necesitan una unidad V16: updater→loader, SPL→app de QG y
   restauración de la copia. Sin ellas el gate sigue **GO CONDITIONAL**.
2. **`[HW]`** el JEDEC leído por el loader de este proyecto en una unidad física: el `0x856014` conocido viene del
   registro de M-Flasher, no de nuestra consulta.
3. `api.github.com` responde 403 desde este entorno: los Issues y PR no se pudieron leer ni cerrar; no se cerró
   ninguno por suposición.
