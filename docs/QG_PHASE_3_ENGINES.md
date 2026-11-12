# Fase 3/5 — Síntesis y motores: auditoría y un motor nuevo (WTABLE)

Estado: **parcial, integrado y verificado en host**. Este documento dice qué se hizo, cómo se midió, qué
se rechazó y **qué no se hizo y por qué**. No se avanzó a la fase 4.

Todos los números de este informe son de **host x86** (`tests/regress.c`, `tests/preset_audit_test.c`,
`tests/wavetable_test.c`, el mismo código C que enlaza el firmware) salvo donde se diga otra cosa. **No hay
ninguna medida `[TARGET]` ni `[FÍSICO]`**: en este entorno no hay toolchain AC791N (`JIELI_TOOLCHAIN`,
`AC79_SDK`) ni unidad física, así que no hay `.bss` enlazado, ni flash real consumida, ni desensamblado del
bucle de render, ni una sola escucha. Nada de lo que sigue se presenta como validación en placa.

---

## 0. Lo entregado, en una tabla

| Pedido | Estado | Qué hay |
|---|---|---|
| Revisar todos los motores existentes | **hecho (auditoría)** | §1: los 8 motores vivos, sus parámetros reales, sus límites y lo que les falta |
| Mejorar los motores existentes | **no hecho** | §7.1: motivo y lista concreta de candidatos |
| Completar STRING, ADD y MATH | **no aplicable** | §0.1: no existen en este árbol; ver el razonamiento |
| Exactamente un motor nuevo | **hecho: WTABLE** | §2–§4: tabla, morph, HARM, UNI, SUB, filtro, DC blocker, 10 presets |
| Medir CPU / RAM / flash / polifonía antes de adoptarlo | **hecho en lo posible** | §3: tabla de 12 288 B, 0 B de `.bss` nuevos, ns/sample en host, polifonía 12; **sin** medida de target |
| Pantallas y flujo de edición por motor | **parcial** | §6: las dos páginas EDIT del motor, con nombres propios; sin gráfico de onda propio |
| MOD MATRIX, presets y performance | **hecho** | §6: `MD_E1..E8` → `P_E0..P_E7`, 10 presets, cuatro knobs de HOME |
| Pruebas de estabilidad, clipping, aliasing, polifonía, regresiones | **hecho en host** | §4 y §5 |

### 0.1 STRING, ADD y MATH: no existen, y no se inventaron tres motores

El pedido dice «implementá STRING, ADD y MATH **si siguen incompletos**». La revisión del árbol
(`firmware/src/engines.c` `ENGINES[]`, `ENGINE_ORDER`, `core.h`, `tests/descdump`) muestra que **no hay
ningún motor con esos nombres**, ni completo ni incompleto: la tabla de motores es ANALOG (0), PHASE (2),
LOFI (3), VOICE (5), WHEEL (7), DRUM (10), NOISE (11), FM6 (12) y ahora WTABLE (14), con 1, 4, 6, 8, 9 y 13
reservados (DIGITAL, SAMPLE, TRIO, GRAIN, PHYS, SLICE, retirados y nunca reutilizados).

Lo que esos nombres suelen designar ya está cubierto por motores que sí existen, con edición propia y audio
real:

- **aditivo** → **WHEEL** (nueve parciales por voz, 16 registros, percusión, rotor) y las formas de la tabla
  nueva;
- **cuerdas / voces** → **VOICE** (formántico: VOWEL, VOWEL2, TALK, SHIFT, BUZZ, BREATH, Q, RAND) y los
  presets de cuerda de ANALOG y FM6; no hay modelado físico (PHYS se retiró en TONIC);
- **«math»** → **FM6** (32 algoritmos, 6 operadores) es la síntesis por operaciones; no hay un motor aparte.

Crear tres motores nuevos habría contradicho el otro requisito explícito del mismo pedido —«evaluá e
implementá **exactamente un** motor nuevo»— y habría duplicado con nombres nuevos lo que ya suena. La
decisión, por tanto: **se implementó el motor nuevo pedido (WTABLE) y se documenta el mapa de nombres** en
lugar de fabricar tres alias. Si lo que se quiere es un motor de cuerdas por modelado físico, eso es un
proyecto en sí (el viejo PHYS ocupaba 50 KB de estado por voz y se retiró justamente por eso) y necesita su
propia fase con presupuesto.

