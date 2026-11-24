# FASE 9/9 — Cierre integral, seguridad y release (2026-11-24)

Versión de este árbol: **v1.11.1** (`FELUCCA_VERSION`, `firmware/src/felucca.c`), identidad de paquete
`FM-1_965`. Es **un parche sobre la línea 1.11**: 1.11.0 → 1.11.1, sin renumerar, sin saltar números y sin
tocar el primer número (docs/VERSIONING.md).

Clases de evidencia, como en AGENTS §4: `[HOST]` nativo x86-64, `[TARGET]` build AC791N (pi32v2 clang +
AC79 SDK), `[WASM]` Emscripten, `[STATIC]` inspección, `[PUBLIC]` leído de las URLs públicas. **No hubo
unidad FM-1 física en ningún momento**: nada de este documento es una medición de hardware.

---

## 1. Lo que se auditó y lo que se encontró

### 1.1 Seguridad del instalador y del paquete

| Requisito | Dónde vive | Estado |
|---|---|---|
| SHA-256 + tamaño del paquete contra el manifiesto, antes de habilitar INSTALL | `web/fm1ota.js` (`admit`), `site/webapp/installer/index.html` | `[HOST]` probado (`web/test_verify_only.mjs`, `tests/install_test.py`, `tests/preflight_test.py`) |
| El paquete se re-deriva del build: commit, config, hashes de app y loader | `release/build-manifest-v1.11.1.json`, `tools/build_manifest.py --verify` | `[TARGET]` + `[HOST]` |
| Identificación real del flash antes de escribir (0x12 en la misma sesión) | `tools/fm1_gate.py`, `web/fm1ota.js` | `[HOST]` (`tests/chip_test.py`) |
| Allowlist exacta de dos partes de 1 MiB | `0x856014` P25Q80H, `0xB36014` UC25HQ80 | `[STATIC]` + `[HOST]`; **no leídas de una unidad aquí** |
| Partes conocidas pero no validadas para escritura (GD25Q80, W25Q80, ZB25VQ80, BY25D80, XM25QH80B) | `READ_ONLY_JEDEC` | solo lectura/backup; escritura rechazada |
| JEDEC en blanco (`0xFFFFFF`, `0`), ausente o tecleado a mano | `fm1_gate.py`, `fm1ota.js` | rechazado; no existe flag que lo sustituya |
| Ventana de escritura del loader `0x4000–0x92FFF` respetada por el paquete | `LOADER_APP_LO/HI`, `LDR_APP_*` | `[HOST]` (`web/test_pkg_bounds.mjs`) |
| Paquete que sale de la ventana o sin marcador de loader | `loaderWindowIssue`, `hasFeluccaLoader` | rechazado antes de cualquier frame |
| V16 (`FM-1_016`, 718 196 B) como **destino de restauración**, nunca como contenedor | `web/fm1pkg.js` `STOCK_RELEASES` | fijado por SHA-256; su área de app termina en `0xA8071` y el loader corta en `0x93000` |
| "Verify only" no escribe: un solo frame de identidad | `web/fm1verify.js` | `[HOST]` (`web/test_verify_only.mjs`) |
| Backup completo antes de escribir; restauración verificada por objeto | `web/fm1backup.js`, `tools/fm1_archive.py` | `[HOST]` (`web/test_backup.mjs`, `tests/backup_tool_test.py`) |
| Recuperación: loader propio como camino de vuelta; jamás el bootloader | `firmware/loader/`, `tools/fm1_mflasher.py` | `[TARGET]` compilado; **recuperación no ejecutada en unidad** |
| Límites de escritura: `MAX_PACKAGE = 0x100000`, sin writes parciales silenciosos | `fm1_gate.py`, `ldr_core.c` | `[HOST]` + `[TARGET]` |
| Paquetes inválidos: truncados, sin fin de lista, con otra clave de chip, con app fuera de la imagen | `web/test_pkg_bounds.mjs`, `web/test_pkg_faults.mjs` | rechazados (`[HOST]`) |
| Firma Ed25519 | histórica (`qg1-release-1`); el release actual es **sin firma por diseño** | integridad por SHA-256 + build record; no se presenta como autenticidad |

**Conclusión de seguridad:** no se relajó ninguna compuerta, no se añadió ningún bypass y no se eliminó
ningún mecanismo de recuperación. Los dos únicos defectos encontrados en esta fase estaban en metadatos y
nombres (abajo), no en el camino de escritura.

### 1.2 Coherencia de versión y publicación (lo que estaba mal)

