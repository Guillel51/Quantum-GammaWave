# Etapa 2/8 — memoria y arquitectura: qué se midió, qué se cambió y qué se encontró roto

Clases: **`[AC791N]`** = lo que el linker/compilador del destino escribieron (`build/felucca.elf`,
`build/felucca.bin`, el log de `./build.sh`). **`[HOST]`** = x86 (`tests/regress`). Sin unidad física: nada de
esto es una medida `[HW]`.

## 1. Verificación del estado de partida

`main` estaba en `5842de1e` (Etapa 1: exp2 del FM6 como segunda diferencia) y el árbol limpio. La Etapa 1 decía
"306 golden renders, 0 cambiados" — **eso no era cierto** y esta etapa lo demuestra con un experimento:

| código | renders distintos de `tests/golden.txt` |
|---|---|
| `HEAD` tal como estaba (Etapa 1, sin tocar nada) | **65 de 306** |
| `HEAD` + inicializar la curva exp2 en `voices_boot()` | 31 |
| Etapa 2 (exp2 + SINE inicializadas + SINE en diferencias) | 31 |
| Etapa 2, con `tests/golden.txt` regenerado | **0** |

Causa: **ningún arnés de host pasaba por `fm6_exp2_init()`**. `fm6_exp2_tab` vivía en `.bss`, el host nunca la
llenaba y el motor FM6 renderizaba con una curva de ceros en todas las pruebas de audio desde la Etapa 1. Los
goldens de entonces se habían generado contra un binario anterior, así que la suite "pasaba" contra valores
viejos. Corregido en `voices_boot()` —el único punto de entrada de todos los arneses— y verificado con dos
corridas consecutivas idénticas. **Los 31 hashes regenerados no son un cambio de sonido: son los que el código
actual produce con las tablas correctas**, y las tablas están probadas punto por punto (abajo).

## 2. El recurso más limitado, medido otra vez

`[AC791N]`, antes de esta etapa:

| ventana | usado | ventana | libre | uso |
|---|---|---|---|---|
| **XIP (app slot)** | 580 900 B | 583 612 B | **2 712 B** | 99.54 % |
| **`.noinit`** | 15 572 B | 15 696 B | **124 B** | **99.2 %** |
| `.lowbuf` | 30 564 B | 32 768 B | 2 204 B | 93.3 % |
| RAM (`.data+.bss`) | 91 560 B | 98 304 B | 6 744 B | 93.1 % |
| pool | 318 432 B | 327 680 B | 9 248 B | 97.2 % |
| RAMTEXT | 19 728 B | 24 576 B | 4 848 B | 80.3 % |

**`.noinit` no lo miraba nadie**: 124 B libres, el 0.8 % de su ventana, y es donde vive `proj_slot[4]`
(4 × 3 840 B de caché retenida de proyectos). No es un descuido que se pueda arreglar sin quitar una función
(son los cuatro slots), así que queda como ventana vigilada con su propio piso y con el `ASSERT` del linker
como guarda dura.

## 3. La optimización implementada: la senoide

`SINE` eran **2 050 B de XIP** para 1 025 puntos de una curva cuyo paso nunca pasa de 201 y cuyo paso cambia
como mucho en 3. Guardada como **segunda diferencia en `int8`** son **1 023 B**, y `dsp.c:sine_init()` la
reconstruye en el arranque con sumas, en `.bss`, junto a `wt_gen_init` y la exp2 del FM6. Todos los usuarios
(`sine_i`, ADDTV, el formante de ANALOG, `fm6_sin`) leen la copia en RAM: el camino caliente cambia una lectura
de XIP por una de RAM, no se hace más lento.

### Antes / después `[AC791N]`

| | Etapa 1 | Etapa 2 | Δ |
|---|---|---|---|
| imagen (app slot) | 580 900 B | **579 876 B** | **−1 024 B** |
| libre en el slot | 2 712 B | **3 736 B** | **+1 024 B (+38 %)** |
| constantes XIP | 131 764 B | 130 716 B | −1 048 B |
| código XIP | 428 596 B | 428 620 B | +24 B |
| `.data+.bss` | 91 560 B | 93 612 B | +2 052 B (la copia en RAM) |
| libre en RAM | 6 744 B | 4 692 B | −2 052 B |
| pool / `.lowbuf` / `.noinit` | 9 248 / 2 204 / 124 | 9 248 / 2 204 / 124 | 0 |

