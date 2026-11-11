# Registro maestro de pendientes y recursos — QG-1 (2026-11-11)

Punto de partida: rama `freebuff/741102a3c5c51ac0c8b8399e`, commit `39a23a2` (sobre `0490846`);
`main` local está en `787d76f` (v2.7.1), es decir **por detrás** de esta rama. Nada de lo aquí escrito se ha
medido en un FM-1: no hay unidad. Clases de evidencia: `host x86`, `render host`, `AC791N` (build del
proyecto, **no re-ejecutado aquí**: faltan `JIELI_TOOLCHAIN` y `AC79_SDK`), `WASM` (sin `emcc`), `físico`.

## 1. Matriz de pendientes

| Pendiente | Origen | Estado real | Problema | Solución | Prueba |
|---|---|---|---|---|---|
| M STRT | encargo previo | **resuelto**: es el *micro-start* | ninguno: `P_MSTART` ("M.STRT") dispersa el inicio de nota 0..~19,6 ms | ya conectado en `voice.c:717-724`, página `VOICE 2`, Web Studio | `tests/microstart_test.c`, `VALIDATION_MATRIX.md:1365,1367` |
| Splash sin BOOT/% | encargo previo | **resuelto** | — | `ui_draw.c:1669` documenta el readout eliminado; no queda la cadena `BOOT` en el firmware | `ui_render` mide su línea de tiempo (775 pasos) |
| Medidores de la página FX | encargo previo | **corregido** (`39a23a2`) | `vu[]`/`hold[]` se leían y nunca se escribían: estela, peak-hold y tick muertos; barra dibujada 3 veces | `fx_meter_step()` en `core.h` + dibujo único, hold, tick, CLIP | `tests/fx_meter_test.c` 13/13; `ui_test` 880 ok; lint 0 |
| Coste de las páginas FX/Reverb/Master/DRUM-X | este encargo | **instrumentado** | el informe de coste no medía esas pantallas | añadidas a `tests/ui_render.c` (COST) | §2.2 |
| Pantalla propia de PHASE | encargo previo | no implementada | PHASE usa EDIT 1/2 + MORPH + FILTER; añadir una página desplaza índices del protocolo de editor | diseñar un `GR_*` nuevo y una pantalla al final de PAGES **con** actualización de la tabla JS y `desc.json` | pendiente |
| MORPH con estados A/B persistentes | encargo previo | MORPH existe y suena; A/B no se guarda | el formato de preset no tiene dos estados | formato nuevo + migración probada | pendiente |
| Animaciones de Reverb por modelo | encargo previo | hay 5 modelos reales y pantallas por modelo | falta identidad visual diferenciada por modelo | auditar `fx.c` modelo a modelo antes de tocar dibujo | pendiente |
| Presets nuevos (42 y familias) | encargo previo | **215 de fábrica** (ver §3) | no hay presets de las familias pedidas más allá de los existentes | crear con render A/B y calibración `preset_level.c` | pendiente |
| Noveno motor | este encargo | **evaluado, no viable ahora** | sin presupuesto de POOL para buffers | ver §4 | — |
| Build AC791N / WASM / emulador / release | este encargo | **bloqueado** | sin toolchain ni `emcc`; sin unidad | — | **no ejecutado** |

## 2. Recursos

### 2.1 Presupuesto del proyecto (clase AC791N build, **citado, no re-medido**)

De `docs/OPTIMIZATION_BASELINE.md`, dos puntos de medida distintos; se dan ambos porque el árbol actual es
v2.7.1 y **ninguno** se ha vuelto a medir en esta sesión:

| región | v2.3.0, pool, 12 voces (`9f50002`) | v2.2 y anteriores (bloques por parte) |
|---|---:|---:|
| `.data + .bss` | 87 584 B de 98 304 → **10 720 libres** | 94 920 B → 3 384 libres |
| POOL | 311 240 de 327 680 → **9 248 libres** (suelo 8 KiB) | igual |
| LOWBUF | 26 532 de 28 672 → 2 140 | igual |
| Código RAM | 19 268 de 24 576 → 5 308 | 19 284 → 5 292 |
| XIP | 379 876 de 581 052 | 372 504 |
| Imagen | 539 836 B de 1 MiB NOR | 532 088 B |

Lectura útil: **el margen real está en `.bss` (~10,7 KiB) y en XIP (~200 KiB); el POOL no tiene margen
utilizable** (9 248 B − 8 KiB de suelo ≈ 1 KiB) y LOWBUF ~2 KiB. Cualquier motor nuevo que necesite buffers
grandes está descartado por construcción.

### 2.2 Coste de dibujo (clase `render host`, `cc -O1`)

Añadidas las cuatro páginas al informe de `tests/ui_render.c`. Medición de esta sesión, misma máquina, dos
binarios (HEAD~1 y este árbol), 200 redibujados por pantalla:

| pantalla | antes (µs) | después (µs) | textos |
|---|---:|---:|---:|
| fx | 132.0 | 122.6 | 49 |
| reverb2 | — | 103.9 | 25 |
| master | 113.8 | 104.2 | 31 |
| drumx | 123.4 | 114.0 | 34 |

**No se atribuye ninguna mejora a los medidores**: `master` y `drumx` no se tocaron y bajaron lo mismo
(~7-8 %), así que la diferencia es la máquina entre las dos tandas, no el código. Lo único defendible: **no
hay regresión detectable** en la página FX y ahora su coste es medible. Los números son de host; en el
dispositivo sólo valen las proporciones dentro de una misma tanda.

## 3. Inventario real de presets de fábrica (contado en el código)

`grep`/conteo de las tablas `*_PRESETS[]` en `firmware/src/eng_*.c` + `fm4_convert.c`:

| motor | tabla | fábrica |
|---|---|---:|
| ANALOG | `eng_analog.c:308` | 56 |
| FM6 | `eng_fm6.c:472` | 29 |
| PHASE | `eng_phase.c:203` | 27 |
| LOFI | `eng_lofi.c:573` | 26 |
| VOICE (FORMANT) | `eng_formant.c:205` | 20 |
| WHEEL | `eng_wheel.c:282` | 20 |
| NOISE | `eng_noise.c:244` | 17 |
| DRUM / DRUM-X | `eng_drum.c:519` | 12 |
| DIGITAL (retirado, se convierte a FM6) | `fm4_convert.c:66` | 8 |
| **total** | | **215** |

Corrige el "200 factory presets" que circula en documentos anteriores. Los 8 de DIGITAL se conservan porque
sus sonidos se convierten a FM6; no son seleccionables como motor.

## 4. Evaluación del noveno motor (Fase 12)

| candidato | utilidad musical | Flash | RAM | CPU | riesgo | veredicto |
|---|---|---|---|---|---|---|
| Granular ligero | alta (texturas) | bajo | **≥16-32 KiB de buffers** | medio-alto | no cabe: POOL ~1 KiB útil, LOWBUF 2,1 KiB | **no viable** |
| Karplus-Strong | media (cuerdas) | bajo | línea por voz (~1-2 KiB × 12) | bajo | no cabe en POOL | **no viable** |
| Wavetable compacta | alta (identidad no cubierta) | tablas en XIP (hay ~200 KiB) | ~4-8 B por voz | bajo-medio | **el único candidato que podría caber** | **no implementado**: sin build AC791N aquí no se puede demostrar el presupuesto, y no se anuncia sin DSP+editor+presets+pruebas+Web Studio |

Decisión: **no se implementa un noveno motor en esta pasada**. Mejorar la profundidad de edición de los ocho
existentes es la vía con margen real.

## 5. Triage de PRs y ramas sin integrar

Las ramas `origin/pr/23`..`origin/pr/35` **no son material recuperable tal cual**: son forks del árbol
anterior a QG-1. Contra `main` actual divergen en ~700 archivos, con 73 000 líneas de borrado: revierten la
marca a "Pocket GammaWave", eliminan `ci_diag.yml`, `spi_dump_test` y el verificador externo, y sus cambios
de UI **quitan** refinamientos que hoy están en el árbol (p. ej. `pr/33` borra la barra de recorrido A→B de
MORPH y sustituye el teclado de SCL por un panel plano). Conclusión: no se fusiona ninguna; su contenido útil
ya está superado por `main`. `origin/pr/35` (13 archivos, +228/−11) queda anotada como la única con algo
potencialmente aprovechable, también sobre árbol viejo.

No hay stashes. No hay worktrees ajenos accesibles.

## 6. Bloqueos que impiden cerrar el encargo

1. **Build AC791N**: sin toolchain pi32v2 y sin SDK no hay imagen, ni presupuesto medido del árbol actual,
   ni paquete que publicar.
2. **WASM**: sin `emcc` no hay emulador nuevo; por tanto no hay prueba de navegador ni sincronización pública.
3. **Unidad física**: no existe; ninguna afirmación de hardware, latencia o estabilidad es posible.
4. Consecuencia: la Fase 16 (publicación y verificación pública) **no puede ejecutarse** aquí y no se declara
   hecha.

## 7. Qué se ha cerrado en esta sesión

- Medidores de FX con balística real, peak-hold y CLIP por bus (`39a23a2`), con prueba unitaria propia.
- M STRT verificado como implementado (no había nada perdido que recuperar).
- Splash verificado sin BOOT ni porcentaje.
- Inventario exacto de presets (215) y de modelos de Reverb (5 reales: ROOM, SPRING, SHIMR, RESO, CLOUD).
- El informe de coste de UI mide ya FX, Reverb, Master y DRUM-X.
- Triage de las ramas `pr/*` con veredicto razonado.
