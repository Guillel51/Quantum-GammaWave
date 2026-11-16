# QG — FASE 1 (2026-11-16): XIP y RAM, medidos sobre el árbol real

**Base:** `main` = `21baf7d` (v1.1.0) · **Rama:** `freebuff/12463b6d6593903a8f08b9de`
**Clase de cada número, siempre:** `[TARGET]` build AC791N (toolchain JieLi `jieli-linux-toolchains-20260730.1`,
clang 4.0.1 `pi32v2`, SHA-256 del archivo `a1545cdd…7647bd` verificado contra `tools/toolchain.sha256`; SDK
`AC79NN_SDK_V1.2.1_2023-12-13`, los tres blobs con los hashes de `SDK_SHA256`), `[HOST]` x86-64 de esta VM,
`[RENDER]` coste de dibujo en host. **No hay `[FÍSICO]`:** en este entorno no existe ninguna unidad FM-1.
Ninguna cifra de este documento es una medición de hardware, ni de latencia, ni de audio real.

---

## 0. Veredicto en una página

| región | antes | después | delta |
|---|---:|---:|---:|
| Imagen `build/felucca.bin` | 583 524 B | **582 388 B** | **−1 136 B** |
| Holgura al slot XIP (`0x8E7BC` = 583 612 B) | **88 B** | **1 224 B** | ×13,9 |
| Código XIP | 418 732 B | 418 768 B | +36 B |
| Constantes XIP | 144 312 B | **143 140 B** | **−1 172 B** |
| `.data + .bss` | 96 836 / 98 304 B | **90 676 / 98 304 B** | **−6 160 B** |
| Holgura `.data+.bss` | 1 468 B | **7 628 B** | ×5,2 |
| POOL (spare) | 305 096 B (15 392) | 311 256 B (**9 248**) | piso 8 192 B respetado |
| RAM code (`RAMTEXT`) | 19 684 / 24 576 B | 19 684 / 24 576 B | 0 |
| LOWBUF / NOINIT spare | 1 948 / 124 B | 1 948 / 124 B | 0 |
| Paquete `.fwsc` | 610 959 B | 610 959 B | 0 (va rellenado al slot) |

El slot XIP era la restricción real: **88 B libres** antes de este cambio. No se ha quitado ninguna función,
motor, preset, voz, pista, efecto ni información de pantalla; no se ha cambiado ningún límite ni ninguna
prueba; no se ha introducido `malloc` en ninguna ruta.

Comprobaciones ejecutadas después de los cambios (§4): **293 renders dorados, 0 cambiados**; texto
bit a bit (0 de 6 732 cadenas, 0 píxeles); 190 pantallas × 22 paletas sin hallazgos de lint; 3 554 249
medidas de alineación, 0 desviadas; `tests/target_budget.py` 39/39 al 0 %; `tests/memory_map_test.py`
ok; suite de host completa en verde.

---

## 1. Qué se midió primero (y por qué el XIP era la urgencia)

```
./build.sh
python3 tests/memory_map_test.py
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
```

`tools/fm1pkg_make.py:APP_SLOT = 0x8E7BC` = **583 612 B** y la imagen del árbol medía **583 524 B**:
**88 B**. Cualquier función nueva era imposible. Las constantes (144 312 B) y el código (418 732 B) se
revisaron por separado, con `nm --size-sort -S` y el reparto por símbolo de `.rodata`.

También se midió lo que **no** había:

* **código duplicado**: hasheando el cuerpo de las 2 364 funciones del desensamblado solo hay **68 B**
  en tres pares idénticos (`osc_tri`/`dx_tri`, `fnv`/`as_mix`, `ev_len`/`tl_len`). `mergefunc` no
  compensa y no se usó.
* **cadenas duplicadas**: `-O` de este clang ya fusiona los literales idénticos del *unity build*
  (11 apariciones de `"LEVEL"` en el fuente, una copia en la imagen). Nada que ganar.
* **blobs de constantes duplicados**: 345 ventanas de 32 B repetidas y todas son filas de arrays de
  estructuras (paso 20 B), no tablas copiadas.
* **buffers muertos en `.bss`**: todos los símbolos de >= 64 B se referencian; los que aparecen una vez
  lo hacen a través de macros (`WT_GEN` vía `WT_GET`).

## 2. Lo que se cambió (y cuánto mide)

### 2.1 Tabla de glifos: el avance, una vez por glifo (XIP −840 B)

`tools/gen_aa_font.py` guardaba por **fase** la misma estructura `aag_t {off, adv, bx, by, bw, bh}`
(8 B) que por **glifo**. El avance es idéntico en las fases de un glifo (S 105/105, M 105/105,
L 41/41; el generador ahora lo **comprueba con un `assert`**, no lo supone) y `by`/`bh` no lo son en
2 glifos de M, así que solo se movió lo demostrable:

* `aag_t` pasa a `{off, bx, by, bw, bh}` = **6 B** y el avance vive en `AF_x_ADV[]` (2 B por glifo).
* S: 420 × 8 = 3 360 B → 420 × 6 + 105 × 2 = **2 730 B** (−630).
* M: 210 × 8 = 1 680 B → 210 × 6 + 105 × 2 = **1 470 B** (−210).
* L: 82 × 8 = 656 B → 82 × 6 + 41 × 2 = **656 B** (0: dos fases).
* Total **−840 B** de constantes; bitmaps, glifos y píxeles dibujados, intactos (lo prueba §4).