---

## 1. Inventario real de los motores (medido)

Sacado de `tests/descdump` (las mismas tablas que usa el firmware y el editor), 212 presets en total:

| # | Motor | Páginas | Parámetros (EDIT 1 / EDIT 2) | Presets | Polifonía | Notas |
|---|---|---|---|---|---|---|
| 0 | ANALOG | OSC / FLT | WAVE, DETUNE, MIX, NOISE / CUT, RES, DRIVE, INT | 56 | NVOICE | Dos osciladores, morph, SYNC/RING/SAW3, filtro SVF con tipos (FILTER page) |
| 2 | PHASE | PHS / LINE | WAVE, WAVE2, DCW, ENV / DETUNE, LINE, SUB, FB | 25 | NVOICE | Distorsión de fase con realimentación; WAVE2 alterna cada ciclo |
| 3 | LOFI | CHIP / TONE | CHIP, WAVE, DUTY, CRUSH / CUT, VIB, BEND, MASK | 25 | NVOICE | Reloj de bits, crusher, máscara |
| 5 | VOICE | VOWL / TONE | VOWEL, VOWEL2, TALK, SHIFT / BUZZ, BREATH, Q, RAND | 20 | **4** | Formántico: dos vocales, coro, ruido de aliento |
| 7 | WHEEL | BARS / TONE | REG, SUB, BODY, TOP / PERC, CLICK, DRIVE, ROTOR | 18 | NVOICE | 9 drawbars, 16 registros, rotor con Doppler |
| 10 | DRUM | KIT / KIT 2 | MORPH, TUNE, TONE, DECAY / NOISE, FM ENV, FOLD, DRIVE | 10 | 8 (lanes) | Caja DRUM-X de 8 lanes, one-shot, sin glide |
| 11 | NOISE | SRC / MOVE | MODE, COLOR, FREQ, PEAK / TRK, DENSE, DRIFT, CRUSH | 17 | NVOICE | 4 fuentes, filtro con seguimiento de tecla, reloj |
| 12 | FM6 | OPS / PATCH | ALG, FB, MOD, RATIO / MEG, VMOD, DETUNE, SLOT | 29 | FM6_POLY | 6 operadores, 32 algoritmos, envolventes propias |
| **14** | **WTABLE** | **TABLE / TONE** | **MORPH, DETUNE, UNI, SUB / CUT, RES, DRIVE, HARM** | **10** | NVOICE | **Nuevo en esta fase** (§2) |

Lo que la auditoría deja claro, y que explica por qué «mejorar los motores existentes» no es un cambio
cosmético (§7.1): cada motor ya tiene su juego de herramientas específico (no hay knobs genéricos
repetidos), y tocar cualquiera de ellos cambia el sonido de sus presets, es decir, cientos de renders
dorados y de oídos que aquí no se pueden poner.

---

## 2. WTABLE: qué es y cómo está hecho

`firmware/src/eng_wavetable.c` + la tabla generada por `tools/gen_tables.py`. No es un motor de pantalla:
genera audio desde el primer sample y todos sus parámetros llegan al DSP.

### 2.1 La tabla

- **8 fotogramas × 256 puntos × 3 límites de banda** = 6 144 `int16` (`WT_DATA`, 12 288 B).
- Fotogramas: SINE, TRIANGLE, SAW, SQUARE, PULSE 25 %, HOLLOW (armónicos impares 1/k³), METAL
  (1 3 5 7 9 11 13) y VOCAL (dos formantes, en el 3.º y el 9.º armónico).
- Cada fotograma es una suma de senos del armónico *k* con **fase 0**: todos cruzan por cero al empezar el
  ciclo, así que un morph entre vecinos no salta en el cruce ni introduce DC (DC de tabla: 0,0 medido en el
  generador).
- Cada fotograma se **normaliza al mismo pico** (32 000), así que moverse por la tabla cambia el timbre, no
  el nivel.
- **Tres niveles de banda**: 64 armónicos (notas por debajo de ~344 Hz), 16 (hasta ~1,38 kHz) y 5 (por
  encima). El nivel se elige por bloque según el incremento de fase de la nota: **nunca se lee por encima de
  ~11 kHz**, así que la parte alta del teclado no puede aliasear (§4.4 lo mide).

