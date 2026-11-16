# FASE 8 — motor ADDITIVE y SOUND EDITED (2026-11-16, v1.2.6)

Parte del árbol publicado en `main` en v1.2.5 (`3e05fdc`). Todo lo que sigue está medido con instrumentos que
ya están en el repositorio, y cada medición dice de qué clase es. **No hay toolchain AC791N ni `emcc` en este
entorno**: no hay binario de la unidad, no hay XIP ni `.data+.bss` del ELF, no hay `.fwsc`, no hay emulador
reconstruido y no hay unidad FM-1. Lo que no se midió se dice como no medido, con la razón.

## 1. La decisión que la fase anterior dejó abierta

`docs/QG_FASE7_AUDITORIA_MOTORES_2026-11-16.md` dejó el motor aditivo como **no conveniente todavía**, y el
bloqueo era **XIP**, no CPU: ~3 416 B libres entonces, y un motor de 6–8 parciales estimado en 2,6–3,0 KB más
22 presets (792 B) → menos de 400 B de margen. Esta fase vuelve a evaluarlo con las mediciones de hoy y cambia
la arquitectura para que entre:

| decisión de la Fase 7 | esta fase |
|---|---|
| 8 parciales con envolvente espectral propia por parcial (estado ~40 B/voz) | 8 parciales con **decaimiento espectral derivado de un solo parámetro** (DECAY · k por bloque): el estado por voz son las 8 fases (32 B), 8 contadores de decaimiento Q16 (16 B) y 8 ganancias de arranque de rampa (16 B) = **68 B** |
| tabla de 22 presets (792 B) | **8 presets** (320 B), uno por familia sonora |
| perfil de espectro por preset | **8 perfiles compartidos** de 8 bytes (64 B) recorridos por un solo parámetro interpolado |
| — | **sin tabla de senos nueva**: `SINE` (Q15, 1 024 puntos), `tsvf`, `softclip`, `soft_knee`, `voice_amp` y el bloqueador de continua son los mismos que usan WAVETABLE y ANALOG |

## 2. Lo que se implementó

`firmware/src/eng_additive.c`, motor 15 (`ENGI_ADDITIVE`), 8 presets, página EDIT de tres páginas y CHARACTER
con **EVOLVE**. Ver `CHANGELOG.md` (v1.2.6) para el detalle de cada control. Lo que importa acá es el coste.

## 3. Memoria (medida sobre las estructuras reales, host x86-64)

`sizeof` de las estructuras del motor, leídas del firmware compilado:

| objeto | tamaño | unidades | total |
|---|---:|---:|---:|
| `ad_vc_t` (estado por voz: 8 fases, 8 decaimientos, 8 ganancias de rampa, sub) | 68 B | `VPOOLN` = 18 | **1 224 B `.bss`** |
| `ad_trk_t` (por parte: ganancias normalizadas y la fase de EVOLVE) | 36 B | `NPART` = 6 | **216 B `.bss`** |
| `preset_t` × presets | 40 B | 8 | **320 B flash** |
| `AD_PROF` (8 perfiles × 8 armónicos) | 64 B | 1 | **64 B flash** |
| `param_desc_t` × 8 + CHARACTER | — | — | 9 descriptores |

**`.bss` del motor: 1 440 B.** Con la holgura de `.data+.bss` que la Fase 6 dejó (3 532 B de 98 304 B), el
presupuesto queda en **~2 100 B libres** — por encima del mínimo de 8 KiB de *pool* (que este motor no toca:
no reserva buffers) pero fino para el presupuesto de 96 KiB. Es el número que hay que vigilar en el próximo
build AC791N, y el primero que hay que reclamar si una pantalla nueva pide RAM.

Código (tamaño de símbolo en el binario **host**, `nm --size-sort` sobre `tests/additive_test`; no es XIP):

| símbolo | host x86-64 |
|---|---:|
| `add_render` | 1 913 B |
| `add_block` | 895 B |
| `add_note_on` | 85 B |
| **total** | **~2,9 KB de código host** |

La Fase 7 estimó 2,6–3,0 KB de XIP para un motor así **más** 792 B de presets; acá los presets son 320 B y el
perfil es una tabla de 64 B, así que la estimación cae en la banda baja. **No está medido en XIP.**

## 4. CPU (medida, host x86-64)

`tests/eng_bench`, ocho teclas en T1, mejor de tres recorridos de 500 bloques de 32 muestras, con el piso
silencioso restado. Una sola corrida, para que las diferencias se lean entre motores y no entre corridas:

| motor | µs/bloque | menos el piso |
|---|---:|---:|
| WHEEL | 10,886 | 4,727 |
| WTABLE | 10,683 | 4,523 |
| NOISE | 10,527 | 4,367 |
| VOICE (FORMANT) | 9,560 | 3,401 |
| FM6 | 9,403 | 3,243 |
| **ADDTV** | **9,319** | **3,159** |
| DRUM | 8,217 | 2,058 |
| PHASE | 5,765 | −0,395 |
| LOFI | 4,706 | −1,453 |
| ANALOG | 6,085 | −0,075 |

**El motor nuevo no es el más caro**: queda en la banda de FM6, por debajo de WHEEL, WTABLE y NOISE con las
mismas ocho voces. La razón es que el trabajo por bloque (los ocho perfiles, la inclinación, la normalización,
los decaimientos, el límite de banda) se paga una vez por parte y por bloque, y los parciales que el perfil
deja en cero —o que el decaimiento ya apagó— no leen la tabla: sólo avanzan su fase.

