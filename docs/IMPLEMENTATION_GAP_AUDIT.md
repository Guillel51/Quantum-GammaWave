# Implementation Gap Audit — Quantum GammaWave (QG-1)

Fuente de verdad de estado. Reemplaza a los `RECOVERY_*.md` de la raíz como referencia de estado
cuando contradigan el código. Cada afirmación cita archivo:línea o test. Clases de evidencia:
`host x86`, `render host`, `AC791N` (no ejecutado aquí), `WASM` (no ejecutado), `navegador`,
`físico` (sin unidad, no marcado nunca).

## 1. Estado inicial real (2026-11-11, HEAD `c5873d6`)

- Versión firmware: `FELUCCA_VERSION "v2.7.2"` (`firmware/src/felucca.c`).
- 8 motores activos: ANALOG(0), FM6(12), PHASE(2), LOFI(3), VOICE(5), WHEEL(7), NOISE(11), DRUM(10)
  (`engines.c:45-68`). Índices 1/4/6/8/9/13 reservados (DIGITAL → FM6 vía `fm4_convert.c`).
- 215 presets de fábrica contados en `eng_*.c` (AUDIT_UI §3).
- 5 modelos de reverb reales: ROOM, SPRING, SHIMR, RESO, CLOUD (`fx.c`, `N_RTYPE` en `params.c:42`).
  Los 7 nombres del encargo (HALL, PLATE, AMBIENT…) **no existen** en el DSP: no se inventan.
- Splash: sin texto BOOT ni porcentaje (verificado, `ui_render` mide 775 pasos).
- HOME: 5 cuerpos planetarios con 5 comportamientos de reacción (`ui_being.c:391`, `NPL 5`,
  `pl_react[5]`), visualizador sobre `scope_buf` real.
- Medidores FX: corregidos (`39a23a2`) con balística real, peak-hold, CLIP por bus.
- M STRT: implementado y conectado (`voice.c:717-724`, página `VOICE 2`, `params.c:104`).
- PHASE: pantalla propia añadida (commit `2dcb57a`), 17 paletas, 0 hallazgos de lint.
- MORPH: operativo con slewing por bloque; estados A/B persistentes **pendientes** (formato de
  preset nuevo + migración).
- Noveno motor: **no viable** por POOL (~1 KiB útil) (AUDIT §4 del pending register).

## 2. Matriz de cumplimiento (resumen)

| Función solicitada | Estado real | Evidencia | Trabajo necesario | Prueba de aceptación |
|---|---|---|---|---|---|
| HOME planetas | IMPLEMENTADO Y VERIFICADO | `ui_being.c:391`, render `home.png`, `planets_test.c` | capas/atmósferas: pendiente | tap por cuerpo en emulador |
| Visualizador | IMPLEMENTADO Y VERIFICADO | `graph_home_viz`, `home_viz.png` | degradación bajo carga: parcial | audio real en emulador |
| 8 editores por motor | IMPLEMENTADO Y VERIFICADO | `params.c:559-586`, `ui_test.c`, `edit_*.png` | PHASE dedicado: hecho (`2dcb57a`) | render + control físico |
| Mini-knobs HOME (CUT/RES/ATK/REL) | IMPLEMENTADO Y VERIFICADO | `core.h:405`, render `home.png` | sin cruces ATK/REL (verificado) | `ui_test.c` |
| Reverb por modelo | IMPLEMENTADO Y VERIFICADO (DSP) | `fx.c` comb/allpass por modelo | identidad visual diferenciada por modelo: mejorada esta pasada | render `reverb_*` |
| VU / medición | IMPLEMENTADO Y VERIFICADO | `fx.c:30`, `ui_being.c:1474`, `ui_tech.c:633` | medidores FX: corregidos (`39a23a2`) | `perform_test.c` L/R |
| EQ maestro con curva | IMPLEMENTADO Y VERIFICADO | `fx_master_eq_hz_db`, `master_eq` | — | `eq_response_test.c`, `eq_sweep_test.c` |
| Phaser / M/S / Punch-in | IMPLEMENTADO Y VERIFICADO | `fx.c`, `pfx.c`, `perform.c` | — | `perform_test.c` |
| Presets (215 fábrica) | IMPLEMENTADO, NO VERIFICADO (recuento) | tablas `eng_*.c` | familias nuevas: pendiente; A/B persistente: pendiente | carga + sonido |
| Splash | IMPLEMENTADO Y VERIFICADO | `ui_render` 775 pasos | animación +1s: pendiente si coste lo permite | render |
| MORPH A/B persistentes | NO IMPLEMENTADO | `core.h mo_cur/mo_prev` | formato nuevo + migración | `morph_audible_test` |
| Noveno motor | BLOQUEADO POR LIMITACIÓN TÉCNICA | POOL ~1 KiB útil | solo wavetable compacta candidata | presupuesto medido |
| Build AC791N | BLOQUEADO | sin toolchain/SDK | — | `./build.sh --release` |
| WASM/emulador | BLOQUEADO | sin `emcc` | — | `sh web/emu/build.sh` |
| Hardware FM-1 | BLOQUEADO | sin unidad | — | prueba física |

## 3. Cambios nuevos en esta pasada

- Reverb: identidad visual diferenciada por modelo en `tech_rev` (`ui_tech.c`): SPRING dibuja una
  bobina (zigzag alternante) sobre la cola en lugar de repetir las barras de ROOM; SHIMR mantiene
  sus chispas de octava; RESO y CLOUD ya eran distintos. Cada modelo es ahora visualmente
  distinguible en la página REVERB.
- Auditoría de gaps consolidada en este documento (requisito de la Fase 0).

## 4. Bloqueos honestos

- Sin `JIELI_TOOLCHAIN`/`AC79_SDK`: no hay imagen AC791N ni presupuesto medido del árbol actual.
- Sin `emcc`: no hay emulador nuevo ni verificación de navegador.
- Sin unidad física: ninguna afirmación de hardware.
- Consecuencia: las fases 16 (publicación) y la validación física no se declaran hechas.

## 5. Tests ejecutados (host x86)

- `tests/ui_render.c`: 179 pantallas × 17 paletas, 0 lint findings, 0 desalineaciones.
- `tests/ui_test.c`: all passed.
- `tests/fx_meter_test.c`: passed.
- `tests/planets_test.c`: all passed.
- `tools/coherence_check.py`: ok.
