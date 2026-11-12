# QG v2.8 — matriz de cumplimiento y estado real

**Fecha:** 12 de noviembre de 2026 · **HEAD:** `f89d723` · **Versión publicada:** v2.7.6
**Este documento no declara v2.8 publicada.** Lo que sigue es el estado medido, casilla por casilla, con la
evidencia y la clase de cada una. La sección 3 dice exactamente qué falta para que v2.8 sea honesta.

Clases de evidencia: `[HOST]` (esta máquina x86) · `[SIM]` (modelo de flash / loader simulado) ·
`[TARGET]` (toolchain pi32v2 + AC79 SDK, medido sobre el `.dis`) · `[JEDEC]` (allowlist documentada) ·
`[PUBLIC]` (API de GitHub) · `[FÍSICO]` (una unidad real: **ninguna**).

---

## 1. Matriz de cumplimiento de las ocho fases

| # | Fase | Estado | Evidencia | Qué falta / por qué |
|---|---|---|---|---|
| 1 | JIANT vs QG: motores, EDIT, SCL, HOME, FX, navegación | **PARCIAL** | `docs/QG_ENGINE_AUDIT.md`, `docs/QG_FX_AUDIT.md`, `docs/JIANT_FEATURE_MATRIX.md`, `docs/JIANT_QG_COMPARISON.md` | comparación escrita y motores auditados; la integración de lo que JIANT tiene y QG no (STRING/ADD/MATH) está en la fila 4 |
| 2 | SCL: escalas, tonalidades, notas, grados, acordes, inversiones, voicings, progresiones | **IMPLEMENTADO Y VERIFICADO** (6 pantallas) | `tests/chord_scale_test.c`: 8 448/8 448 invariantes, 360 acordes idénticos al recorrido viejo `[HOST]`; `ui_render` pinta `scl_*` en 17 paletas | inversiones/voicings por encima de lo que exponen las 6 pantallas actuales: no auditado pantalla por pantalla |
| 3 | EDIT: páginas funcionales de motores, osciladores, filtros, envolventes, modulación, routing | **IMPLEMENTADO Y VERIFICADO** | 187 pantallas renderizadas, 0 hallazgos de lint, 0 de 2 777 796 medidas de alineación `[HOST]`; `tests/ui_test.c` | ninguna página nueva; el contenido de cada motor está en `docs/QG_ENGINE_AUDIT.md` |
| 4 | HOME con vistas OVERVIEW / PERFORMANCE / SOUND ENGINE / MIXER / MODULATION / SYSTEM / VISUALIZER; motores STRING, ADD, MATH | **PARCIAL** | HOME→VISUALIZER→OVERVIEW→SYSTEM implementado (`firmware/src/ui_homeviews.c`, `test_home_views` verde) `[HOST]` | PERFORMANCE, SOUND ENGINE, MIXER y MODULATION **no existen**; STRING, ADD y MATH **no están implementados** (`docs/QG_NEW_ENGINES.md` los especifica, no los implementa). EXT no evaluado a fondo. ANIMAL no se incorpora (decisión) |
| 5 | FX y presets: Phaser, M-WIDE, EQ, filtros, reverb, delay; VU reales; Punch-in; macros; latch; mutes; pantalla de buses en la 2ª página de FX | **PARCIAL** | M-WIDE con compensación de potencia medida (−0,0 dB a 127, correlación −0,594, mono a −3,9 dB) `[HOST]`; 202 presets auditados (0 silenciosos, 0 fuera de rango, 0 duplicados); VU sobre picos reales (`fx_vu`); latch y mutes existen | **la pantalla BUSES no existe**: no hay página, ni pestañas TRACK/BUSES, ni tarjetas DELAY/REVERB/CHORUS/SLICER/PUNCH/MASTER. Punch-in FX y macros: no implementados |
| 6 | Seguridad: PR #86–#89, ambos JEDEC, validaciones, integridad, autenticidad, protección de flash | **IMPLEMENTADO Y VERIFICADO** en software | `docs/QG_FINAL_SECURITY_AUDIT.md`; `ALL HOST TESTS PASSED`; `flash evidence: 20 checks`, `flash read: 16 checks`; `test_verify_only` 153 checks; ASan/UBSan + 2 fuzzers; attestation consultada por API `[PUBLIC]` | nada `[FÍSICO]`: sin lectura real de JEDEC en una unidad, sin backup verificado, sin recuperación demostrada. **Instalación: NO-GO** |
| 7 | Optimización: CPU, núcleos, RAM, flash, DSP, buffers, latencia, gráficos, concurrencia | **IMPLEMENTADO Y VERIFICADO** en lo medible | `.data+.bss` 87 696/98 304 B, pool spare 15 392 B, imagen 551 720 B `[TARGET]`; 39 presupuestos de `target_budget.py` dentro de límite `[TARGET]`; `fx_buses` a una pasada: 127,2 → 126,0 ns/muestra `[HOST]`; build reproducible byte a byte | el coste es modelado (`.dis`), no tiempo medido en silicio; la división del limitador y la reverb en reposo quedan como candidatos descartados con razón escrita |
| 8 | UI: rediseño de todas las pantallas, animaciones, partículas, reacciones, espacios vacíos | **PARCIAL** | EXTRA muestra ahora `CORES 1/2`, `CPU n%` y `CPU MODE: NEXT POWER-ON`; DLY dibuja la envolvente real de sus ecos; `QG_DUMP_NAME` permite revisar una pantalla por nombre | **falta el grueso**: indicador de páginas homogéneo en todas las pantallas, BUSES, y rotulación/escala de chorus, dly 2, dist, comp, slicer |

