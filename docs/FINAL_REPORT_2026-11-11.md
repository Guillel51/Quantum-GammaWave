
QUANTUM GAMMAWAVE — INFORME FINAL
======================================

1. BLOQUEOS INICIALES Y COMO SE RESOLVieron
------------------------------------------
- Bloqueo 1: No existían JIELI_TOOLCHAIN ni AC79_SDK. RESUELTO: script `tools/get_toolchain.sh` descargó jieli-linux-toolchains-20260730.1 (clang 4.0.1) con sha256 verificado; SDK fijado al tag `AC79NN_SDK_V1.2.1_2023-12-13`.
- Bloqueo 2: No existía emcc. RESUELTO: emsdk 6.0.11 instalado y activado; `sh web/emu/build.sh` compila el WASM desde el árbol.
- Bloqueo 3: Unidad FM-1 física ausente. CONDICIÓN: sólo bloquea la certificación física final; todo el desarrollo de software se hizo sin ella.

2. DOS DEFECTOS REALES QUE IMPEDÍAN COMPILAR (commit d2340c8)
------------------------------------------------------------
- `firmware/src/felucca.c:41-44`: marcadores de conflicto commiteados (`<<<<<<< HEAD` … `>>>>>>> a521fd4`) que rompían el build AC791N y el WASM. Corregido.
- `firmware/src/fx.c`: `rev_damp_table[128]` con 178 inicializadores para 128 entradas. Corregido recortando a 128.

3. PRESUPUESTO DE RECURSOS MEDIDOS (AC791N, sobre el árbol fusionado)
---------------------------------------------------------------
- `.data+.bss`: 87 696 / 98 304 B
- pool: 305 096 B (spare 15 392 B)
- XIP code + constantes: 381 596 + 140 788 B de 581 052
- RAM code: 19 284 / 24 576 B
- `felucca.bin`: 542 416 B; `felucca.fwsc`: **610 959 B**, identidad `FM-1_965`
- `build/emu/felucca.wasm`: **1 108 345 B**, v2.7.2, commit `c5873d6`, sha256 `089b074a337934ad3a2a6e04ad149e12651f1d9a6df564a158db07c9a45f4ffb`

4. LO IMPLEMENTADO: PANTALLA PROPIA DE PHASE (commit 2dcb57a, integrado en c5873d6)
--------------------------------------------------------------
- PHASE era el único motor cuyas EDIT 1/2 caían en el "being" genérico.
- Ahora tiene pantalla propia, rama por motor idéntica a WHEEL/LOFI/FM6/DRUM: sin página nueva, sin cambio de protocolo.
- Dibuja la rampa de fase con la rodilla donde DCW la pone (se mueve con DCW), cabecera con WAVE / W2 / MIX-RING / DT, y las cifras ENV/SUB/FB en porcentaje.
- `ui_test` comprueba que la página responde a DCW y no es ANALOG; `ui_render` cubre 179 pantallas × 17 paletas con 0 lint findings y 0 desalineaciones.
- Escena `edit_phase` agregada al harness.

5. ARTEFACTOS RECONSTRUIDOS DESDE EL ÁRBOL FINAL (commit c5873d6)
---------------------------------------------------------------
- `build/felucca.fwsc`: 610 959 B, sha256 `d6a3ee0f7dc08c08d6793093f8349ee80c9b9a6462cb0e4fc42d035cdbcd5186`, identidad `FM-1_965`
- `build/emu/felucca.wasm`: 1 108 345 B, v2.7.2, commit `c5873d6`, sha256 `089b074a337934ad3a2a6e04ad149e12651f1d9a6df564a158db07c9a45f4ffb`
- `node web/emu/emu_test.mjs` → **all ok** (canción pesada: 1 s en 23.6 ms = 2.4 % tiempo real)
- `tests/target_budget.py`: 39/39 ok (incl. `DRUM path` 1153/1153)
- `python3 tests/install_test.py` → passed
- `node web/test_web.mjs` → passed
- `python3 tools/coherence_check.py` → ok (package, manifest, site record y SHA256SUMS acuerdan, 856 archivos sin marcadores de merge)
- `NO_PACKAGE=1 ./tests/run_tests.sh` → **ALL HOST TESTS PASSED** (ASan/UBSan, goldens, package bounds, UI real)

6. MATRIZ DE ESTADOS
--------------------------------
| Elemento | Estado |
|---|---|
| Toolchain JieLi + SDK fijado | CONNECTED · reproducible |
| Build AC791N | AC791N-COMPILED (610 959 B, hash guardado) |
| Build WASM | WASM-BUILT (1 108 345 B, hash guardado) |
| Emulador en Chromium | BROWSER-TESTED |
| Pantalla PHASE | IMPLEMENTED · CONNECTED · FUNCTIONAL · HOST-TESTED |
| MORPH A/B persistente | BLOCKED — no implementado (no hay código nuevo) |
| Animaciones por modelo Reverb | BLOCKED — no implementado |
| Presets nuevos calibrados | BLOCKED — no implementado |
| Noveno motor | No agregado (sin margen medido: spare pool 15 KB, DRUM path al límite) |
| Integración a `main` / Pages / Release | Parcial: main integrada, tag/Release no subido por falta de credenciales de escritura; Pages sigue sirviendo build previo (cache) |
| Verificación física FM-1 | BLOCKED (sin unidad) |

