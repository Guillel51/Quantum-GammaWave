# FASE 6 — opciones nuevas en los motores: una entra, la otra queda medida

La Fase 3 pedía opciones nuevas **en los motores** y lo que quedó fueron cosas transversales (LFO, matriz de
modulación, tipos de filtro). Esto diseñó dos, cada una en un motor, con su control, su test y las dos medidas que
importan: **XIP** y **CPU del lazo de render**. Una entra; la otra no, y el número dice por qué.

**No hay FM-1 en este entorno.** Nada de este documento afirma cómo suenan en la unidad: son medidas `[HOST]`
(render del motor en x86) y `[AC791N]` (tamaño de imagen y presupuesto de instrucciones del build de destino).

## 1. La restricción real

Al empezar había **888 B libres de XIP** (la Fase 4 citaba 1 272 B; ya no era el número) y un presupuesto de DSP por
función que el CI verifica sobre la desassemblada (`tests/target_budget.py`, +10 % sobre el baseline).

Eso descarta cualquier opción con **id de parámetro nuevo**, control nuevo o cambio de formato de preset: lo que
entra es un **valor nuevo de un control que ya existe**, con la lista extendida por detrás. También descarta los
motores cuyos selectores están enmascarados a potencia de dos en el camino caliente (`PHASE` WAVE `& 7`, `LOFI` CHIP
`& 3`, `LOFI` WAVE RAM `& 15`, `NOISE` MODE `& 3`): ahí un valor nuevo cambia una máscara en el render.

## 2. Lo que entra: LOFI · WAVE = SYNC (valor 7)

Un pulso cuya fase corre **2 a 5 veces** la de la nota y se reinicia con ella —el *hard sync* de los chips, que este
motor no tenía: tenía pulso, triángulo, sierra, ruido, wave RAM, bytebeat y floatbeat.

- **Control:** `WAVE` (octavo valor) y **DUTY** para la relación. CHIP, CUT, CRUSH y MORPH siguen igual; el modo
  también entra en `lofi_mto`, así que MORPH hacia SYNC lo barre.
- **`[AC791N]`:** **+64 B** de XIP — imagen **582 788 B** de 583 612, **824 B libres**.
- **CPU `[AC791N]`:** `lofi_render` 197 → **206** instrucciones en lazos (**+5 %**, dentro del +10 %).

### Medición `[HOST]` de que suena y de que es otro sonido

`tests/engine_options_test.c` renderiza la nota mantenida del preset 0 con WAVE en **todos** sus valores y compara
las muestras: la distancia es `rms(a − b) / rms(a)`.

| WAVE | rms | cruces por muestra | distancia a SYNC |
|---|---|---|---|
| 0 PLS | 8132 | 0,0117 | **141 %** |
| 1 TRI | 5219 | 0,0137 | **119 %** |
| 2 SAW | 5296 | 0,0117 | **119 %** |
| 3 NOIS | 7944 | 0,0005 | **140 %** |
| 4 WRAM | 6744 | 0,0313 | **129 %** |
| 5 BYTE | 4585 | 0,0054 | **115 %** |
| 6 FLOAT | 2598 | 0,0205 | **105 %** |
| **7 SYNC** | **8162** | **0,3659** | — |

Además, la biblioteca no se movió: **0 presets** de fábrica usan el valor 7, **306 golden renders, 0 cambiados**,
`engine_param_test` verde (240 knobs × 3 presets, ninguno sin efecto).

## 3. Lo que no entra: PHASE · LINE = FOLD, y por qué

Diseño: la línea 2 ya podía mezclarse (MIX), modular en anillo (RING) o modular en fase la línea 1 (PM); FOLD
**plegaba** la suma en los rieles (un *wavefolder*), con WAVE2 dando la forma y DTN el detalle, sobre el mismo knob
`LINE`.

| medida | resultado |
|---|---|
| XIP `[AC791N]`, sola | **+96 B** — entraba (792 B libres) |
| CPU `[AC791N]`, `phase_render` | **94 → 107 instrucciones en lazo (+14 %)** — **sobre** el presupuesto de +10 % |
| CPU `[AC791N]`, `phase_render_noise` | **113 → 125 (+11 %)** — también sobre |
| intento de bajar el costo (plegar sobre la magnitud, sin un segundo compare) | 107 igual: el costo no está en el `if` sino en el lazo que lo acompaña |
| variante para no pagarlo con FOLD apagado (lazo propio) | no cabe: duplicar el lazo de `phase_render` son cientos de bytes y quedaban 744 |

Por eso `LINE` sigue terminando en **PM** y el test lo verifica (`PHASE LINE still ends at 2: FOLD did not ship`),
para que el código, la documentación y la pantalla no puedan separarse. **v1.5.28 llevó FOLD y su CI falló en
`target_budget.py`**; v1.5.29 lo saca y publica la medición en lugar de subir el baseline para que pase.

## 4. Lo que sigue sin entrar

La caché de ventanas angostas de `gfx.c` (21 440 px por frame quieto frente a 29 952) pide **3 640 B** y quedan
**824 B**: sigue detrás de `FELUCCA_WINCACHE`, con el camino medido en `docs/FASE6_XIP_NARROW.md` §5 (decodificar la
fuente L al arranque: −4 270 B de XIP por ~5 KB de `.bss`).

## 5. Cómo se reproduce

```
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/engine_options_test tests/engine_options_test.c -lm
./build/engine_options_test                                  # [HOST]: sonido, distancias, biblioteca, FOLD ausente

cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/regress tests/regress.c -lm
./build/regress tests/golden.txt tests/cpu_baseline.txt      # 306 renders, 0 cambiados

JIELI_TOOLCHAIN=… AC79_SDK=… ./build.sh                      # [AC791N]: 582 788 B, 824 B libres
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt    # exit 0
```