| Hallazgo | Antes | Ahora |
|---|---|---|
| README anunciaba una versión que el árbol no era | `v1.8.6` | `v1.11.1` |
| README citaba paquetes de otra numeración | `v3.1.1`, `v2.9.7` | el paquete de este árbol, sin números inventados |
| `web/emu/manifest.json` era un registro de build **v2.7.3** commiteado | 1 112 286 B, otro commit | eliminado: el registro real es `site/emulator/manifest.json` y `release/emu/manifest.json`, y ambos se generan |
| `gmon.out` (volcado de perfilado, 427 KiB) en la raíz | commiteado | eliminado |
| `tools/fm1_probe.py` nombraba la parte UCUN como `P25Q80H-2` | nombre inexistente | `UC25HQ80` |
| `docs/INDEX.md` apuntaba a un estado de rendimiento de 4.1.0 y a rutas sin `tests/` | desactualizado | apunta a `OPTIMIZATION_BASELINE.md` y `tests/panel_table_test.py`, y suma este informe |
| `docs/VALIDATION_MATRIX.md` declaraba "current release v1.6.0" | contradecía al árbol | cabecera corregida a esta fase |

### 1.3 Limpieza de código y assets

- Eliminados: `gmon.out`, `web/emu/manifest.json` (registro generado y obsoleto).
- Reescrito el comentario de `FELUCCA_VERSION`: llevaba dos bloques históricos superpuestos (1.6.0 y
  v3.1.2) que se contradecían. Ahora dice qué es la constante y qué significa el parche.
- Los paquetes `v1.12.x` y los tags `v2.x–v4.x` **se conservan**: están publicados y AGENTS §5 prohíbe
  renombrar o borrar artefactos publicados. La regla nueva (solo 1.x.x) está escrita en
  `docs/VERSIONING.md`.
- No se tocó ningún asset con licencia, crédito ni `LICENSES/`.

---

## 2. Optimización y benchmarks (comparativo real)

Ninguna optimización de DSP, del panel ni de la UI en esta fase: la instrucción es cerrar sin sacrificar
calidad, y cambiar el camino de audio sin unidad física contradice AGENTS §8. Lo que sí se midió es que el
parche **no costó recursos**.

| Métrica | v1.11.0 | v1.11.1 | Δ |
|---|---|---|---|
| Imagen de app | 581 508 B | 581 508 B | 0 |
| Libre en el slot de app (suelo 2 048) | 2 104 B | 2 104 B | 0 |
| `.data+.bss` (presupuesto 98 304) | 94 144 B | 94 144 B | 0 |
| RAM libre (suelo 4 096) | 4 160 B | 4 160 B | 0 |
| Pool / holgura (suelo 8 KiB) | 311 256 / 9 248 B | 311 256 / 9 248 B | 0 |
| XIP: código + constantes | 446 556 + 130 152 B | 446 556 + 130 152 B | 0 |
| Código en RAM | 3 940 B de 24 576 | 3 940 B de 24 576 | 0 |
| Loader `ota.bin` | 7 777 B | 7 777 B | 0 |
| Paquete `.fwsc` | 610 933 B | 610 933 B | 0 |
| Emulador WASM | 1 316 630 B | 1 316 630 B | 0 |
| Presupuesto de render por bloque (`target_budget.py`) | en verde | en verde, `DRUM_PATH` 1 155/1 153 (+0 %), `BLOCK_PATH` 33 433/45 704 (**−27 %**) | igual |

`DRUM_PATH` sigue 2 instrucciones por encima de su presupuesto nominal y dentro del 10 % aceptado; es el
mismo valor que traía la línea y no se tocó. `BLOCK_PATH` tiene un 27 % de holgura: se deja anotado como
candidato a presupuesto más ajustado, no se cambia el baseline en una fase de cierre.

---

## 3. Bugs corregidos

1. **El árbol no decía su propia versión en la cara pública** (README `v1.8.6`, instalador citando
   `v3.1.1`/`v2.9.7`): corregido, y el número ahora sale de `FELUCCA_VERSION`.
2. **Registro de emulador obsoleto commiteado** (`web/emu/manifest.json`, v2.7.3): eliminado; la
   verificación de coherencia compara `release/emu/manifest.json` con el build record.
3. **Nombre de parte de flash incorrecto** en `tools/fm1_probe.py` (`P25Q80H-2` → `UC25HQ80`).
4. **Golden renders desactualizados** al cambiar el número: 48 frames de splash/about regenerados con
   `tests/ui_pixels.py --update`; 3 129 frames, solo esos 48 difieren (verificado).

---

## 4. Pruebas ejecutadas en esta fase