Lectores actualizados: `gfx.c` (`text_w`, `text_run`), `ui_menu.c` (el corte de líneas del documento),
`tests/text_ref_test.c`, `tests/theme_test.c` y el parser de `tests/text_spacing_test.py`.

### 2.2 `LFO_HZ_X100`: `uint32_t` → `uint16_t` (XIP −256 B)

Rango real 5..4000 (0,05..40 Hz ×100): cabe en 16 bits. `params.c` lo ensancha al leerlo, el valor
mostrado y el LFO no cambian. −128 × 2 = **−256 B**.

### 2.3 `up_bank` al POOL (RAM −6 160 B del presupuesto que ata)

El espejo RAM del banco de presets de usuario (6 160 B: 2 bancos × 16 registros × 192 B) solo lo toca el
bucle principal (guardar, cargar, navegar, el backup del editor); ningún camino del ISR lo lee. Va a
`.pool`, como `proj_wire_u`, `mut_anchor` y `song_keep` desde v1.2.1, y `main.c` ya pone el pool a cero
al arrancar (igual que `.bss`).

* `.data+.bss` 96 836 → **90 676 B**; holgura 1 468 → **7 628 B**.
* POOL spare 15 392 → **9 248 B** (el suelo declarado es 8 192 B: se respeta).
* Coste: **+36 B de código XIP** por materializar direcciones del pool (0x01C2xxxx) y −4 B de constantes.
* **Es un re-layout dentro de la misma ventana de SRAM, no memoria nueva.** Los 6 160 B siguen en el
  mismo chip; lo que cambia es qué presupuesto los cuenta. Se documenta así a propósito.

## 3. Lo que se midió, se probó y se **rechazó**

* **`-fdata-sections` en `CFLAGS`** (para que `--gc-sections` elimine datos no referenciados):
  imagen **582 388 B frente a 582 356 B** (+32 B por la alineación de cientos de secciones pequeñas).
  Revertido; no queda en el árbol.
* **Añadir a `SIZE_FILES` los ficheros de UI posteriores a la lista** (`ui_tech.c`, `ui_being.c`,
  `ui_tools.c`, `gen.c`, …): 898 → 1 307 funciones con `minsize noinline` y **solo −80 B**
  (`-Os` de este clang ya es casi tan pequeño), a cambio de 409 funciones más sin inlinear en el camino
  de dibujo. Revertido: no compensa el riesgo de coste de frame.
* **Mover `up_bank` "y además" `st_buf`**: dejaría el pool en 5 408 B de spare, por debajo del suelo.
  No se hizo.
* **Relleno de `track_t`**: 23 B por pista (138 B en seis) según el layout calculado; reordenar 90
  campos por 138 B no es una optimización que valga el riesgo. No se tocó.

## 4. Verificación ejecutada (clase de cada una)

| comprobación | resultado |
|---|---|
| `./build.sh` con toolchain y SDK fijados | ok; presupuestos del linker ok |
| `tests/memory_map_test.py` | passed; RAM spare 7 628 B, pool spare 9 248 B, RAMTEXT 4 892 B |
| `tests/target_budget.py build/felucca.dis tests/target_budget.txt` | 39/39 `ok`, **0 %** sobre la baseline, ninguna línea reescrita |
| `tests/regress` (dorados + salud + voces + CPU) | **293 renders, 0 cambiados**, 0 fallos de salud, 0 de voces, 0 de CPU |
| `tests/text_ref_test.c` | 6 732 cadenas × fuentes × paletas: **0 pens finales y 0 píxeles distintos** |
| `tests/theme_test.c` | paletas, contraste, blending, métricas y elipsis: passed |
| `tests/text_spacing_test.py` | S 0,239 px / M 0,044 px / L 0,425 px de peor error: **los mismos** topes de antes |
| `tests/ui_render` + `ui_render.py` | 190 pantallas × 22 paletas, **0 hallazgos de lint**, 0 píxeles GREY/MONO fuera de gris/neutro, **3 554 249 medidas de alineación, 0 a 1 px** |
| suite de host completa (`NO_PACKAGE=1 tests/run_tests.sh`) | ver el commit: en verde, sin grupos saltados nuevos |

No se ha debilitado ninguna prueba: las dos que construían `aag_t`/`aafont_t` a mano
(`theme_test.c`, `text_ref_test.c`) y el parser de `text_spacing_test.py` se actualizaron al nuevo
layout **sin cambiar sus umbrales** (S/M 0,25 px, L 0,5 px).

## 5. Reproducir

```sh
sh tools/get_toolchain.sh "$HOME/tc"        # pin verificado
# los tres blobs del SDK, con los hashes de tools/build.py (docs/BUILD_AC791N.md §6)
export JIELI_TOOLCHAIN="$HOME/tc/toolchain" AC79_SDK="$HOME/fw-AC79_AIoT_SDK"
python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
./build.sh
python3 tests/memory_map_test.py
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
NO_PACKAGE=1 ./tests/run_tests.sh
```

## 6. Lo que sigue sin poder afirmarse

* No hay unidad FM-1: nada de esto es una medición de CPU, latencia o audio del instrumento.
* No hay `emcc` en esta VM: el emulador WebAssembly no se reconstruyó.
* La instalación sigue **NO-GO** (`docs/VALIDATION_MATRIX.md`): este trabajo no aporta evidencia física.
