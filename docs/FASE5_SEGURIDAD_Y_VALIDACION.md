# Fase 5 — Seguridad y validación final (2026-11-19)

Clase de cada cifra: `[HOST]` x86-64 con `cc`, `[WASM]` Node contra el wasm de Emscripten 6.0.11,
`[AC791N]` el binario con el toolchain `pi32v2` y el SDK `AC79NN_SDK_V1.2.1` fijados. **No hay ninguna
medición de unidad**: no hubo FM-1 en esta sesión y no se envió ninguna escritura a un dispositivo.

## 1. La batería de seguridad del repositorio, ejecutada

| Suite | Qué cubre | Resultado |
|---|---|---|
| `tests/chip_test.py` | allowlist JEDEC (P25Q80H `0x856014`, UC25HQ80 `0xB36014`), rechazo de chips desconocidos | **PASS**, 37 comprobaciones |
| `tests/restore_test.py` | restore crudo, límites de la ventana `[0x4000,0x93000)`, cabeza intacta | **PASS**, 35 |
| `tests/sign_test.py` | firma Ed25519: válida, alterada, clave desconocida, downgrade | **PASS**, 37 |
| `tests/preflight_test.py` | preflight del instalador, cabecera exacta del loader | **PASS**, 22 |
| `tests/key_custody_test.py` | custodia: sólo la clave pública en el árbol, sin privada | **PASS**, 56 |
| `tests/gate_test.py` | compuerta de instalación, estados y evidencias | **PASS**, 13 |
| `tests/flash_evidence_test.py` | evidencia de flash por chip | **PASS**, 16 |
| `tests/install_test.py` | paquete ↔ `fm1pkg.js`, marcador de loader, hashes | **PASS**, 39 |
| `web/test_web.mjs`, `test_backup`, `test_pkg_bounds`, `test_provenance`, `test_verify_only` | instalador, backup, límites del paquete, procedencia, Verify Only | **PASS** (cinco) |
| `tests/fuzz_ed.c`, `tests/fuzz_proj.c` con ASan + UBSan | SysEx/USB-MIDI y proyectos malformados | **PASS** los dos |

Ninguna protección se aflojó para que pasaran: no se tocó el allowlist de chips, ni el rechazo de chips
desconocidos, ni la custodia de claves, ni se introdujo credencial, firma o hash ficticio. El bootloader y
las regiones fuera de `[0x4000,0x93000)` siguen sin escribirse por diseño (`ldr_core.c`).

## 2. Compilación real y presupuestos `[AC791N]`

| Métrica | Valor |
|---|---|
| app | 582 104 B de 583 612 — **1 508 B libres** |
| RAM `.data+.bss` | 91 608 B de 98 304 — 6 696 B libres |
| POOL | 318 432 de 327 680 — 9 248 B libres |
| RAMCODE | 19 756 de 24 576 |
| paquete | 610 959 B, identidad **FM-1_965**, build `57530bc` |
| `tests/memory_map_test.py` | passed |
| `tests/target_budget.py` | **41 líneas `ok`**, ninguna sobre presupuesto |
| `tools/build_manifest.py --verify` | ok: versión, commit, config, artefactos y emulador coinciden |

## 3. Sonido, UI y estrés `[HOST]`

| Prueba | Resultado |
|---|---|
| `regress` (regresión sonora) | **303 renders, 0 changed**, 0 fallos de salud/voces/routing, 0 sobre CPU |
| `poly_report_test` (#107, BRASS/PLUCK, 1..12 teclas) | all passed: 12 voces, 0 robos, 0 denegadas |
| `poly_stress_test` (6 partes, 6 motores, FX, cambios de motor) | all checks ok: 12/12, PK 12, STL 8, DEN 0, 14 µs/bloque, 0 colgadas |
| `voice_defer_test` (carrera entre núcleos) | 9/9 ok |
| `ui_test` | all passed |
| `ui_render` (barrido de pantallas) | 191 pantallas × 22 paletas, FLAT/LINE, animación on/off: **0 hallazgos de lint**, 0 GREY/MONO fuera de gris, 3 783 017 medidas de alineación con 0 desvíos |
| `boot_order_test` | ok (hold antes del motor, apertura tras el core 1) |

## 4. Coherencia de la release

- Fuente, `release/felucca-v1.5.0.fwsc`, `site/firmware/felucca-1.5.0.fwsc`, `versions.json`, el instalador,
  el editor y el emulador describen **un solo paquete** (`tools/make_release.py --check`), y `site_check`
  pasa.
- El registro de build del paquete nombra el commit de **código** `57530bc`; el emulador se reconstruyó en
  ese mismo commit. El commit que agrega estos bytes (`release/` y `site/`) no cambia código, y por eso
  `build_manifest --verify` compara contra `HEAD` y queda un commit por detrás: es el comportamiento
  documentado en `docs/VOICE_POOL.md` §14 y la razón por la que CI reconstruye todo desde el tag.
- No se publicó ninguna release automática: no hay tag nuevo, y la firma de release (custodia fuera del
  árbol) no está disponible aquí. Lo publicado es `main` → Pages (sitio + paquete + emulador).

## 5. Limitaciones que siguen necesitando hardware

1. Pop de encendido y clicks de preset/motor/FX: corregida la **causa de orden** (hold antes del motor),
   sin medición acústica.
2. DUAL CORE real: reparto de partes, picos de carga del core 1 y latencia del diferido de notas.
3. Flash: JEDEC real, geometría, escritura, recuperación y corte de energía.
4. CPU de ambos núcleos con la carga de seis pistas (los 14 µs/bloque son de este host).
5. #106 (UI): las invalidaciones de región no se pueden verificar con el arnés actual, que dibuja frames
   enteros.