Todo lo de abajo se ejecutó en Linux x86-64. La suite nativa completa (`tests/run_tests.sh`, sin
`NO_PACKAGE`, con el paquete real presente) se corrió sobre el árbol de esta release y terminó en
**`ALL HOST TESTS PASSED`: 6 514 comprobaciones `ok`, 0 fallos**, con ASan/UBSan, los golden renders y el
lint de alineación. La suite **saltó** el bloque del emulador porque comprueba `emcc` en el PATH y aquí se
instala con `emsdk_env.sh`; se ejecutó aparte, con emsdk 6.0.11: `web/emu/emu_test.mjs` da **`emu: all ok`**
(incluye `bit for bit` contra el build nativo, 88 200 frames, y 2,5 % de tiempo real para 1 s de la canción
pesada).

| Grupo | Comando | Clase | Resultado |
|---|---|---|---|
| Suite nativa completa (host, ASan/UBSan, golden renders, lint, presets, DSP, UI) | `bash tests/run_tests.sh` | `[HOST]` | **6 514 ok, 0 fallos, ALL HOST TESTS PASSED** (el bloque del emulador se saltó: no ve `emcc`) |
| Emulador: arranque, teclas, MIDI, pantalla, LEDs, PLAY, guardado entre instancias, paridad con el build nativo | `node web/emu/emu_test.mjs build/emu/felucca.wasm …` | `[WASM]` | `emu: all ok`; 88 200 frames bit a bit; 24,9 ms por 1 s de audio |
| Instalador CLI, paquete real, marcador de loader, paridad con `fm1pkg.js` | `python3 tests/install_test.py` | `[HOST]` | pasa |
| Preflight: sin evidencia de chip no hay frame de escritura | `python3 tests/preflight_test.py build/felucca.fwsc` | `[HOST]` | pasa |
| Identificación de flash y backup | `python3 tests/chip_test.py` | `[HOST]` | pasa |
| Firma: válida, manipulada, clave desconocida, downgrade | `python3 tests/sign_test.py` | `[HOST]` | pasa |
| Restauración: archivo usable, truncado, sin programador | `python3 tests/restore_test.py` | `[HOST]` | pasa |
| Límites de paquete y fallos de paquete | `node web/test_pkg_bounds.mjs`, `web/test_pkg_faults.mjs` | `[HOST]` | pasa |
| Backup web / restore | `node web/test_backup.mjs` | `[HOST]` | pasa |
| Página: paquete, WASM, installer | `node web/test_web.mjs` | `[HOST]` | pasa |
| Coherencia del árbol | `python3 tools/coherence_check.py` | `[HOST]` | ok |
| Release set (paquete, sums, sitio, instalador, editor, emulador) | `python3 tools/make_release.py --check` | `[HOST]` | ok |
| Sitio | `python3 web/test_site.py site` | `[HOST]` | 20 ids, 10 imágenes, 31 enlaces, 0 fallos |
| Presupuesto de render | `python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt` | `[TARGET]` | en verde |
| Build AC791N + paquete + loader | `./build.sh --release 1.11.1` | `[TARGET]` | imagen 581 508 B, paquete 610 933 B |
| Emulador | `sh web/emu/build.sh` | `[WASM]` | WASM 1 316 630 B, emcc 6.0.11 |
| Golden UI | `python3 tests/ui_pixels.py` | `[HOST]` | 3 129 frames idénticos |

**No ejecutado (y por eso no se declara):** ninguna transferencia, arranque, lectura de flash, backup,
restauración ni recuperación sobre una FM-1 física; ninguna medición de reloj, CPU o audio en la unidad;
ninguna prueba de las dos partes de flash sobre hardware. `tests/memory_map_test.py` se salta sin
`build/felucca.elf` + objdump de JieLi, y así se informa.

---

## 5. Lo que queda pendiente de verdad

1. **Evidencia física** para la matriz de instalación: identidad de la unidad, JEDEC real, geometría,
   compatibilidad de escritura, mapa aplicable, backup previo verificado, recuperación demostrada y
   revalidación posterior. Sin eso, el estado sigue siendo `GO CONDITIONAL`/`NO-GO` según
   `docs/VALIDATION_MATRIX.md`; este release no cambia ninguna de esas celdas.
2. **V16 en unidad**: el archivo oficial está medido en host y fijado por hash, pero nadie lo ha instalado
   aquí.
3. `DRUM_PATH` 2 instrucciones sobre su presupuesto nominal (dentro del 10 % permitido).
4. El aviso `QUEUE FULL` de la cola MIDI/SysEx sigue fuera del firmware por tamaño (documentado en
   v1.11.0).
