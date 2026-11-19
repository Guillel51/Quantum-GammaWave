# Fase 2 — Issues #106 (UI) y #107 (polifonía): qué se midió, qué se corrigió, qué falta

Todo lo de esta página es `[HOST]` (x86-64, las fuentes reales) salvo donde dice `[STATIC]`. **No hay
medición en una FM-1**: no hubo unidad. Ningún punto se declara resuelto en hardware.

## 1. #107 — voice stealing vs. polifonía: la medición

`tests/poly_report_test.c` recorre el camino exacto del reporte: el preset del motor, el teclado de 27
teclas (`fm1_in.notes`), la capa de teclas (chord/strum) y `trk_note_on`, con 1, 4, 6, 7, 8, 11 y 12
teclas, una por una y todas en un mismo scan.

| Caso | Sonando / 12 | Robos | Ventana `vn` |
|------|--------------|-------|--------------|
| POLY genérico, 12 teclas de una vez | 12 | 0 | 12 |
| ANALOG BRASS, 12 de una vez / una por una | 12 / 12 | 0 | 12 |
| ANALOG PLUCK, 12 | 12 | 0 | 12 |
| BRASS + `load_tight` (lo que audio.c pone sobre 65 %) | 12 | 0 | 12 |
| BRASS + DUAL CORE (todas las partes en el core 1) | 12 | 0 | 12 |
| PLUCK + SUSTAIN 100 | 12 | 0 | 12 |
| BRASS, acorde largo (400 bloques ≈ 0,29 s) | 12 | 0 | 12 |
| 6 partes × 3 notas (`poly_voices_test`) | 18 pedidas / tope del pool | — | — |

**Conclusión de la medición:** el asignador no tiene un tope de 7. `voices_boot` reparte
`VDEAD/NPART = 2` slots por parte, `voice_window_grow` toma los 5 slots libres (12..16) y presta los
sobrantes de las otras partes, y una parte sola llega a 12. El "7" del reporte **no se reproduce** en el
host en ninguna de las configuraciones del issue; lo que queda por explicar en la unidad es el camino que
el host no puede medir: el guardián de sobrecarga (`shed_voice`, audio.c) apaga una voz cada dos mitades
por encima del 80 % — con 12 voces de BRASS y FX eso corta notas mantenidas — y la carrera de ventanas
entre el core 0 (que asigna) y el core 1 (que renderiza partes 3..6).

### Lo implementado

- `voice.c`: `voice_peak`, `voice_denied`, `voice_peak_now()` (pico contado donde arrancan las voces, no
  en el refresco de pantalla), `voice_stats()` / `voice_stats_reset()` con **sounding / release / fading
  / peak / steals / denied** por separado.
- `vshare()` cuenta sólo las pistas **vivas** (`trk_live`, por defecto 4): antes T5 y T6 — dormidas —
  entraban en el reparto y una parte con 3 voces leía como "por encima de su parte" mientras cuatro
  slots estaban en pistas que no pueden sonar.
- El monitor (`ui_graph.c`, fila POLYPHONY) imprime `N / 12 VOICES`, `REL`, `FADE`, `PK`, `STL`, `DEN`.
  El pico se reinicia al abrir la pantalla (`ui.force`), así que ya no depende del frame en que se mira.
- `tests/poly_report_test.c` (nuevo, registrado en `tests/run_tests.sh`).

### Pendiente (fase 3)

1. `[HW]` Reproducir el 7 en la unidad con este monitor: si aparece `FADE`/`STL` creciendo, la causa es
   el guardián de sobrecarga; si `REL` no baja, es un RELEASE que no termina.
2. `[HW]` Instrumentar `shed_count`/`shed_voice` por track y medir el coste real de 12 voces de BRASS
   con FX en el AC791N; si el techo es DSP, optimizar el render por voz (no bajar NVOICE).
