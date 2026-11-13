# Informe final — Quantum GammaWave v2.9.1

Cierre de las siete fases: qué está integrado, qué se comprobó y de qué clase es cada comprobación, y
qué queda pendiente y por qué. Escrito el 2026-11-13.

> **Clases de medida usadas aquí:** *compilado*, *medido en host x86*, *emulador*, *build AC791N*, *red*,
> **PENDING**. Nada de lo que sigue se midió en una unidad física: el FM-1 del dueño no se ha escrito, ni
> se ha encendido siquiera desde este repositorio (AGENTS.md §3).

---

## 1. Versión y commits

| | |
|---|---|
| Versión | **v2.9.1** (`FELUCCA_VERSION` en `firmware/src/felucca.c`) |
| Commit de código | **`1e4c31af2e62`** (subida de versión sobre `bb2ee21`, que trae el arreglo de seguridad) |
| Commit de entrega | **el que contiene este documento** (el árbol quedó limpio al escribirlo) |
| Identidad | **`FM-1_965`** en la app; `ota-FM-1_965` en el loader |
| Paquete | `release/felucca-v2.9.1.fwsc`, **610 959 B**, sha256 **`c4d45ab6d372d0be…`** |
| Imagen de la app | `felucca.bin`, **580 404 B** en un slot de **583 612 B** (**3 208 B** de margen), sha256 `c4d45ab6…` |
| Loader | `ota.bin`, 7 803 B, sha256 `3ca785b9…` (idéntico al de 2.9.0) |
| Emulador | `felucca.wasm`, **1 223 264 B**, sha256 `d608efe6…`, build **`1e4c31af2e62`** |
| Registro de build | `release/build-manifest-v2.9.1.json` (`qg1-build-1`, árbol limpio) |
| Manifiesto de release | `release/felucca-v2.9.1.fwsc.manifest.json` (`qg1-release-1`, **`keyid: ""`**: sin firma Ed25519) |

## 2. Cambios integrados

1. **Fase 5 — motores**: `tests/engine_param_test.c` (nuevo) barre los **216 mandos EDIT** de los nueve
   motores a mínimo y máximo en tres presets cada uno, muestra a muestra: **0 sin efecto**. Su modo
   `--levels` mide el RMS de los 267 presets. Sin motores nuevos: con 204 B de flash, GRANULAR (22 051 B
   de buffer contra 15 392 libres), SAMPLE y VOCODER no caben.
2. **Fase 5 — presets**: auditoría de los 267 (9 motores): 0 silenciados, 0 al rojo, 0 duplicados, 0 fuera
   de rango, 265 con movimiento. **No se reescribió ninguno**: el defecto medido (la familia NOISE 12 dB
   por debajo de ANALOG) queda localizado por nombre para cuando haya margen de flash.
3. **Fase 6 — seguridad**: un proyecto importado con la ventana de voces de una pista a 0 hacía `% 0` en
   `voice_alloc` y devolvía un puntero nulo que el llamador desreferenciaba. Lo encontró `fuzz_proj` bajo
   ASan/UBSan (seed 7); corregido, +16 B de flash, y esa semilla quedó en la suite. El informe dice
   también lo que **no** existe: sin firma de paquete, sin anti-rollback, sin arranque seguro.
4. **Fase 7 — espacio**: el slot declaraba `0x8DFBC` por herencia del paquete de fábrica, no porque la
   imagen acabara ahí: la región empaquetada terminaba en `0x8E52A` de `0x8F000`, con **2 774 B que
   nadie direccionaba** (los 3 493 B que circulaban eran una cifra vieja). El slot crece **2 048 B**
   hasta `0x8E7BC`; `FLASH_SIZE`, BTIF y USR no se mueven y la región acaba en `0x8ED2B` con 725 B de
   holgura. Margen de código: **204 B → 3 208 B**.
5. **Fase 7 — sincronización**: README (decía `v2.7.4.7` y 212 sonidos; ahora `v2.9.1` y 267), CHANGELOG,
   capturas regeneradas del render real, `SHA256SUMS` reescrito (tenía una entrada de 2.7.3 y un hash de
   `build/emu`, que no se publica), `tools/generate_screenshots.py` arreglado (usaba un `S_SCL` que ya no
   existe en la UI: la captura de SC/SCALE/CHORD llevaba sin poder generarse desde el renombrado), sitio y
   registro del emulador puestos al día.