### 2.2 La señal, parámetro por parámetro

| # | Nombre | Qué hace en el DSP | Modulable |
|---|---|---|---|
| E1 | **MORPH** | posición en la tabla: dos fotogramas vecinos y su fracción (Q8), leídos por bloque e interpolados por muestra | **sí: es `MD_E1`**, el primer destino de motor de la matriz |
| E2 | **DETUNE** | cents (±) de la segunda copia cuando UNI está en 2 VOICES | sí |
| E3 | **UNI** | 1 VOICE / 2 VOICES: la segunda copia desafinada; la suma se divide por dos para conservar el nivel | sí (enumeración) |
| E4 | **SUB** | seno una octava por debajo, mezclado en la voz | sí |
| E5 | **CUT** | corte del SVF de 2 polos, con seguimiento de tecla y `m->cutoff` (ENV / LFO → CUT siguen funcionando) | sí |
| E6 | **RES** | resonancia del mismo SVF | sí |
| E7 | **DRIVE** | saturación suave (tanh) antes del filtro | sí |
| E8 | **HARM** | inclinación espectral continua: un paso bajo de un polo a ~1 kHz y su banda alta sumada o restada (±0,75), sin escalones | sí |

### 2.3 Un defecto encontrado y corregido: el DC del saturador

La primera versión medía **hasta 41,5 % de la escala de una voz en continua** en los presets con SUB y DRIVE
(SUB BASS 15,9 %, BRASS SWELL 6,1 %, QUANTUM MASS 5,2 %; 10 voces suman). Causa: un saturador alimentado por
un seno y su octava no es simétrico en medio ciclo, y el recorte asimétrico deja continua.

Corrección: un **bloqueador de continua a ~5 Hz** al final de la voz (un polo, 3 operaciones por muestra,
inaudible), con el producto **redondeado**: con el `>>15` truncado el propio bucle se sesgaba ±1 LSB
acumulado (≈685 por voz, medido en la primera prueba: −41 % en los diez presets). Con redondeo: **0,00 %**.

### 2.4 Estado y coste de memoria

- **Flash**: 12 288 B de tabla `const` (XIP, se lee directamente; no se copia a RAM).
- **RAM nueva**: **0 B**. El estado por voz vive en `voice_t`: `ph[0..2]` (principal, segunda copia, sub) y
  `s[0..4]` (dos estados del SVF, el paso bajo de HARM, los dos del bloqueador de continua), con
  `keep = 0x1f` para que un retrigger no los borre. No hay arrays nuevos indexados por parte ni por voz.
- **Polifonía**: el motor no limita (`poly = 0` → `NVOICE` = 12); con UNI en 2 VOICES cada voz lee la tabla
  dos veces por muestra.
- **Coste por muestra**: 2 lecturas interpoladas por fotograma (4 cargas), más el sub, el tilt, el SVF, el
  saturador y el bloqueador.

---

## 3. Medidas (con su clase)

| Medida | Valor | Clase | Cómo |
|---|---|---|---|
| Tabla en flash | 12 288 B | **compilado** (tamaño del objeto `const` en el árbol) | `WT_DATA[6144]`, `int16_t` |
| `.bss` nuevo | 0 B | **leído del código**, no enlazado | el estado cabe en `voice_t`; no hay estáticos nuevos |
| CPU del motor, aislado | **11–12 ns** por muestra y voz | **host x86**, reloj de pared, esta VM | `tests/wavetable_test.c`, 8 notas, restando el ocio |
| CPU por preset (ruta completa de voz) | **62,7–116,6 ns** por muestra | **host x86**, misma tanda | `tests/regress.c` (GLASS PAD 74,0 · MORPH LEAD 84,6 · SINE KEY 62,7 · VOCAL AAH 87,2 · METAL BELL 66,9 · HOLLOW WOOD 77,2 · BRASS SWELL 116,6 · SUB BASS 77,1 · PULSE ORGAN 91,8 · QUANTUM MASS 88,3) |
| Instrucciones por muestra | **no medidas** | — | este host no expone contadores; `tests/target_budget.py` y el desensamblado necesitan toolchain AC791N |
| Nivel en el peor caso | **1,94 × `VOICE_FS`** por voz | **host x86** | todos los EDIT al máximo, 8 notas; ANALOG mide 2,07 y PHASE 1,47 en la misma prueba |
| Nivel por defecto | **0,19 × `VOICE_FS`** por voz | **host x86** | ANALOG 0,14 · PHASE 0,20 |