3. `[STATIC]` `voice_window_grow` escribe `vidx[]` de otra parte y `vpool[s]` mientras el core 1 puede
   estar renderizando esa parte: exige un punto de sincronización explícito con `mc` (hoy se confía en
   que la asignación ocurre entre bloques).
4. `[HW]` Verificar que el pico nuevo no gasta tiempo de ISR (es un `voices_busy()` por note-on).

## 2. #106 — UI: lo corregido y lo que falta

### Corregido

- **Parpadeo de las líneas superiores (HOME 2–5).** `hv_view_sig()` metía `song.cpu_q8/4` y los medidores
  con un paso de `>> 9` en la firma que decide si se repinta el cuerpo entero (240×176 en tres blits SPI,
  la cabecera primero). Con PLAY, la carga y los VU cambian cada frame: el cuerpo —y con él la línea de
  arriba— se rebliteaba hasta 60 veces por segundo aunque ningún píxel hubiera cambiado. Ahora la carga
  no entra en la firma (ninguna de esas tres pantallas la dibuja; SYSTEM sí, y tiene la suya) y los
  medidores se cuantizan a los píxeles que se dibujan (9 segmentos en MIXER, 12 px en OVERVIEW).
- **Corte de audio al cambiar preset (#106 GLO > MIXER).** `apply_preset_to` escribía los ~40 parámetros
  con la IRQ de audio abierta: el ISR podía leer el sobre nuevo con el filtro viejo durante un bloque.
  Ahora el bloque de asignaciones va con `fm1_irq_off/on` (sólo asignaciones: ni flash ni reservas), y
  `fx_preset_flush()` —que vacía las líneas de delay/reverb de **todas** las pistas— ya no se ejecuta
  cuando la pista que cambia de sonido no tiene ninguna voz activa. Ese era el corte que se oía "aunque
  el preset no esté sonando".

### Pendiente (fase 3), con la causa localizada

1. **Nombre de preset en la zona dB (fragmento de pop-up).** `cv_free_text` sí recorta al ancho de la
   tarjeta, así que el fragmento no es un desborde de texto: es una **región invalidada a medias** — el
   pop-up que muestra el nombre escribe sobre la zona de medidores y al cerrarse no se repinta toda su
   caja. Corregir en el cierre de pop-up/overlay (invalidar la caja completa, no sólo la fila del texto)
   y añadir el caso al lint de `tests/ui_render.py`.
2. **TEMPO sobre los indicadores de T3/T4.** El icono/valor vive en la cabecera (`ui_draw.c:215-234`,
   `draw_header`) y la tira de seis columnas del MIXER arranca en `Y_GRAPH`; con `MENU > LARGE` el badge
   cae sobre las columnas 3 y 4. Mover el badge a la banda de la cabecera que no comparte fila con la
   tira (o acortar la tira) y verificar con el lint de alineación.
3. **ALGORITHM repinta la caja MIXER entera.** `draw_tracks()` usa una firma **por columna**
   (`ts.col[c]`), pero el cambio de pista con ALGORITHM invalida con `ui.force` (repintado completo:
   `lcd_fill` de toda la tira + las seis columnas). Hace falta una invalidación selectiva: comparar el
   estado por columna y no forzar el marco cuando sólo cambia `song.sel`, y separar en la firma las
   filas LEVEL/PAN de los indicadores superior/inferior para que no se rebliteen.

## 3. Verificación ejecutada en esta fase

| Check | Resultado |
|-------|-----------|
| `build/poly_report_test` (nuevo, issue #107) | **all passed** (12/12 en todos los casos) |
| `build/poly_voices_test` (existente) | **all passed** |
| `build/ui_test` (fuentes de UI reales, con stubs) | **all passed** |
| `build/regress tests/golden.txt tests/cpu_baseline.txt` | **303 renders, 0 changed**, 0 health/voice failures, 0 CPU over budget |
| `[HW]` | **no ejecutado**: sin unidad |

Nada de lo anterior es una medición de una FM-1, y ninguno de los dos issues se marca como resuelto.
