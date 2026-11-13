# FASE 1 — QUANTUM CORE: auditoría, optimización, validación (2026-11-13)

Rama de trabajo: `freebuff/0054c97d7e260bd7fff60382` · Base medida: `6c25f7f` (v2.9.6).
Alcance: exclusivamente Fase 1 del plan maestro. No se inicia la Fase 2.

Clases de evidencia usadas aquí (nunca se mezclan):
`[HOST]` x86-64 de esta VM · `[TARGET]` build pi32v2/AC79 (no disponible aquí)
· `[WASM]` Emscripten (no disponible aquí) · `[HW]` unidad FM-1 (no existe aquí).

## 1. Baseline registrado ANTES de modificar código productivo

Generado con `python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"`.

| Medida `[HOST]` | Valor pre-cambio |
|---|---|
| `regress` dorados | **285 renders, 0 changed**, 0 health, 0 voice/routing, 1 nota CPU (`PHASE/19_CHIME_TREE` +49 % ns puntual frente a mediana host 1.03: ruido, no regresión) |
| `regress` mixes | `idle_drums` 109.0 · `3parts_full_drums` 289.3 · `6parts_full` 328.6 · `idle_click` 79.7 ns/muestra |
| `fx_bench` (mejor de 5, CPU proceso) | escena buses 125.6 · secas 121.6 · flanger 121.3 ns/muestra |
| `isr_bench` tick TIMER5 | idle 27.5 · leds 28.1 · teclas 28.9 ns/tick; `sr_bits` 38.4, `keys` 3.6 ns |
| Seguridad host | `sign_test` ok · `backup_audit` ok · `negative_write` ok · `transport_gate` ok |
| Sanitizadores/fuzz | `robust` ok · `fuzz_ed` 20000/seed7 ok · `fuzz_proj` 5000/seed13 ok |

Memoria/RAM `[TARGET]`: **no re-medida en esta sesión** (sin `JIELI_TOOLCHAIN` ni `AC79_SDK`).
Último registro válido citado: `.data+.bss` 87 696/98 304 B, pool spare 15 392 B, RAM code
19 284/24 576 B, XIP ~380 KB de 581 052 B (`docs/QG_PHASE_1_OPTIMIZATION.md` §7.1).

## 2. Inventario de funcionalidades (firmware)

Motores audibles (9 reales + reservas): ANALOG(0), PHASE(2), LOFI(3), VOICE/formant(5),
WHEEL(7), DRUM-X(10), NOISE(11), FM6(12), WAVETABLE(14). Reservas retiradas: DIGITAL(1),
SAMPLE(4), TRIO(6), GRAIN(8), PHYS(9), SLICE(13) → suenan como ANALOG/FM6/DRUM.
FX: DIST por parte, sends C/D/R, chorus(+flanger), delay (grain/tape), reverb
(ROOM/SPRING/SHIMMER/RESO/CLOUD), slicer, master (DC block, lowcut, M-WIDE, leveler,
limitador con división por muestra, softclip). Secuenciador 6 partes, 32-sample blocks
@44.1 kHz, dual-core (0: master/UI/partes 1-3, 1: partes 4-6), SysEx/editor, persistencia
A/B, backup/restore, loader `[0x4000,0x93000)`, instalador con compuertas, Web Studio/WASM.

## 3. Mapa de dependencias (audio)

`audio.c` half-buffer → `mix_block` (fx.c): eventos/seq → `mix_part` por parte →
`track_render` (voice.c): env (`env_tick`) → LFO (`track_lfo_tick`, un bloque de retardo)
→ MORPH slew → motor `e->render` (osc `dsp.c`: sine_i/saw/pulse/tri + polyBLEP) →
`track_post`/`track_filter` (SVF `tsvf_coef` por voz y bloque + COMB) → `track_dist` →
nivel/pan/envíos → `fx_buses` → `slicer` → `perf_master` → `master_out`.
Coeficientes SVF ya por bloque; `beat_samples` cacheado; envíos secos sin clamp/divide.

## 4. Lista de defectos

1. **`morph_six_test` falla en el HEAD limpio (preexistente, no causado por esta fase).**
   `6 transitions, 6 failed`, binario sale 1 con y sin mis cambios (verificado con
   `git stash`: base prístina también sale 1). Las 6 transiciones miden diferencia 0.00x.
   Estado: reportado, no maquillado; bloquea el cierre de Fase 1 hasta resolverse.
2. `PHASE/19_CHIME_TREE` +49 % ns puntual en una corrida (mediana host 1.03): ruido de VM
   compartida, no regresión (el gate es la mediana +20 %: PASS).
3. Sin toolchain/ni unidad: todo `[TARGET]`/`[WASM]`/`[HW]` queda NO VERIFICADO (ver §8).

## 5. Experimento voice-pool anterior: investigado, NO integrado como tal