**Advertencia sobre los ns**: esta VM es compartida y en la misma tanda los presets de ANALOG midieron
260–410 ns contra su línea base registrada (mediana del host 1,03, o sea que el host entero estaba más lento
que cuando se grabó la base). Los números de arriba sirven para comparar entre motores **dentro de la misma
tanda**, no como presupuesto. El presupuesto real de la fase (725 µs por bloque en el AC791N) **no se puede
medir aquí**.

---

## 4. Pruebas nuevas (`tests/wavetable_test.c`)

Se ejecuta como el resto (host, `hostsim.c`), y todas sus comprobaciones pasan:

1. **Los fotogramas y el morph**: MORPH 0 es el seno (2.º armónico 35 dB por debajo del 1.º); 18 está cerca
   del triángulo (2.º < −30 dB, 3.º a −18,8 dB); 37 es la sierra (2.º a 0,44 del 1.º, 3.º a 0,35); 127 es el
   fotograma vocal (3.º y 9.º por encima de sus vecinos); a 45 el 2.º armónico queda entre la sierra y la
   cuadrada (un morph es un crossfade, no un salto).
2. **HARM**: la energía de 2–8 kHz sobre el fundamental sube al menos 6 dB de HARM 0 a 127, y 64 queda en
   medio.
3. **SUB y UNI**: con SUB 90 la octava de abajo es la parcial fuerte bajo la nota; con dos voces a 20 cents
   el nivel se mueve más de 3 % cada 10 ms (batido).
4. **Aliasing**: en C7, A7 y C8 el espectro no tiene **ninguna** bin fuerte que no sea un armónico, y lo mismo
   en cada octava de C1 a C8 (pico a pico, tolerancia de una bin: una nota baja tiene armónicos muy juntos).
5. **Salud**: los 10 presets con 10 notas a velocidad 127 → 0 clipping, 0 continua (media sobre 2 s en
   C4..F5), 0 voces sonando tras el release; los ocho parámetros al máximo → ≤ 2,2 × `VOICE_FS` por voz;
   una nota desde el silencio da exactamente los mismos samples dos veces.
6. **Coste**: impreso y comprobado contra un tope de host.
7. **Integración**: el motor está en la lista (`eng_ok`, alcanzable con el knob de PRESETS) y su primer
   parámetro EDIT es MORPH (o sea, `MD_E1`).

---

## 5. Regresiones (lo que no se movió)

- **Renders dorados**: `tests/golden.txt` pasa de 220 a **230 renders: 0 cambiados, 10 nuevos, 0
  desaparecidos**. Es decir: el motor nuevo **no toca el sonido de ningún preset existente**.
- **Auditoría de presets**: **212 presets, 0 silenciosos, 0 pasados de nivel, 0 nombres repetidos, 0
  duplicados exactos, 0 fuera de rango**. WTABLE aporta 10.
- **Suite completa**: `NO_PACKAGE=1 tests/run_tests.sh` → **`ALL HOST TESTS PASSED`** (121 secciones).
- **Barrido de UI**: al principio el motor se llamaba `WAVETABLE` y el campo del navegador lo **cortaba con
  puntos suspensivos** (un hallazgo real del lint de maquetación, «ellipsised (not free text)»). Se acortó a
  **`WTABLE`**, que cabe en los seis caracteres del campo, y el barrido vuelve a **0 hallazgos** en 191
  pantallas × 22 paletas (y 201 con `FELUCCA_FM4=1`), con 3 163 051 y 3 583 944 medidas de alineación
  respectivamente y **0 desviaciones de 1 px o más**.
- **UI**: la prueba del orden de motores fija ahora `ANALOG FM6 PHASE LOFI VOICE WHEEL NOISE WTABLE DRUM` —
  el motor nuevo entra **antes de DRUM**, que sigue siendo el último.
- **Editor web y Web Studio**: `web/editor.html` gana el motor 14 y los 10 presets (mismas tablas que el
  firmware), `web/test_web.mjs` actualiza sus cuentas (15 motores, `WTABLE` en la lista de `G_ENGSEL`), y
  `N_ENGNAME` de `params.c` gana la entrada 14.

