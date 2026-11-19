# FASE 5 — Validación, limpieza y cierre (v1.5.4)

*19 de noviembre de 2026. Clase de toda medición de este documento: **host x86-64 sobre este árbol**, salvo
donde se diga explícitamente otra cosa. No hubo flasheo, borrado, escritura ni restauración en ningún FM-1.*

## 1. Suite completa: dos fallos, dos causas, cero pendientes

`NO_PACKAGE=1 ./tests/run_tests.sh` se corrió entero. Falló en dos puntos, y los dos eran reales:

| Fallo | Causa encontrada | Corrección |
|---|---|---|
| `evidence index` | `docs/EVIDENCE.md` (617 filas) había quedado atrás de `docs/VALIDATION_MATRIX.md` (675) | regenerado con `tools/evidence_index.py`; `--check` vuelve a estar en la suite |
| `UI pixels` (197 frames) | el baseline era anterior a v1.5.3: el cuerpo de HOME se ve detrás de ABOUT/FX/MONITOR y la suite dibuja con ANIM OFF, donde el pulso ya no late por diseño | regenerado con `tests/ui_pixels.py --update` y el motivo escrito en `CHANGELOG.md` |

Después: **3 056 frames idénticos**, `regress` **303 renders 0 changed**, lints de alineación
**3 783 017 mediciones / 0 fuera de 1 px**.

## 2. Cobertura que la suite ejerció (y la clase de cada cosa)

- **Polifonía y robo**: `poly_report_test` (12/12 con BRASS y PLUCK, 0 robos, 0 denegadas), `poly_stress_test`,
  `six_live` (seis pistas sonando), `voice_defer_test` (la cola diferida de la carrera teclado/core 1).
- **Motores y presets**: recorrido de todos los motores, conversión DIGITAL→FM6 sin división por cero bajo
  UBSan (#61), morphing (21 casos, 6 transiciones), presets de usuario (parser, ida y vuelta, versiones).
- **FX, buses, routing, mezcla**: escenas de FX, `fx_scenes`, límites de paquete, `fx_preset_flush`.
- **ARP/SEQ/LFO/SCL/macros**: cuantización de SEQ, LEDs de paso, escalas, escalado de teclas, `idle_glow`.
- **Entrada y hardware simulado**: pulsaciones rápidas, rebotes, glitches, encoders, identificador de flash
  (allowlist, dos lecturas, 1 MiB, protección → sólo RAM), auto-test del segundo núcleo.
- **Integridad**: manifiesto (12 checks), límites del paquete, fallos de paquete, procedencia, firma,
  preflight, pipeline del instalador.
- **UI**: barrido de render por paleta, alineación, píxeles, `ui_test` completo (incluye la prueba nueva de
  ANIM OFF de v1.5.3 y los dos lados del cambio de preset de v1.5.2).
- **ASan/UBSan** sobre los caminos que la suite compila con sanitizers.

## 3. Lo que sigue necesitando la unidad

Pop de encendido, clics y dropouts, latencia de teclado y MIDI, CPU del core 1 con audio real, y el resultado
acústico del hold/fade de arranque y del `perf_reset` acotado. Todo eso está corregido en código y fijado por
tests, y **no** está medido en hardware: decirlo de otra forma sería fabricar evidencia.

## 4. Limpieza

- El índice de evidencia vuelve a generarse solo: nada que quede invisible.
- `README.md`: la línea de versión decía `v1.3.9`; ahora dice `v1.5.4`. Los «diez motores» del README coinciden
  con `NENG_SHOWN` (16 ranuras, FM4 retirado y cuatro reservadas).
- No se borraron documentos de auditoría: son evidencia y material de recuperación (regla de la §2 de
  `AGENTS.md`). Lo obsoleto se marca en su propio texto, no se tira.

## 5. Presupuestos tras el cambio

Imagen **582 164 / 583 612 B** (1 448 B libres), `.data+.bss` **91 608 / 98 304 B** (6 696 B libres), paquete
**610 959 B**, identidad `FM-1_965`. Sin cambios de tamaño respecto de v1.5.3: esta fase no agregó código de
audio.
