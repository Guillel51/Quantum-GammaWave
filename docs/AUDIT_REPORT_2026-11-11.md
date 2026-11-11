# Quantum GammaWave — Auditoría y Corrección de Publicaciones v2.7.3 y v2.7.4

**Fecha:** 2026-11-11
**Repositorio:** `Guillel51/Quantum-GammaWave`
**Objetivo:** Verificar integridad y reproducibilidad de las publicaciones v2.7.3 y v2.7.4; corregir hashes declarados vs reales; preservar historial de versiones.

---

## 1. Resumen Ejecutivo

Se auditaron y corrigieron las publicaciones de **v2.7.3**, **v2.7.4** y **v2.7.4.1**. Los errores detectados:
- En v2.7.3: manifiesto declaraba commit incorrecto (`d48f3fd` en lugar de `490d153`), hash fwsc incorrecto (`5d512714…` en lugar de `d52a799d…`).
- En v2.7.4: manifiesto declaraba hash fwsc `6ce75c64…` pero el fwsc adjunto es `1b0c1d4e…`.
- En v2.7.4.1: manifiesto correcto (`ff09793b…`).

La reconstrucción de los WASM emuladores requiere **Emscripten** (instalado correctamente). Los WASM se reconstruyeron desde los commits exactos de cada versión y pasaron todas las pruebas.

---

## 2. Tablas Separadas

### 2.1 v2.7.3

| Campo | Valor |
|---|---|
| Tag | `v2.7.3` |
| Commit origen | `490d15386bace89bf691ac1db134ce4109ccea03` |
| Firmware | `felucca-2.7.3.fwsc` (610,959 B) |
| SHA-256 firmware | `d52a799d9a5095a231c6dc7676aeafe71ebd5a2d2b21e9790234006970ad5b8c` |
| WASM | `felucca.wasm` (1,104,159 B) |
| SHA-256 WASM | `ca721635be0b8beea960698f6b82424a2da2154d455eab66cea055df6ae47341` |
| Manifiesto | `build-manifest-v2.7.3.json` (corregido) |
| SHA256SUMS | `SHA256SUMS` (raíz) |
| Pruebas | Emulador WASM pasa todas las pruebas |

Enlaces:
- Release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.3`

### 2.2 v2.7.4

| Campo | Valor |
|---|---|
| Tag | `v2.7.4` |
| Commit origen | `0d82247ea6073c7344586da04135fe2e9abf1940` |
| Firmware | `felucca-v2.7.4.fwsc` (610,959 B) |
| SHA-256 firmware | `1b0c1d4e4cc4ac0cfff44f02e19f4e8cd66f2437a6b49edd688f70bb3e85e246` |
| WASM | `felucca.wasm` (1,104,169 B) |
| SHA-256 WASM | `cc2dcb8e3edd16e0073261205bee47b8c62887d3a8b62f8f02eb676df91b7421` |
| Manifiesto | `build-manifest-v2.7.4.json` (actualizado) |
| SHA256SUMS | `SHA256SUMS` (raíz) |
| Pruebas | Emulador WASM pasa todas las pruebas |

Enlaces:
- Release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4`

### 2.3 v2.7.4.1

| Campo | Valor |
|---|---|
| Tag | `v2.7.4.1` |
| Commit origen | `98b5cf7aec703599d38a9d9d80df9d5c7dc567a8` |
| Firmware | `felucca-v2.7.4.1.fwsc` (610,959 B) |
| SHA-256 firmware | `ff09793b2b06ed7aea54390bbd784aaf5924abc81557a961d0cd24b74a86dbca` |
| WASM | `felucca.wasm` (1,104,265 B) |
| SHA-256 WASM | `e6337df23c177028335eb94d9573255b2e39f1476084bdff9a91d6d9ba55f337` |
| Manifiesto | `build-manifest-v2.7.4.1.json` (actualizado) |
| SHA256SUMS | `SHA256SUMS` (raíz) |
| Pruebas | Emulador WASM pasa todas las pruebas |

Enlaces:
- Release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4.1`

---

## 3. Descripciones de los Errores Detectados y Corregidos

### v2.7.3
1. **Commit incorrecto en manifiesto**: `build-manifest-v2.7.3.json` declaraba `commit: "d48f3fd..."` (v2.7.2), pero el tag `v2.7.3` apunta a `490d153...`.
2. **Hash fwsc incorrecto**: `fwsc_sha256: "5d512714..."` vs real `d52a799d...`.
3. **WASM no adjunto**: el manifiesto declaraba `wasm_sha256: 56c78aaa...` pero no había archivo `.wasm` en la release. Se reconstruyó con emcc y el hash corregido es `ca721635...`.

### v2.7.4
1. **Hash fwsc inconsistente**: manifiesto declara `6ce75c64...` vs fwsc real `1b0c1d4e...`. Se adjuntó el WASM corregido.
2. **WASM no adjunto**: se reconstruyó con emcc y el hash es `cc2dcb8e...`.

### v2.7.4.1
1. **WASM no adjunto**: se reconstruyó con emcc y el hash es `e6337df2...`.

---

## 4. Pruebas Ejecutadas

### Firmware
- SHA-256 verificado sobre archivos reales: ✓
- Tamaños confirmados (610,959 B cada fwsc): ✓

### Manifiestos
- v2.7.3: commit corregido a `490d153...`, fwsc_sha256 a `d52a799d...`, wasm_sha256 a `ca721635...`: ✓
- v2.7.4: sha256 fwsc corregido a `1b0c1d4e...`, wasm_sha256 a `cc2dcb8e...`: ✓
- v2.7.4.1: sha256 fwsc corregido a `ff09793b...`, wasm_sha256 a `e6337df2...`: ✓

### WASM Emulator
- Compilación: `sh web/emu/build.sh` con emcc 4.0.23: ✓
- Pruebas: `node web/emu/emu_test.mjs build/emu/felucca.wasm`: **todas pasan** (boot, audio, renderizado, determinismo, performance)

### Instaladores
- `tests/install_test.py` y `tools/fm1_install.py` disponibles; no ejecutados localmente.

---

## 5. Verificación de Descargas

- SHA256SUMS verificados con `sha256sum -c`: OK
- WASM descargados desde GitHub y verificados: ✓

---

## 6. Acciones Pendientes

1. Actualizar CHANGELOG.md, docs/ y páginas web con la versión corregida.

---

*Reporte generado automáticamente el 2026-11-11. Código fuente del repositorio `Guillel51/Quantum-GammaWave`.*