## 2. Lo implementado y verificado en esta sesión (todas las fases)

| Entrega | Evidencia |
|---|---|
| `fx_buses` a una sola pasada sobre los tres buses | A/B intercalado 6 rondas: mediana 127,2 → 126,0 ns/muestra `[HOST]`; 220 dorados bit-idénticos |
| `release/SHA256SUMS` dañado (marcadores de commit) → regenerado | 61 entradas, `sha256sum -c` limpio; el defecto se cometió en `91df789` y nadie lo vio |
| `coherence_check.py` crasheaba en vez de reportar | valida forma de línea y da número de línea; `coherence: ok` |
| v2.7.5 y v2.7.6 con paquete real | AC791N + emcc 6.0.11 desde el mismo commit; releases publicadas con attestation (`sha256:ccc67307…` para v2.7.6) `[PUBLIC]` |
| EXTRA explica CPU MODE | `RUNNING · CORES 1/2`, `CPU n%`, `CPU MODE: NEXT POWER-ON`; el ajuste sí llega a `main.c` antes de arrancar el núcleo 1 |
| DLY con envolvente real | `fx_dly_rep` unido y relleno; nada inventado |
| `QG_DUMP_NAME` | revisión visual por nombre de pantalla |

## 3. Qué falta para publicar v2.8 (y por qué no se publicó)

1. **Pantalla BUSES** (pestañas TRACK/BUSES, tarjetas DELAY/REVERB/CHORUS/SLICER/PUNCH/MASTER, animaciones
   ligadas a señal real). Es una página nueva del sistema de páginas (tabla `PAGES`, grafo propio, mapeo de los
   cuatro knobs) más su animación; sin ella el requisito visual específico de la fase no está cumplido.
2. **Indicador de páginas homogéneo** en HOME, SCL, FX, presets, menús y subpantallas. Hoy el pie muestra
   "DLY 1/3" y "HOME", pero no hay un indicador común con total de secciones por familia.
3. **Motores STRING, ADD y MATH**: especificados en `docs/QG_NEW_ENGINES.md`; requieren decisión de memoria
   (piso de 8 KiB del pool) y su propio presupuesto.
4. **Vistas de HOME** PERFORMANCE, SOUND ENGINE, MIXER y MODULATION.
5. **Auditoría de las 187 pantallas** una por una, con rotulación y escala donde hoy hay gráfico sin ejes.

Publicar v2.8 sin (1) y (2) sería publicar una versión que no cumple lo que su propio número promete, y la
instrucción de esta fase es justamente no hacer eso.

## 4. Riesgos residuales

| Riesgo | Severidad | Estado |
|---|---|---|
| Instalación física sin evidencia `[FÍSICO]` | **crítico** | el instalador sigue en Verify Only; **no instalar** |
| Build oficial producido por un workflow de un tercero (pinneado a `85a003ab`) | alto | mitigado, no eliminado |
| Dos configuraciones de flash tratadas como equivalentes por allowlist | medio | requiere lectura por unidad |
| Coste de CPU modelado, no medido en silencio | medio | `target_budget.py` sobre `.dis`; falta medición en placa |

## 5. Commit, release y hashes

| Componente | Estado |
|---|---|
| `main` | `f89d723` |
| Firmware | v2.7.6, paquete 610 959 B, identidad `FM-1_965`, sha256 `ccc67307c260…` |
| Release | v2.7.6 publicada `[PUBLIC]`, activos `felucca-v2.7.6.fwsc` + `SHA256SUMS`, digest coincidente |
| Emulador | wasm v2.7.6 desde `92f17c3` |
| Sitio / instalador / Web Studio | regenerados para v2.7.6; `site_check` y `test_site` en verde; `coherence: ok` |
| v2.8 | **no publicada**: ver §3 |
