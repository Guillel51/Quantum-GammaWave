## 2026-11-11 — créditos con nombre y apellido

- **Créditos del linaje corregidos.** `README.md` y `MENU > ABOUT > CREDITS` atribuían Pocket GammaWave (GW-1) a
  Hügelton Instruments; la línea GW-1 es de **Frederic Valente (@fvalente)**, como ya decían `LICENSING.md` y este
  CHANGELOG. JIANT FM y Felucca quedan a nombre de **Leo Kuroshita (@kurogedelic) / Hügelton Instruments**.
- **Se suman los aportes de este fork**: **Guillermo Lauria** (instalador publicado, CI y registro de build del
  emulador, capa de FX, banco QUANTUM) en README y en el ABOUT del firmware; **David Konsumer (@konsumer)**
  (primera instalación en una unidad de fábrica, chequeo de chip del loader, reanudación, releases por tag) en README.
- **`firmware/src/ui_being.c`**: el encabezado decía `Copyright (C) 2026 ShadowTune / GammaWave // Wraith Custom
  Firmware`, un titular que no aparece en ningún commit, en `LICENSES/` ni entre los colaboradores. El archivo se
  creó en este repositorio y los mundos de HOME se desarrollaron acá: el aviso pasa a Guillel51 / Quantum
  GammaWave, aclarando que "Wraith" es un nombre propio de este firmware (la ribbon y la barra).
- **Marcadores de conflicto, fuera del árbol y con guard.** `firmware/src/felucca.c` llegó a `main` con un marcador de conflicto sin resolver dentro de su
  `#ifndef FELUCCA_VERSION` y `ci_artifact/manifest.json` con el JSON partido: el build
  AC791N no define la versión con `-D`, así que sólo el target fallaba. Los dos resueltos, `ci_artifact/` (registro
  de build commiteado, de un commit que ya no existe) retirado, y `tests/run_tests.sh` ahora nombra cualquier
  marcador commiteado con archivo y línea antes de compilar nada.

## 2026-11-11 — v2.7.2: los créditos llegan al binario

- **El ABOUT publicado mostraba créditos viejos.** El paquete y el emulador de v2.7.1 se construyeron en
  `91029198`, antes del commit de créditos: el fuente ya decía Frederic Valente (@fvalente) para GW-1 y sumaba a
  Guillermo Lauria, pero el binario servido no. v2.7.2 se construye desde este árbol, así que `MENU > ABOUT >
  CREDITS` del paquete y del emulador publicados dicen lo mismo que `README.md`.
- **Mismo procedimiento, sin atajos:** paquete AC791N con la toolchain pineada, manifiesto de integridad
  (SHA-256 + tamaño, modelo de §9), emulador del mismo commit, espejo del sitio y `release/SHA256SUMS` regenerados
  juntos; `tools/fm1_sign.py verify` sobre el paquete antes de commitear.

## 2026-11-11 — v2.7.1: el conflicto del firmware, resuelto, y el paquete reconstruido

- **El arbol no compilaba.** El bump a v2.7.1 llego con marcadores de merge dentro de
  `firmware/src/felucca.c` (y de `ci_artifact/manifest.json`): `cc` fallaba con "version control conflict marker".
  Se resuelve conservando `#define FELUCCA_VERSION "v2.7.1"` y se elimina el `ci_artifact/` suelto, que era una
  copia de artefacto sin lectores.
- **Todo vuelve a nombrar una sola version.** README, `web/site/`, `web/index_pkg.html` y el banner pasan de
  v2.7.0 a v2.7.1; el arbol queda limpio (`dirty: []`) en el commit del build.
- **Paquete y emulador reconstruidos desde ese commit** (`9102919`): `release/felucca-v2.7.1.fwsc` 610 959 B,
  sha256 `8b1798b93892ffcc89956aeb3b6d5e1e8e14c1c48d3e9c472f08386fd3456de6`, imagen 541 484 B, identidad
  `FM-1_965`; WASM 1 104 650 B sha256 `e3388879…2d03`, Emscripten 6.0.11, mismo commit. Manifiesto
  `qg1-release-1` sin firma (`keyid: ""`), `fm1_sign.py verify` -> integrity only, y `sha256sum -c` limpio
  sobre las 22 entradas de `release/SHA256SUMS`.
- **Imagenes al dia:** banner y las 30 capturas de `assets/screenshots/` re-renderizadas del arbol v2.7.1
  (el splash del banner ya muestra la version real), y el espejo `site/` regenerado desde el paquete nuevo.

