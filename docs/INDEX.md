# Índice de documentación — por dónde empezar

223 documentos en `docs/`. Los de las fases viejas (1.x, 2.x, 3.x) son **históricos**: describen decisiones de su
momento y no siempre el estado de hoy. Esta página ordena lo que sigue vigente y, sobre todo, **qué clase de
evidencia** respalda cada afirmación (AGENTS §4: build, host, emulador y unidad no son lo mismo).

## Estado y presupuesto (lo que se mide hoy)

| Documento | Qué responde |
|---|---|
| `OPTIMIZATION_PHASE1_AUDIT.md` | Presupuesto de recursos medido en el build AC791N (imagen, XIP, `.bss`, pool, código RAM), las funciones con más trabajo estático, y la tabla de compactaciones probadas con su coste en bytes (fuente S, píldoras de tecla, kerning, `ui_shapes`, banderas del compilador). |
| `MEMORY_BUDGET.md` | El mapa de RAM: `.data+.bss`, pool, LOWBUF y sus reglas (96 KiB, holgura del pool ≥ 8 KiB). |
| `OPTIMIZATION_BASELINE.md` | El estado de rendimiento vigente: qué se midió, con qué instrumento y contra qué versión. |
| `VALIDATION_MATRIX.md` | La matriz de compuertas de instalación, requisito por requisito, con su evidencia y su estado. |
| `FASE9_CIERRE_2026-11-24.md` | El cierre 9/9: auditoría de seguridad, limpieza, pruebas medidas y lo que queda pendiente por hardware. |

## Arranque y pantalla

| Documento | Qué responde |
|---|---|
| `AUDIT_BOOT_v1.4.11.md` | El pop de encendido (causa: el fade se gastaba con la salida en mute; corrección: silencio hasta después de las etapas con interrupciones apagadas) y la revisión de la ruta de pantalla. Host x86 + build. |
| `BOOT_SPLASH_AND_POP.md` | Historia del splash y de los pops anteriores (1.4.2.1). |
| `PANEL_*` / `tests/panel_table_test.py` | La tabla de init del ST7789 y la prueba que la fija al driver publicado. |

## Audio y motores

`MOD_MATRIX_SLOTS.md`, `POLYPHONY_1.6.md`, `ENGINE_EXPANSION_PLAN.md`, `ARP_CLOCK.md`, `PATTERN_LENGTH.md`,
`PUNCH_IN_FX_AUDIT.md`, `JIANT_FEATURE_MATRIX.md`. Las cifras de CPU de estos documentos son **estáticas**
(compilador de destino) salvo que digan explícitamente "unidad".

## Instalación, flash y seguridad

`FM1_INSTALL_SAFETY.md`, `INSTALL_FAILURE_MATRIX.md`, `JEDEC_COMPATIBILITY.md`, `FLASH_MAP_AND_RECOVERY_FORENSICS.md`,
`SIGNING.md`, `M-FLASHER.md`, `BRICK_RESILIENCE_FM1.md`. Ninguno de estos reemplaza la evidencia física: la unidad
del dueño no se escribe nunca (AGENTS §3).

## Emulador y sitio

`EMULATOR_FIDELITY.md`, `EMULATOR_SYNC_AUDIT.md`, `FINAL_RELEASE_SYNC.md`. El registro del emulador
(`build/emu/manifest.json`) y el paquete de `release/` los verifica `tools/coherence_check.py` y
`tools/make_release.py --check`.

## Manuales

`MANUAL.md` / `MANUAL.es.md` (de uso), `INSTALL.md` / `INSTALL.es.md` (instalación), `BUILD_AC791N.md` (build).

## La regla que vale para todos

Un build no es un test y un test no es la unidad. Cuando un documento diga "funciona", buscá con qué se midió; si
no lo dice, vale como diseño, no como evidencia.