## 3. Archivos e imágenes actualizados

- **40 capturas del firmware** en `assets/screenshots/` (HOME, EDIT, FM6, sequencer, rollo, DRUM-X, macro,
  motion, Punch-in, FX rack, mixer, song, escalas/acordes, ARP, parameter locks, visualizador, proyectos,
  performance, master EQ, presets, telemetría, créditos, **9 temas**, splash, delay, reverb×3, HOME×9).
  Todas salen de `tests/ui_render.c`, es decir del mismo código que corre el aparato: no hay mockups.
- **20 piezas** en `assets/ui-art/art/` y **9 imágenes del portal** en `site/assets/` (`fw-home`,
  `fw-matrix`, `fw-browser`, `fw-fx`, `fw-mixer`, `fw-master`, `fw-presets`, `fw-routing`…).
- `site/` regenerado con `web/make_site.py` desde el paquete de release: portal, instalador (247 375 B),
  editor y `firmware/versions.json` con **2.9.1** y 2.9.0 como anterior.
- `site/emulator/` y `release/emu/` con el WASM nuevo y su registro.

## 4. Estado del repositorio

```
coherence: ok — v2.9.1, package and manifest agree, the site serves that package,
           the emulator record matches the build, SHA256SUMS verifies, 1030 files carry no merge marker
```

Sin marcadores de merge, sin referencias a versiones anteriores en el README, `SHA256SUMS` con los cuatro
artefactos reales de esta entrega.

## 5. Estado de la web

`tools/site_check.py`: **all checks passed** — el tamaño y el SHA-256 del paquete son los de
`versions.json`, el emulador declara el commit y la versión que le corresponden, el WASM coincide con el
manifiesto, el portal imprime la versión publicada y el instalador ofrece esa misma versión.

**No verificado:** el sitio *desplegado* no se ha abierto en un navegador desde aquí. El espejo `site/`
está en regla; Pages lo reconstruye CI desde el paquete de release en cada tag.

## 6. Estado del instalador

`python3 tests/install_test.py`: **install tests passed** — el paquete lleva el marcador del loader de
Felucca, la identidad se lee de los bytes (`FM-1_965`), los límites de paquete se respetan
(`web/test_pkg_bounds.mjs`: all checks passed) y la procedencia se comprueba
(`web/test_provenance.mjs`: passed). La restauración desde V15 se salta en este entorno porque el fichero
oficial `FM-1_v15.fwsc` no está en el repositorio.

## 7. Estado del emulador

Cadena completa, cada eslabón con su comprobación:

| Eslabón | Comprobación |
|---|---|
| Código → WASM | `sh web/emu/build.sh` con Emscripten 6.0.11: 1 223 264 B |
| WASM → JS → página | el paquete del emulador se genera en el mismo paso (`index.html`, `worklet.js`) |
| Build visible | el registro lleva `commit 1e4c31af2e62` y `v2.9.1`, y la página lo imprime |
| Emulador ↔ firmware | `coherence_check`: el registro del emulador es el del build |
| Caché | el WASM va con hash en el manifiesto; `site_check` compara tamaño y SHA-256 |

**No verificado:** no se ha abierto el emulador en un navegador real en esta sesión, así que no afirmo
que suene ni que se vea bien: afirmo que compila, que el registro cuadra y que el sitio sirve ese fichero.

## 8. Estado de la release

**El paquete está construido y verificado; la publicación está BLOQUEADA, y no por falta de trabajo.**

`./build.sh --release 2.9.1` desde árbol limpio, mismas entradas que la release anterior:

```
toolchain  jieli-linux-toolchains-20260730.1  clang c45189ee624d…
SDK        AC79NN_SDK_V1.2.1_2023-12-13 (los tres blobs, los que fija tools/build.py)
emcc       Emscripten 6.0.11
```

**El motivo del bloqueo.** `tools/build.py` fija por sha256 los tres blobs del SDK que considera probados
(`uboot.boot 4e3b4c22…`, `cfg_tool.bin 27657995…`, `cfg/eq_cfg_hw.bin 41167491…`). Este build los cumple
—no imprimió el `warning: SDK … differs` que el script emite cuando no—, pero el manifiesto **publicado**
de v2.9.0 declara otros dos: `uboot.boot 64ccc63e…` y `cfg/eq_cfg_hw.bin 179ac9f4…`. Es decir: la 2.9.0
que la gente tiene instalada se construyó con un SDK distinto del que el repositorio considera el probado.

