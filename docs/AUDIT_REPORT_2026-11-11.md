# Quantum GammaWave — Cierre Definitivo de Publicación v2.7.4 / v2.7.4.1

**Fecha:** 2026-11-11
**Repositorio:** `Guillel51/Quantum-GammaWave`

---

## 1. Estado Final

### v2.7.4

| Campo | Valor |
|---|---|
| Tag | `v2.7.4` |
| Commit origen | `0d82247ea6073c7344586da04135fe2e9abf1940` |
| Firmware | `felucca-v2.7.4.fwsc` (610,959 B) |
| SHA-256 firmware | `1b0c1d4e4cc4ac0cfff44f02e19f4e8cd66f2437a6b49edd688f70bb3e85e246` |
| WASM | `felucca.wasm` (1,104,169 B) |
| SHA-256 WASM | `cc2dcb8e3edd16e0073261205bee47b8c62887d3a8b62f8f02eb676df91b7421` |
| Manifiesto corregido | `release/build-manifest-v2.7.4.json` (commit `0d82247e...`, fwsc `1b0c1d4e...`, wasm `cc2dcb8e...`) |
| SHA256SUMS corregidos | `SHA256SUMS` (raíz) + `v2.7.4_SHA256SUMS` (release) |
| Pruebas | Emulador WASM v2.7.4: **todas las pruebas pasan** |

Enlaces:
- Release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4`
- Orbita release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4`

### v2.7.4.1

| Campo | Valor |
|---|---|
| Tag | `v2.7.4.1` |
| Commit origen | `98b5cf7aec703599d38a9d9d80df9d5c7dc567a8` |
| Firmware | `felucca-v2.7.4.1.fwsc` (610,959 B) |
| SHA-256 firmware | `ff09793b2b06ed7aea54390bbd784aaf5924abc81557a961d0cd24b74a86dbca` |
| WASM | `felucca.wasm` (1,104,265 B) |
| SHA-256 WASM | `e6337df23c177028335eb94d9573255b2e39f1476084bdff9a91d6d9ba55f337` |
| Manifiesto corregido | `release/build-manifest-v2.7.4.1.json` (commit `98b5cf7a...`, wasm `e6337df2...`) |
| SHA256SUMS corregidos | `SHA256SUMS` (raíz) + `v2.7.4.1_SHA256SUMS` (release) |
| Pruebas | Emulador WASM v2.7.4.1: **todas las pruebas pasan** |

Enlaces:
- Release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4.1`
- Orbita release: `https://github.com/Guillel51/Quantum-GammaWave/releases/tag/v2.7.4.1`

---

## 2. Descripciones de los Errores Corregidos

### v2.7.4
1. **Commit incorrecto en manifiesto**: `build-manifest-v2.7.4.json` declaraba `commit: "587fc989..."` (commit del build), no el commit del tag `0d82247e...`.
2. **Hash fwsc incorrecto**: `felucca.fwsc.sha256: "6ce75c64..."` (fwsc del build 587fc989) vs fwsc real de la release `1b0c1d4e...`.
3. **WASM incorrecto en manifiesto**: `wasm_sha256: "98eb82fe..."` y `wasm_bytes: 1108355` vs WASM real `cc2dcb8e...` y 1,104,169 B.
4. **Se añadió el WASM** como `felucca.wasm` a la release.

### v2.7.4.1
1. **Commit incorrecto en manifiesto**: `build-manifest-v2.7.4.1.json` declaraba `commit: "c1fbe145..."` (commit del commit anterior) y no reflejaba el commit del tag `98b5cf7a...`.
2. **WASM `null` en manifiesto**: el manifiesto no declaraba ningún WASM, pero se subió `felucca.wasm` (1,104,265 B).
3. **Se añadió el WASM** como `felucca.wasm` a la release.

---

## 3. Pruebas Ejecutadas

### WASM Emulator
- Compilación: `sh web/emu/build.sh` con emcc 4.0.23: ✓
- Pruebas: `node web/emu/emu_test.mjs build/emu/felucca.wasm`: **todas pasan** (boot, audio, renderizado, determinismo, performance)

### Firmware
- SHA-256 verificado sobre archivos reales: ✓
- Tamaños confirmados (610,959 B cada fwsc): ✓

### SHA256SUMS
- `sha256sum -c SHA256SUMS`: OK
- `sha256sum -c v2.7.4_SHA256SUMS`: OK
- `sha256sum -c v2.7.4.1_SHA256SUMS`: OK

---

## 4. Verificación de Descargas

- Release v2.7.4 assets verificados: `felucca-v2.7.4.fwsc` (610,959 B), `felucca.wasm` (1,104,169 B), `SHA256SUMS`, `v2.7.4_SHA256SUMS`.
- Release v2.7.4.1 assets verificados: `felucca-v2.7.4.1.fwsc` (610,959 B), `felucca.wasm` (1,104,265 B), `SHA256SUMS`, `v2.7.4.1_SHA256SUMS`.
- `sha256sum -c` devolvió OK para todos los archivos verificados.

---

## 5. Pendientes Realmente Estas

1. **Emscripten 4.0.23** usado en lugar de la versión 6.0.11 (documentada). El proceso de compilación es compatible, pero la versión difiere del estándar del proyecto.
2. **Description de las releases** (v2.7.4 y v2.7.4.1) se mantuvieron intocadas (se añadieron solo archivos assets).
3. **Caché de GitHub Pages**: se verificó que los assets publicados son descargables; la verificación del emulador online en la web implica verificar el JSON de versiones y el intento de carga del WASM.

---

## 6. Commit y Push

- Commit `92d5428` "Correct audit of v2.7.3, v2.7.4 and v2.7.4.1 releases" (creado).
- Push no solicitado por el usuario. Si se requiere, el commit está listo para ser subido.

---

*Reporte generado automáticamente el 2026-11-11. Código fuente del repositorio `Guillel51/Quantum-GammaWave`.*
