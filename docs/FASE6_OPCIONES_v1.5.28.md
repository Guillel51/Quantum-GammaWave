# FASE 6 — dos opciones nuevas en los motores, medidas

La Fase 3 pedía opciones nuevas **en los motores** y lo que quedó fueron cosas transversales (LFO, matriz de
modulación, tipos de filtro). Esto agrega dos opciones de motor, audibles, cada una en un motor distinto, con su
control, su test y la medición de XIP —o el número que demuestra que no entra.

**No hay FM-1 en este entorno.** Nada de este documento afirma cómo suenan en la unidad: son medidas `[HOST]`
(render del motor en x86) y `[AC791N]` (tamaño de la imagen del build de destino).

## 1. Qué se eligió, y por qué en esos motores

La restricción real no es el DSP: es el XIP (**888 B libres** al empezar; la Fase 4 citaba 1 272 B, que ya no era el
número). Eso descarta cualquier opción que pida un **id de parámetro nuevo** (quedan 2 y no hay página libre), un
control nuevo en la UI, o cambiar el formato del preset. Lo que sí entra es una **opción que es un valor nuevo de un
control que ya existe**, con la lista de valores extendida por detrás: los ids, las páginas, el `.bss` y el formato
quedan como estaban, y ningún preset de fábrica usa el valor nuevo.

Eso descarta también los motores cuyos selectores están **enmascarados a potencia de dos** en el camino caliente
(`PHASE` WAVE con `& 7`, `LOFI` CHIP con `& 3`, `LOFI` WAVE RAM con `& 15`, `NOISE` MODE con `& 3`): agregar un valor
ahí obliga a cambiar una máscara en el render, y cuesta más de lo que hay. Los dos elegidos no la tienen:

| motor | control | valores | por qué |
|---|---|---|---|
| **PHASE** | `LINE` (`P_E5`) | MIX / RING / PM → **FOLD** | se lee sin máscara (`line = p[P_E5]`) |
| **LOFI** | `WAVE` (`P_E1`) | PLS TRI SAW NOIS WRAM BYTE FLOAT → **SYNC** | se lee sin máscara (`wave = p[P_E1]`) |

## 2. PHASE · LINE = FOLD

La línea 2 podía mezclarse (MIX), modular en anillo (RING) o modular en fase la línea 1 (PM). Ahora puede
**plegarla**: la suma de las dos líneas se envuelve en los rieles en vez de recortarse (`q > 32767 → 65534 − q`), o
sea un *wavefolder* — la cresta que pasa el techo vuelve como armónicos en vez de como distorsión.

- **Control:** el mismo `LINE` (cuarto valor). `WAVE2` da la forma de la línea 2 y `DTN` su altura; el `DCW` sigue
  doblando la fase de la línea 1. No se agrega ningún knob.
- **Código:** cuatro líneas en el lazo de render más el valor en el descriptor.

## 3. LOFI · WAVE = SYNC

Un pulso cuya fase corre **2 a 5 veces** la de la nota y se reinicia con ella: el *hard sync* de los chips, que este
motor no tenía (tenía pulso, triángulo, sierra, ruido, wave RAM, bytebeat y floatbeat).

- **Control:** `WAVE` (octavo valor) y `DUTY` para la relación (0..127 → 2×, 3×, 4×, 5×). El resto —CHIP, CUT, CRUSH,
  MORPH— funciona como siempre; el modo entra también en el morfado (`lofi_mto`), así que MORPH hacia SYNC lo barre.
- **Código:** un `case` en el lazo y otro en `lofi_mto`.

## 4. Medición `[HOST]`: suenan, y no son un LSB

`tests/engine_options_test.c` (nuevo, en `tests/run_tests.sh`) renderiza la nota mantenida del preset 0 de cada motor
con el control en **todos** sus valores y compara las muestras. La distancia es `rms(a − b) / rms(a)`: cuánto de la
señal cambió, no una impresión.

| PHASE (preset 0) | rms | cruces por muestra | distancia a FOLD |
|---|---|---|---|
| LINE 0 MIX | 6970 | 0,0112 | **119 %** |
| LINE 1 RING | 7169 | 0,0161 | **148 %** |
| LINE 2 PM | 9778 | 0,4905 | **164 %** |
| LINE 3 **FOLD** | 7275 | 0,0176 | — |

| LOFI (preset 0) | rms | cruces por muestra | distancia a SYNC |
|---|---|---|---|
| WAVE 0 PLS | 6892 | 0,0117 | **145 %** |
| WAVE 1 TRI | 4050 | 0,0117 | **117 %** |
| WAVE 2 SAW | 4001 | 0,0176 | **117 %** |
| WAVE 3 NOIS | 6027 | 0,0005 | **136 %** |
| WAVE 4 WRAM | 5168 | 0,0303 | **127 %** |
| WAVE 5 BYTE | 3541 | 0,0054 | **113 %** |
| WAVE 6 FLOAT | 2019 | 0,0205 | **105 %** |
| WAVE 7 **SYNC** | 6559 | **0,3659** | — |

FOLD y RING cruzan por cero casi igual (0,0176 vs 0,0161) —una métrica de brillo sola los confundiría, y por eso el
test mide sobre las muestras—: lo que los separa es el 148 % de señal cambiada. SYNC, además, triplica la tasa de
cruces: es un pulso metálico, no un cambio de nivel.

Y la biblioteca no se movió: **0 presets** de fábrica usan FOLD o SYNC, **306 golden renders, 0 cambiados**,
`engine_param_test` verde (240 knobs × 3 presets, ninguno sin efecto).

## 5. Medición `[AC791N]`: cuánto cuesta en XIP

Cada opción medida sola y las dos juntas, `./build.sh` con el toolchain y el SDK:

| build | imagen | libre en el slot | costo |
|---|---|---|---|
| v1.5.27 (antes) | 582 724 B | 888 B | — |
| sólo PHASE FOLD | 582 820 B | 792 B | **+96 B** |
| sólo LOFI SYNC | 582 788 B | 824 B | **+64 B** |
| **las dos (v1.5.28)** | **582 884 B** | **728 B** | **+160 B** |

Entran con margen y sin tocar `.data+.bss` (86 912 B de 98 304), el pool, la LOWBUF ni el formato del preset. La
suma de las dos medidas por separado da exactamente el total: no hay cascada del compilador en este parche (la hubo
en el de `gfx.c`, ver `docs/FASE6_XIP_NARROW.md`).

## 6. Lo que sigue sin entrar

La caché de ventanas angostas de `gfx.c` (21 440 px por frame quieto frente a 29 952) pide **3 640 B** y ahora hay
**728 B**: sigue detrás de `FELUCCA_WINCACHE`, y el camino con números para liberarlos es el de
`docs/FASE6_XIP_NARROW.md` §5 (decodificar la fuente L al arranque: −4 270 B de XIP por ~5 KB de `.bss`).

## 7. Cómo se reproduce

```
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/engine_options_test tests/engine_options_test.c -lm
./build/engine_options_test                       # [HOST]: sonido, distancias, presets limpios

cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/regress tests/regress.c -lm
./build/regress tests/golden.txt tests/cpu_baseline.txt    # 306 renders, 0 cambiados

JIELI_TOOLCHAIN=… AC79_SDK=… ./build.sh           # [AC791N]: 582 884 B, 728 B libres
```
