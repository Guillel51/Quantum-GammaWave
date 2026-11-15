# Estado de la release v4.1.0 — comprobado contra la URL pública

Todo lo de abajo se verificó descargando los artefactos publicados, no leyendo el repositorio.

## Lo que existe y coincide

| Cosa | Valor comprobado |
|---|---|
| Tag | `v4.1.0` (anotado) → commit `671ac35b583becaccd8b26b0ad4c72d04909b69e` |
| Release | *Quantum GammaWave v4.1.0* (página `releases/tag/v4.1.0`, HTTP 200) |
| Asset del paquete | `felucca-v4.1.0.fwsc`, **610 959 B**, **sha256 `dfc2a973ab8d718d7c757827aa0d6fc5c3f313f33f3d99d8cacc7f5bfd2cec3a`** |
| Asset del registro | `felucca-v4.1.0.fwsc.manifest.json` |
| Asset del build | `build-manifest-v4.1.0.json` |
| Asset de sumas | `SHA256SUMS` |
| Sitio | `site_check.py` → *all checks passed*: el paquete del sitio es ese mismo hash, el wasm del emulador es el del registro (`c92a734`, 1 233 268 B, `ae90ee7b…`), el instalador ofrece 4.1.0 |
| Coherencia local | `coherence_check.py` → *ok — v4.1.0, package and manifest agree, the site serves that package, the emulator record matches the build* |

El archivo descargado desde la release **hashea igual que el construido acá** (`dfc2a973…`): la release no
lleva otro binario, lleva el mismo.

## Lo que no se puede afirmar todavía

* **Attestation:** verificarla pide `gh attestation verify` con credenciales; en este entorno no hay ninguna
  (`gh auth status` → *not logged in*). El badge de procedencia del Hub queda sin comprobar desde acá.
* **`release/SHA256SUMS`:** estaba desactualizado (llegaba hasta v4.0.0). Se agregaron las tres entradas de
  v4.1.0 (paquete, manifest del registro y `build-manifest-v4.1.0.json`), con los hashes calculados sobre los
  archivos locales. Verificado: `sha256sum -c release/SHA256SUMS` sin fallos y `tools/coherence_check.py` → ok.
  El instalador valida contra el manifiesto del paquete, no contra este archivo; la instalación no depende de él.
  Los hashes locales coinciden con el asset publicado, según la comprobación de arriba.
* **Unidad física:** nada de esto se probó en una FM-1.