Contra el inicio de la Etapa 1: imagen 582 884 → 579 876 B (**−3 008 B**), libre 728 → 3 736 B (**×5.1**).

## 4. Sistema de presupuestos y métricas por subsistema (nuevo)

`tools/resource_map.py` ahora lee **todas** las ventanas del `app.ld` (XIP, RAM, POOL, LOWBUF, NOINIT, RAMTEXT),
calcula lo usado por dirección de sección, y usa los dos números que el build imprime: la región hasta
`_bss_end` y el tamaño de `build/felucca.bin`. Cada ventana tiene **un piso** y `--check` falla si se cruza:

| ventana | piso | valor actual |
|---|---|---|
| XIP | 2 048 B | 3 736 B |
| RAM | 4 096 B | 4 692 B |
| POOL | 8 192 B | 9 248 B |
| LOWBUF | 1 024 B | 2 204 B |
| NOINIT | 64 B | 124 B |

Además imprime el reparto **por subsistema** (fm6, wavetable, dsp/voz, fx, ui/gfx, project, generator, sdk) a
partir de los nombres de símbolo, `--json` deja el mapa en `build/resources.json` y `tests/resources_test.py`
lo corre, verifica los pisos y fija la estructura de las dos tablas optimizadas (exp2 y SINE).

## 5. Investigado y descartado, con la medición

- **`UAC_FIR` (7 680 B)**: 80 fases × 24 taps, **0 filas repetidas, sin simetría de espejo** (`h[p] ≠ h[79−p]`),
  valores hasta 2^30 → necesita `int32` y no hay estructura que explotar sin pérdida.
- **Presets**: `preset_t` ya es todo `uint8`/`int8` (62 B por sonido, 286 sonidos ≈ 4 KB en total). No hay
  nada que empaquetar.
- **Iconos y keycaps** (`AI12/AI16/KC` ≈ 8 KB de XIP): ya se guardan a **nibble por píxel** (`cv_alpha_hc`,
  "HC" = half coverage). Empaquetarlos más exigiría un códec con pérdida.
- **`WT_COEF` / `WT_GEN`**: ya son la mitad de ciclo en `.bss` generada al arranque (Fase 1/6).
- **Buffers de 32 KB** (`sl_buf`, `cloud_buf`): son de audio y conviven (sonidos distintos de buses distintos);
  unirlos rompería FX simultáneos.

## 6. Pruebas ejecutadas

- `tests/sine_table_test.c` (**nuevo**, 6 checks): los 1 025 puntos decodificados iguales uno por uno al
  generador, rango `int8` de las diferencias, cuarto de ciclo exacto, simetría impar.
- `tests/fm6_exp2_test.c` (Etapa 1): 7 checks, sigue exacto tras mover la inicialización.
- `tests/regress`: **306 golden renders, 0 cambiados** (tras regenerar los 31 que el bug del host había dejado
  obsoletos), 0 fallos de salud, 0 de voz/routing (14 129 eventos aleatorios), 0 entradas de CPU sobre
  presupuesto, 0 crashes. Dos corridas consecutivas con hashes idénticos.
- `tests/resources_test.py`: 13 checks (pisos de las cinco ventanas + estructura de exp2 y SINE).
- `tests/target_budget.py`, `tests/groups.sh ui|presets|web`: sin cambios respecto de la Etapa 1 (0 hallazgos,
  0 de 3 785 110 medidas de alineación).

## 7. Pendientes para la Etapa 3

1. **La RAM es ahora el recurso más apretado en términos relativos** (95.2 %, 4 692 B). Lo siguiente que la
   libera de verdad es unir los buffers transitorios de `.lowbuf` (`proj_scratch` 9 620 B, `sec_stage` 9 422 B,
   `undo_st` 7 680 B): sus ciclos de vida parecen excluyentes, pero **falta la prueba** — hay que instrumentar
   cada uso y demostrar con un test que nunca coinciden antes de unirlos.
2. **`.noinit` (124 B)**: por diseño está lleno; cualquier estado retenido nuevo se pagará con un rediseño de
   `proj_slot` (por ejemplo, un solo slot retenido en vez de cuatro, que es una decisión de producto).
3. **Uso del núcleo 1 y stacks por tarea**: sin contador de carga ni "high water" expuesto; hoy no se puede
   medir desde el ELF ni desde el host.
4. **Estrés prolongado**: la suite cubre 16–18 s de audio por corrida; un lazo de minutos con cambios de preset
   y guardado/carga sigue sin existir.