---

## 6. Integración: pantallas, matriz, presets y performance

- **Pantallas**: TABLE (MORPH, DETUNE, UNI, SUB) y TONE (CUT, RES, DRIVE, HARM), con los nombres, los rangos
  y los valores por defecto del motor. Se llega desde el navegador de motores (knob de PRESETS / EDIT) como a
  cualquier otro; el motor aparece con su nombre en el campo de seis caracteres.
- **MOD MATRIX**: `MD_E1..MD_E8` se mapean a `P_E0..P_E7`, así que los ocho parámetros son destinos; el
  modulador escribe en `t->p[]` durante el bloque y el motor los lee de ahí. **MORPH es `MD_E1`**, el destino
  que la matriz ofrece como «E1» (mover la posición de la tabla con un LFO o una envolvente es configurarlo,
  no un modo aparte).
- **Presets**: 10 (`GLASS PAD`, `MORPH LEAD`, `SINE KEY`, `VOCAL AAH`, `METAL BELL`, `HOLLOW WOOD`,
  `BRASS SWELL`, `SUB BASS`, `PULSE ORGAN`, `QUANTUM MASS`), cada uno con su envolvente, envíos de FX y
  patrón sugerido (`PAT`). Se cargan desde el navegador, el editor y el librero como los demás.
- **Performance**: los cuatro knobs de HOME del motor son MORPH, HARM, CUT y UNI.

---

## 7. Deuda declarada (lo que NO se hizo)

### 7.1 Mejoras a los motores existentes

Se auditó cada motor (§1) y **no se cambió ninguno**. Motivo: cualquier cambio en un motor existente cambia
el sonido de sus presets (ANALOG 56, FM6 29, PHASE 25, LOFI 25, VOICE 20, WHEEL 18, NOISE 17, DRUM 10), es
decir, reescribe cientos de renders dorados y de decisiones de sonido que aquí no se pueden escuchar. En la
fase 1 se demostró que el método que funciona es: medir, proponer, medir el candidato y **rechazarlo si no
paga** (así se descartó el hoist de `master_out`, +1,4 %). Aplicar ese método a ocho motores es trabajo de
una fase propia, no de un añadido al final de ésta. Candidatos concretos, por motor, para esa fase:

- **ANALOG**: una tercera fuente (sub) y un modo de sync con cruce suave; el filtro ya tiene tipos pero el
  DRIVE no compensa nivel por tipo.
- **PHASE**: el DCW no tiene destino de modulación propio más allá de ENV.
- **LOFI**: el reloj de bits es global por parte, no por voz (T.CLK).
- **VOICE**: la polifonía está limitada a 4 por motor; el TALK no acepta velocidad.
- **WHEEL**: la percusión es de disparo único y no hay second-touch.
- **FM6**: la banca de voces y el SLOT ya están; falta un editor de operadores en la pantalla (hoy son los
  parámetros ALG/FB/MOD/RATIO/MEG/VMOD/DETUNE/SLOT).
- **NOISE**: el DRIFT es por parte, no por voz.
- **DRUM**: 8 lanes fijas, sin capa por lane.

### 7.2 Pantalla propia del motor nuevo

WTABLE usa las páginas EDIT genéricas (con sus nombres y valores reales), pero **no dibuja la onda** de la
posición actual. Sería el siguiente añadido natural (`ui_draw.c` no tiene hoy gancho por motor; FM6 tampoco
dibuja sus algoritmos en la pantalla del instrumento, sólo el editor web).

### 7.3 Pendientes de la fase 2 (siguen abiertos)

- **FX ROUTING** (vista del recorrido parte → insert → envíos → bus → master).
- **SEQUENCER TOOLS**.
- Personalización de pieles más allá de paleta + rampa térmica.

### 7.4 Lo que no se pudo medir aquí

- `.bss` y flash enlazados, y el bucle de render en el desensamblado: **sin toolchain AC791N**.
- Instrucciones por muestra: **sin contadores** en este host.
- Escucha, estabilidad en placa, latencia real: **sin unidad física**. El motor se verificó con medidas de
  host (espectro, nivel, continua, aliasing, determinismo), no con oído.
