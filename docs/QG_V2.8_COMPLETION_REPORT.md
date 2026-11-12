# QG v2.8.3 — qué se cerró de verdad

**Commit:** `10139ad3a6d8` · **Versión:** v2.8.3 · **Fecha:** 12 nov 2026

Estados: **IMPLEMENTADO Y VERIFICADO** (IV) · **IMPLEMENTADO CON VERIFICACIÓN PARCIAL** (IVP) ·
**PARCIAL** (P) · **PENDIENTE POR BLOQUEO TÉCNICO** (PB) · **DESCARTADO** (D).
Clases: `[HOST]` `[SIM]` `[TARGET]` `[PUBLIC]` `[FÍSICO]` — no hay nada `[FÍSICO]` en todo el proyecto.

## 1. Pendientes y resolución

| # | Pendiente de fases anteriores | Estado | Prueba |
|---|---|---|---|
| 1 | Vista HOME MIXER plana y básica | **IV** | 191 pantallas × 17 paletas, 0 lint; captura revisada |
| 2 | HOME PERFORMANCE inexistente | **IV** | ciclo de 8 pantallas en `ui_test`; sweep `home_perf` |
| 3 | HOME SOUND ENGINE inexistente | **IV** | idem; parámetros leídos de los descriptores del motor |
| 4 | HOME MODULATION inexistente | **IV** | idem; filas = `P_M*SRC/DST/AMT` y `P_LD_*` reales |
| 5 | Crash del renderer con `label` NULL | **IV** | ASan localizó el `fprintf`; guarda añadida |
| 6 | Textos solapados en las pantallas nuevas | **IV** | 476 hallazgos de lint en el primer borrador → 0 |
| 7 | `signed release` rojo desde v2.7.6 | **IV** | run 43198715449 `success` |
| 8 | Indicador universal de páginas | **P** | el pie ya muestra `PÁGINA k/n` en toda pantalla paginable; falta el indicador segmentado superior |
| 9 | Motores STRING / ADD / MATH | **PB** | no hay ruta que quepa en el pool sin desplazar voces; especificados en `docs/QG_NEW_ENGINES.md` |
| 10 | Skins conceptuales (Nebula, Plasma…) | **PB** | requieren un sistema de capas que hoy no existe; 17 paletas son color, no diseño |
| 11 | Inserts por pista, parameter locks, escenas | **PB** | 6× el coste DSP de cada inserto; locks exigen RAM de proyecto nueva |

## 2. Archivos tocados

`firmware/src/ui_homeviews.c` (MIXER redibujado + PERFORMANCE + SOUND ENGINE + MODULATION), `firmware/src/ui.c`
(enum del ciclo, 8 pantallas), `firmware/src/felucca.c` (versión), `tests/ui_render.c` (3 pantallas nuevas),
`tests/ui_test.c` (ciclo y lectura-sólo), `CHANGELOG.md`, `release/*`, `site/*`.

## 3. Optimización

| Medida | Antes | Después |
|---|---|---|
| Lint del barrido | 476 hallazgos (borrador) | **0** |
| Mediciones de alineación | 2 778 340 | 2 779 972, **0** fuera de sitio |
| Pantallas renderizadas | 188 | 191 |
| Paquete | 610 959 B | 610 959 B (sin cambio) |
| WASM del emulador | 1 143 364 B | 1 164 338 B (+20 974 B por las tres pantallas) |
| DSP dorados | 220 | 220, **0** cambios |

## 4. Seguridad y compatibilidad

ASan/UBSan y los dos fuzzers dentro de `ALL HOST TESTS PASSED` (0 FAIL) sobre este commit. JEDEC `0x856014` /
`0xB36014` intactos, Verify Only sin frames de escritura, `_Static_assert` de la ventana de flash en pie.
SHA-256 sigue presentándose como integridad, **no** como autenticación.

## 5. Estado de la publicación

Firmware v2.8.3 `df0ceef0…`, emulador v2.8.3 desde `10139ad`, sitio e instalador sirviendo v2.8.3 con v2.8.2,
v2.8.1 y v2.7.6 instalables, `SHA256SUMS` con 72 entradas, `coherence: ok` sobre 943 archivos.

## 6. Lo que no se hizo, y por qué

Los motores nuevos, las skins por capas, los inserts por pista y los parameter locks no caben en el presupuesto
de RAM/flash/DSP del FM-1 tal como está medido hoy (`tests/target_budget.py`). No se añadieron controles sin
procesamiento detrás. **Instalación física: NO-GO**, sin cambios.