## 5. Optimizaciones que esta fase midió y aplicó

1. **Trabajo por bloque fuera del lazo.** El perfil interpolado, la inclinación, la normalización a 32 767 y
   los ocho decaimientos se calculan una vez por bloque y parte. El lazo por muestra queda en una lectura de
   tabla, un multiply-add y una rampa de ganancia cuando la ganancia cambió.
2. **Parciales en cero sin tabla.** Un parcial con ganancia 0 avanza su fase (`ph += inc * n`) y no toca
   `SINE`: un `GLASS BELL` que ya decayó a su fundamental cuesta 1/8 de su ataque.
3. **Límite de banda en vez de plegado.** Entre ~4,5 y 6 kHz la ganancia del parcial baja en pendiente (una
   división por constante, que el compilador resuelve con un multiply-high) y arriba de 6 kHz el parcial no
   se renderiza. WHEEL **pliega** un octava y suena bien como órgano, pero un parcial plegado cae *entre* los
   armónicos: en un motor cuyo contrato es "el espectro son sus parciales" eso es un alias. `tests/additive_test`
   comprueba C7, A7 y C8 con un detector de armónicos (era el chequeo que este motor necesitaba y WHEEL no tiene).
4. **Una sola fase para SUB** (`v->ph[0]`) y el filtro/bloqueador de continua en `v->s[]`: el motor no agrega
   estado a `voice_t` ni cambia su tamaño.
5. **Estrechamiento de estado**: fases `uint32`, decaimiento `uint16 Q16`, ganancias de rampa `int16 Q15` —
   68 B por voz en vez de los ~100 B de una versión `int32` de los tres vectores.

## 6. SOUND EDITED (el pendiente del indicador de parámetros modificados)

Implementado como **comparación de huella**, no como hook: `snd_hash()` (FNV-1a sobre `p[P_COUNT]`, `mx[4][3]`,
motor y preset) contra `snd_ref[]`, que dejan la última carga (`load_end`), el último guardado (`up_store`) y
toda restauración de proyecto (`project_restore_runtime`: la carga de un proyecto, el recall de una sección
de la canción y un UNDO). `ui_notices()` lo evalúa una vez por cuadro y **nunca con el transporte andando**. Ventaja medible: cubre rutas que ningún setter central conoce (la capa rápida, el editor
SysEx, A/B, el recall, el cambio de motor) sin tocar una línea de ninguna de ellas. Coste: 4 × `NTRK` bytes de
`.bss` de referencia y ~750 operaciones por cuadro para el hash de la pista seleccionada (despreciable frente a
los 235 µs de un repintado de HOME en host).

Se muestra en el pie (una estrella tras el nombre del sonido), en HOME > OVERVIEW y como un punto en la fila
del sonido cargado en PRESETS. Cero parámetros nuevos, cero bytes en los formatos guardados.

## 7. Interfaz

- **EDIT del motor nuevo**: página con el espectro dibujado, ocho barras (una por parcial) con su numeral, en
  el mismo lenguaje que las barras de WHEEL. Se encienden en acento mientras se gira SPECTR o TILT.
- **PRESETS**: el punto de SOUND EDITED en la fila cargada.
- **Pie y OVERVIEW**: la estrella de SOUND EDITED.
- El barrido (`tests/ui_render`, 190 pantallas × 22 paletas, más MENU > LARGE) queda con **0 desalineaciones**
  de 3 894 154 medidas; el lint, con 0 hallazgos fuera del self-test.

## 8. Tests y verificación (clase de cada uno)

| qué | clase | resultado |
|---|---|---|
| `tests/additive_test` (nuevo) | host x86-64, fuentes reales | **all checks passed** (perfiles, crossfade, TILT 10 dB, DECAY −27,6 dB / −0,1 dB, STRETCH 3,375 f0, SUB, band limit C7–C8, 8 presets sin clipping/DC/silencio, esquina 7,25 × VOICE_FS, determinismo, coste) |
| `tests/regress` | host | **301 renders: 0 cambiados, 8 nuevos, 0 perdidos**; 0 fallas de salud, 0 de voz/ruteo, 0 entradas de CPU sobre presupuesto |
| `tests/eng_bench` | host | tabla de §4 |
| `tests/ui_test` | host | todos pasan (la lista de motores mostrados incluye ADDTV) |
| `tests/ui_render` | host, renderizador real | lint y alineación (ver §7) |
| `./build.sh` (AC791N) | — | **no ejecutado: no hay toolchain Jeli / SDK** |
| `.fwsc` / release | — | **no generados** por lo mismo |
| unidad FM-1 | — | **no hay aparato** |

## 9. Lo que queda pendiente, con nombre y número

1. **Build AC791N y medición de XIP / `.data+.bss` del ELF.** Es el único número que falta para cerrar el
   presupuesto del motor nuevo; la estimación de código host (~2,9 KB) sugiere que entra, pero **no está medido**.
2. **Escuchar los ocho presets en la unidad** (balance de niveles frente al resto de la banca): los ocho pasan
   salud en host y la normalización mantiene el nivel entre perfiles (±6 dB del fundamental), pero el juicio
   final es del aparato.
3. **Emulador WASM**: sin `emcc` no se reconstruye; `release/emu` sigue en 1.1.0 y la release pública sigue
   bloqueada por la regla de que todos los componentes sirven la misma versión.
4. **Recorte de la banca ANALOG** (68 → 48, 1 116 B de XIP): sigue sin ejecutarse, por la razón de la Fase 7
   (borrar ids de fábrica es destructivo y exige escucha comparada).
