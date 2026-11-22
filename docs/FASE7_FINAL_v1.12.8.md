# Fase 7 — cierre, auditoría y release final (v1.12.8)

## Qué se cerró y qué no

Esta fase se propuso "solucionar absolutamente todo lo pendiente". Lo que sigue es lo que **está comprobado** y lo
que **sigue abierto**, con la evidencia que hay en el árbol. Nada de esto se apoya en la unidad física: no hay una.

### Cerrado en las siete fases

| fase | entrega | evidencia |
|---|---|---|
| 1 | `ui_render --home`, `batt_icon()` muerto eliminado, §7 de V16, v1.12.2 | `--home` 5 pantallas; 191×22 paletas, 0 lint |
| 2 | polifonía medida (12 voces; 16 = +13 % y no entra en RAM); 2 optimizaciones descartadas **por medición** | `fx_bench`, `poly_stress_test` 12/12 |
| 3 | **TILT** (inclinación espectral por voz), id 125, página EDIT, `MD_HARM`, CC 76 | `tests/tilt_test.c` |
| 4 | **LFO CURVE** (fase doblada), id 126 = último id, `MD_LSHP`, CC 77; **bug PLUK 32768 corregido** | `tests/lfo_test.c` |
| 5 | **ENV ONE** (release sin soltar la tecla) sin id nuevo | `tests/env_test.c` |
| 6 | `graph_tilt()`: la página TILT dibuja su estante | compilado; **sin verificación visual** |

### Abierto, con nombre y sin maquillaje

1. **Cobertura del barrido UI.** `tests/ui_pixels.txt` **no contiene ni un solo frame** de las páginas `TILT` ni
   `LFO CURVE` (comprobado: `grep -c tilt` = 0, `grep -c lfo_curve` = 0, 0 archivos en `build/ui_new/ppm`).
   El síntoma está confirmado; **la causa no**. `page_visible()` las admite por lectura y `NPAGES` = 90 (no hay
   desbordamiento de `ui.page`), así que el filtro está en otro sitio del recorrido de `sweep_columns()` y no se
   encontró en el tiempo de esta fase. **El dibujo de `graph_tilt()` está compilado, integrado y no lo ha mirado
   nadie.** Es el defecto más importante que queda.
2. **Presets de fábrica** sin TILT, CURVE ni ONE (cuatro fases declarándolo): `preset_level.c` y los 307 renders
   dorados son medidas generadas y `regress` no tiene ruta de re-baseline.
3. **Ids de parámetro: 127/127.** Cualquier control nuevo exige formato de almacenamiento nuevo.
4. **Instalación desde stock V16: GO CONDITIONAL**, sin cambios. Ver §7 de `docs/FM1_V16.md` y abajo.
5. **Tag y GitHub Release**: `gh auth status` no tiene sesión en este entorno. El paquete sellado queda en
   `release/` y `site/`.

## Optimización final: lo que se midió y lo que no

- No se añadió ninguna optimización en esta fase. Las dos candidatas de la Fase 2 se midieron y se **descartaron**
  (dentro del ruido del host); inventar una tercera sin medición es exactamente lo que `AGENTS.md` prohíbe.
- Estado de los presupuestos al cierre: imagen **580 488 / 583 612**, `.data+.bss` **93 640 / 98 304**, reserva del
  pool **9 248 B**, bloque de audio **725 µs** con BLOCK_PATH en 45 704 (tope +10 %).
- **FPS del panel**: no medidos. No hay unidad y el barrido mide píxeles, no tiempo de panel. Cualquier cifra de FPS
  en este informe sería inventada, así que no hay ninguna.

## Seguridad (auditoría de lo que hay en el árbol)

- **Instalador**: el gate exige SHA-256 + tamaño del paquete, y el firmware sólo acepta dos JEDEC (`0x856014`,
  `0xB36014`) por allowlist explícita; cualquier otro se rechaza.
- **Backup**: el camino documentado (`docs/BACKUP.md`, `BACKUP_RESTORE.md`) cubre la ventana que el loader escribe
  (`0x4000–0x93000`); **`USR`/`BTIF`/`NVS` no tienen ni una medida en V16**, y por eso el gate **no** se relaja.
- **Sin claves en el árbol**: Ed25519 (`qg1-release-1`) es histórico; el paquete se valida por hash y tamaño, no por
  firma privada. No se ha generado ni escrito ninguna clave en esta fase.
- **Nunca se escribió la unidad física**: no existe en este entorno, y `AGENTS.md` §3 lo prohíbe en cualquier caso.

## Instalación desde V16: el impedimento, exacto

Se puede volver a stock sin el dump de 1 MiB **si y sólo si** se acepta que `USR`/`BTIF`/`NVS` (`0x93000–0xFFFFF`)
queden sin evidencia de su contenido original en un V16: la vía corta (backup selectivo de la ventana del loader +
archivo oficial V16 con hashes fijados) cubre todo lo que una instalación **toca**, no lo que la unidad **guarda**.
Sin una lectura de esas tres regiones en un V16, relajar el gate sería afirmar algo que no se ha medido. Queda
escrito, con sus tres pasos, para la fase que tenga hardware.

## QA ejecutado en esta fase y en las anteriores

`ui_test`, `regress` (**307 renders, 0 cambiados**), `tilt_test`, `lfo_test`, `env_test`, `poly_stress_test` 12/12,
`poly_budget_test`, `gate_test`, `resources_test` 13/13, `release_gate_test`, `test_web.mjs` (incluye backup, bounds
y stock releases), `emu_test.mjs`, `make_release --check`, `sha256sum -c`, barrido **191 pantallas × 22 paletas, 0
lint, 0 desalineaciones** (3 828 645 medidas).