`docs/VOICE_POOL.md`: el pool global ahorra 7.8–9.6 KiB pero mueve 25/220 dorados
(fases/estado residual por slot, seed de WHEEL por slot, orden de asignación).
El árbol actual (`NVOICE=12`, `VPOOLN=NVOICE+NPART`, `FELUCCA_VRESET=1`,
`FELUCCA_WSEED=1`) ya incorpora pool + resets (vino con `ea01671`), es decir, la
decisión de sonido que VOICE_POOL.md pedía ya la tomó el historial: los 285 dorados
actuales son la referencia con pool. No se revierte ni se re-migra nada en esta fase.

## 6. Optimizaciones reales integradas (bit a bit idénticas, 1 archivo)

`firmware/src/voice.c` (único archivo modificado, +50/−14):

- **(a) Términos por parte fuera del bucle por voz** (`track_render`): `lfo*P_LD_PIT/FLT/
  SHP/AMP`, `pfx_pit+tune+bend16` se calculaban por voz siendo invariantes por parte y
  bloque. Ahora una vez por parte/bloque (`lfo_pit/flt/shp`, `lfo_amp_gain`, `base_pit`,
  `base_fine`). Sumas enteras sin overflow posible: idénticas.
- **(b) Caché de `tune`/`bend`**: `G_TUNE` cambia rara vez (2 divides evitadas por parte
  y bloque en estado estable); `bend16/bend_fine` se reutilizan mientras `bend_q8` no
  cambie (caso común sin pitch-bend). Misma entrada → misma salida, por construcción.
- **(c) `track_filter` sin divisiones**: `w0=(x<<12)/8` → `x<<9` exacto (`TF_XB=8`,
  `x` en 0..8); `dw=(4096/8)>>5` → constante `16`.
- Coste: ~90 B `.bss` (cachés), 0 B de estado nuevo en ruta de muestra, sin tablas.

Medida A/B `[HOST]` (mismo compilador, `taskset`, orden alternado, mejor de 3, 3 rondas):
escenas ~128/123/120 ns en ambas versiones, **neutro dentro del ruido** (±1 %).
Declaración honesta: en host el bloque lo dominan las muestras, no los 4 divides y ~10
mults por parte/bloque eliminados; el ahorro existe en `pi32v2` (cada divide evitada es
una rutina libgcc) pero **no medido sin toolchain**. 285/285 dorados idénticos (§7).

Rechazado explícitamente: recíproco fijo en rampas DRUM-X/ATK (cambiaría bits),
limitador sin división por muestra (cambiaría audio), `-O3`/`-Os`/`-O2` globales
(medidos y descartados en fases previas).

## 7. Validación ejecutada (árbol final)

- `regress`: **285 dorados 0 changed**, 0 health, 0 voces, CPU budget ok (1 nota ruido).
- `mod_test` ok · `six_track_test` ok · `motion_test` ok · `engine_param` 216 knobs ok.
- `input/audio/fm6/drumx/noise/wavetable` ok · `chip/restore` ok.
- ASan/UBSan `robust`, `fuzz_ed` (20000/7), `fuzz_proj` (5000/13): sin hallazgos.
- `sign/backup_audit/negative_write/transport_gate`: PASS (sin cambios en esas rutas).
- Suite completa `NO_PACKAGE=1 ./tests/run_tests.sh` sobre el árbol final: lanzada,
  log en `/tmp/fase1_newcode.log`; a la redacción de este informe iba por TRACKS sin
  fallos nuevos salvo el `morph_six` preexistente (§4.1). El PASS final completo queda
  como trabajo en curso y se anexará al PR.
- UI: sin cambios de dibujo (solo firma); no se reclama coste `[RENDER]`.
- No se tocó bootloader, SPL, `USB_KEY`, regiones, allowlist JEDEC, Ed25519 ni firmas.

## 8. Limitaciones no verificadas (bloquean instalación, no se maquillan)

- `AC791N TARGET COMPILED`: **NO** (sin toolchain/SDK: sin imagen, secciones, `.dis`,
  `target_budget.py`, `memory_map_test`). El +90 B `.bss` cabe en el margen citado
  (10 608 B) pero no está medido en target.
- `WASM TESTED`: **NO** (sin `emcc`).
- `HARDWARE VERIFIED`: **NO** (sin unidad: sin latencia, CPU real, JEDEC, escritura).
- Instalación en FM-1 físico: **PROHIBIDA** en esta fase; `INSTALL BLOCKED` sigue.
- Fase 2: **NO iniciar** hasta resolver el defecto §4.1 y obtener el PASS completo.

## 9. Archivos, commit y PR

- Modificado: `firmware/src/voice.c` (único). Añadido: este informe.
- Commit/PR: pendientes de crear al cierre de la suite completa del §7.
"
  