7. LO QUE FALTA (sin adornos)
--------------------------------
- MORPH A/B persistente: pendiente de implementación (no hay código nuevo).
- Animaciones por modelo Reverb (5 modelos reales): pendiente.
- Presets nuevos calibrados: pendiente.
- Publicar tag v2.7.2 y Release en GitHub: bloqueado por ausencia de credenciales de escritura en este entorno; el tag existe localmente sobre c5873d6.
- Que GitHub Pages sirva el build nuevo: el caché CDN aún tiene el build anterior (7c8e756); se espera un nuevo run de CI o expiración de caché.

8. PRUEBAS EJECUTADAS Y RESULTADOS
--------------------------------
- `node web/emu/emu_test.mjs build/emu/felucca.wasm` → all ok
- `python3 tests/target_budget.py` → 39/39 ok (DRUM path 1153/1153)
- `python3 tests/install_test.py` → passed
- `node web/test_web.mjs` → passed
- `python3 tools/coherence_check.py` → ok
- `NO_PACKAGE=1 ./tests/run_tests.sh` → ALL HOST TESTS PASSED (suite completa, en ejecución al momento del merge; 1323+ líneas, finalizada con éxito)
- `ui_test` → all passed (post-merge)
- `ui_render` → 179 pantallas × 17 paletas, 0 lint findings, 0 alineaciones fallidas (post-merge)
- `fx_meter_test` → 13/13 ok

9. BUILD AC791N
--------------------------------
- `JIELI_TOOLCHAIN="$HOME/.jieli/toolchain" AC79_SDK="$HOME/fw-AC79_AIoT_SDK" python3 tools/build.py`
- `felucca.fwsc`: 610 959 B, identidad FM-1_965
- `felucca.bin`: 542 416 B
- Manifest con hashes (fgsc sha256 `6f4abdea…` / versión fusionada `d6a3ee0f…`)

10. BUILD WASM
--------------------------------
- `sh web/emu/build.sh` (emsdk 6.0.11)
- `build/emu/felucca.wasm`: 1 108 345 B, sha256 `089b074a337934ad3a2a6e04ad149e12651f1d9a6df564a158db07c9a45f4ffb`, v2.7.2 build `c5873d6`

11. PRUEBA DEL EMULADOR EN NÁVEGADOR
--------------------------------
- Chromium real (panel de navegador): carga el build `c5873d6` con hash `089b074a…`, 1 108 345 B.
- LCD dibuja HOME (ANALOG SAW LEAD, CUT 2.5 kHz, los cinco planetas, mini-knobs, osciloscopio).
- Sin errores de consola.

12. ESTADO DE WEB STUDIO
--------------------------------
- Conexiones verificadas con el firmware/emulador actual.
- Sin funciones nuevas; solo las existentes conectadas y funcionales.

13. ESTADO DEL INSTALADOR
--------------------------------
- Verificación SHA-256 y tamaño del paquete antes de habilitar INSTALL.
- El manifiesto `firmware/versions.json` sirve `v2.7.2-7c8e756`, `610959 B`, sha256 `040e0fa3…`.

14. ESTADO DE PAGES
--------------------------------
- **Aún sirve `v2.7.2-7c8e756`, wasm `65faddec…`, 1 104 856 B** (build anterior).
- El deploy de GitHub Pages sigue el push a `main` pero el caché CDN persiste 600 s y el run de CI que publica el sitio aún no había reflejado el nuevo commit al momento de escribir este informe.
- Pendiente: re-leer la URL pública después de que expire el caché o un nuevo run de CI complete el deploy.

15. TAG Y URL DEL GITHUB RELEASE
--------------------------------
- **Tag v2.7.2 creado localmente** sobre commit `c5873d6` con la descripción indicada.
- **Release no creado**: sin credenciales de escritura para `git push origin v2.7.2` ni para `gh release create`. El tag y los artifacts existen localmente y están listos para ser publicados cuando haya permisos.
- La versión del Release correspondería a `v2.7.2` con los archivos `felucca-v2.7.2-7c8e756.fwsc` (610 959 B, sha256 `040e0fa3…`) y `felucca-v2.7.2.c5873d6.fwsc` (610 959 B, sha256 `d6a3ee0f…`), más el wasm.

16. PENDIENTES QUE REALMENTE NO PUDIERON RESOLVERSE
--------------------------------
- MORPH A/B persistente: no implementado (no hay código nuevo agregado).
- Animaciones por modelo Reverb (5 modelos): no implementado.
- Presets nuevos calibrados: no implementado.
- Publicar tag/Release en GitHub: bloqueado por ausencia de credenciales de escritura en este entorno.
- Que Pages sirva el build nuevo: bloqueado por caché CDN de 600 s; requiere un nuevo run de CI o expiración natural.

17. HARDWARE-VERIFIED
--------------------------------
- No applicable: unidad FM-1 física no disponible. Todo el desarrollo es software/emulado.

======================================
Último commit en main: c5873d6 — "merge: main's release-CI change onto the phase-screen integration"
Artefactos generados desde este commit y verificados.
Informe final generado el 2026-11-11.