`uboot.boot` es el payload de arranque que viaja dentro del `.fwsc`. Instalar este paquete **cambiaría el
bloque de arranque** respecto de lo que hay en las unidades. El dueño prohíbe tocar el bootloader, y desde
aquí no puedo demostrar que los dos blobs sean equivalentes: no hay unidad, no hay lectura previa y no
puedo reconstruir la 2.9.0 con su SDK original. Por eso:

- **no se ha creado el tag `v2.9.1`** ni se ha publicado release alguna;
- el paquete de `release/` es un **candidato local**, no una release; el sitio desplegado sigue sirviendo
  la 2.9.0 hasta que se resuelva;
- `docs/VALIDATION_MATRIX.md` no cambia de estado: sigue **NO-GO** para instalación pública sin pruebas
  físicas, y ahora con este motivo adicional documentado.

Qué lo desbloquea, en orden: (1) decidir cuál de los dos SDK es el de referencia y corregir el pin de
`tools/build.py` o el manifiesto; (2) reconstruir en el entorno del SDK de referencia y comparar; (3) con
eso resuelto, tag y CI construye, atesta y publica.

## 9. Pruebas ejecutadas en esta fase

| Prueba | Resultado | Clase |
|---|---|---|
| `./build.sh --release 2.9.1` | imagen **580 404 B (1 160 B libres)**, RAM 87 748/98 304, pool spare 15 392 B | build AC791N |
| `sh web/emu/build.sh` | WASM 1 223 264 B, v2.9.1, `1e4c31af2e62` | emulador |
| `tools/coherence_check.py` | ok | host |
| `tools/site_check.py` | all checks passed | host |
| `tests/install_test.py` | passed | host |
| `web/test_web.mjs`, `test_backup.mjs`, `test_pkg_bounds.mjs`, `test_provenance.mjs` | los cuatro pasan | host |
| `tests/ui_test.c` | all passed, 0 dorados cambiados | host |
| `tools/check_fmhub.py` | todas las reglas, ok | red |
| `tools/check_identity.py` | `FM-1_965` libre, catálogo de 198 fichas | red |
| `engine_param_test`, `preset_audit_test`, `ratchet`, `swing`, `scale`, `chord_scale`, `motion`, `perform`, `midi_control`, `midi_uart`, `editor`, `tempo`, `slicer`, `six_track_live`, `ota`, `ota_cut`, `storage`, `storage_cut`, `flash_id`, `robust`, `upreset`, `fuzz_ed`, `fuzz_proj` | todos pasan | host x86 (ASan/UBSan en los dos fuzz) |

## 10. Pendientes y por qué

| Pendiente | Motivo |
|---|---|
| **Tag y release de v2.9.1** | discrepancia de blobs del SDK (§8): el paquete cambiaría el bloque de arranque |
| **Los tres ensayos en un FM-1** (desde V15, desde otro firmware, salida hacia otro) + rescate USB | sin hardware de laboratorio; la unidad del dueño no se escribe |
| **Nivel de la familia NOISE** (−29,7 dBFS contra −17,9 de ANALOG) | necesita bytes de flash (204 B) y oídos |
| **SID / ACID / POLY como presets** | caben como presets, pero no hay margen para añadirlos ahora |
| **`tests/test_polyphony.c`** | no compila (copia muerta de `quiet_tail`) y no está en la suite; reportado, no borrado |
| **Restauración desde V15 en el instalador** | el fichero oficial no está en el repositorio |
| **Verificación de la atestación de CI** (`gh attestation verify`) | este entorno no tiene `gh` con atestaciones ni token; es un job del workflow |

## 11. Lo que este informe no dice

No dice que el firmware funcione en un FM-1, que el emulador suene, que el sitio desplegado se vea bien,
que el Hub haya aceptado nada, ni que haya firma digital. Nada de eso se comprobó. Lo que sí dice, con su
clase de medida, es que el árbol es coherente consigo mismo, que los paquetes y manifiestos cuadran, y que
la única cosa que impide publicar no es código: es un blob de arranque que no sé si es el mismo.
