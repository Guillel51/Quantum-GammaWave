# RECONCILED BUILD, PROVENANCE, AND MEMORY AUDIT (QG-1)

## 1. Procedencia y Relación de Commits
- **HEAD en desarrollo / tests de seguridad (PR #73):** `7977c882a9a4cf9cd30e475cbfa26be5f93e9dd1` (contiene validaciones, fault injection, matrices multi-chip, suites de producción criptográfica y reconciliación documental).
- **Firmware Release Oficial compilado y firmado:** `release/felucca-v1.9.0.fwsc`
  - **Commit histórico de compilación target:** `7eb9636f58f442ccbfb8440dec0aa61e55203889`
  - **Tamaño exacto:** 610.806 bytes
  - **SHA-256 oficial publicado:** `80ad995caf9147f28fcbe3b135e3ab8da3e4509bdfed6d4766fee09c53eafed2`
  - **Firma Ed25519:** Verificada con clave anclada `qg1-release-1` (`70b6036fd33da3297a41da2141c5d135b3efce2d815d55911fc4f9b1d07fcd40`)
- **Recompilación limpia target realizada en este entorno (`build.sh`):**
  - **Compilador:** JieLi Clang 4.0.1 (pi32v2), archive SHA `a1545cddbc45...`, clang binary SHA `c45189ee624d...`
  - **SDK:** `fw-AC79_AIoT_SDK` commit `0617f4cc`
  - **Artefactos generados:** `build/felucca.bin` (532.120 B), `build/loader/ota.bin` (7.650 B), `build/felucca.fwsc` (610.806 B, SHA-256 `985f7441...`).

## 2. Conciliación Unificada de Memoria AC791N (JieLi WL82)
A lo largo de las campañas de optimización de Quantum GammaWave, los reportes registraron distintas mediciones intermedias. Esta tabla unifica y reconcilia definitivamente los valores:

| Etapa / Referencia | `.data + .bss` (RAM) | Presupuesto Máx | Holgura (Headroom) | App BIN (`felucca.bin`) | RAM Code (`.ram_text` + hot) | Pool Spare |
|---|---|---|---|---|---|---|
| **v1.8.7 (Histórico base)** | 96.452 B | 98.304 B | 1.852 B | 561.732 B | 22.528 B | 8.192 B |
| **v1.9.0 (Post `-Oz` release `7eb9636`)** | 96.088 B | 98.304 B | 2.216 B | 531.204 B | 19.444 B | 9.248 B |
| **Head Actual (`build.sh` limpio)** | 95.912 B | 98.304 B | **2.392 B** | 532.120 B | 19.284 B (de 24.576) | **9.248 B** |

### Explicación de Secciones:
1. **`.data + .bss` (95.912 B):** Estructuras estáticas de voces, síntesis, osciladores, envolventes y parámetros. El presupuesto asignado por el linker script (`firmware/app.ld`) es de 96 KiB (98.304 B), dejando **2.392 bytes de holgura estática**.
2. **Pool de Audio (311.240 B):** Buffers de retardo, reverb y procesamiento estático en SRAM secundaria. Dispone de **9.248 B libres (spare)**, superando el requisito mínimo de ≥ 8 KiB.
3. **RAM Code (19.284 B):** Código ejecutado en memoria rápida sin latencia de flash (ISRs, rutinas críticas de DSP). El límite es de 24.576 B (holgura de 5.292 B).
4. **App Flash (`felucca.bin`, 532.120 B):** Código XIP (372.536 B) y constantes (139.552 B), perfectamente contenido dentro de los 768 KiB disponibles en la partición NOR.
