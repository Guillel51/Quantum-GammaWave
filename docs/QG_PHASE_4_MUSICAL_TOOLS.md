# Quantum GammaWave — Fase 4/5: FX, ruteo, secuenciador y herramientas musicales

Qué se revisó, qué se arregló, qué se agregó y qué quedó pendiente con su razón. Cada número dice de dónde sale:
**compilado** (lo verifica el compilador), **medido en host x86** (una prueba del árbol en este VM), o
**no medible aquí** (toolchain AC791N o unidad física ausentes). Nada de lo que sigue se midió en la placa.

---

## 0. Resumen

| Área | Estado al empezar | Qué se hizo |
|---|---|---|
| §1 FX y medidores | funcionando, con dos defectos de presentación | medidores unificados en **una** balística (`fx_meter_step`); el medidor duplicado de la página FX, borrado |
| §2 efectos nuevos | 8 buses vivos | **FLANGER** como tercer `CHORUS > TYPE` (0 B de RAM, 0 B de flash, 20 líneas de DSP) + tabla de veredicto de los 15 restantes |
| §3 ruteo | sin vista del recorrido | novena pantalla de HOME, **ROUTING**, con los medidores reales de cada etapa + prueba del ruteo |
| §4 secuenciador / performance | ya muy completo (herencia JIANT / SLOOP) | auditoría con evidencia, carencias nombradas y plan |
| §5 herramientas musicales / MIDI | escalas, acordes, voicing, intervalos, progresiones, KEYS ya existentes | encuesta de JIANT / X0X / Optimist / Melodee / Jangada con licencia, lo adaptado y lo descartado |

Dos archivos de firmware tocados para §1–§3: `firmware/src/fx.c`, `firmware/src/ui_homeviews.c`,
`firmware/src/ui_graph.c`, `firmware/src/ui.c`, `firmware/src/ui_draw.c`, `firmware/src/params.c`,
`firmware/src/project.c`.

---

## 1. Reparación de FX (§1)

### 1.1 Lo que ya estaba medido y sigue en pie

Estas cifras son de `tests/fx_probe.c` (**medido en host x86**, la DSP real del árbol compilada como host, no un
modelo) y se releyeron en esta fase; ninguna cambió:

| Efecto | Medida | Valor |
|---|---|---|
| PHASER | profundidad del peine barriendo el tono, 16 posiciones del LFO | **≥ 20 dB** de cresta a valle en 200 Hz..6 kHz |
| PHASER | salto de `DEPTH` en marcha | sin click (paso máximo de muestra bajo el umbral de la prueba) |
| M-WIDE | correlación L/R a 0 / 64 / 127 | **1,000** (mono) / **−0,003** (sin tocar) / **−0,594** (más ancho) |
| M-WIDE | nivel 127 contra 64 | **−0,0 dB**: más ancho no es más fuerte, no empuja el limitador |
| M-WIDE | fuente mono a 127 | sigue mono (`r = 1,000`), −3,9 dB |
| LOWCUT / BASS+ | 1 kHz | **−0,2 / −0,7 dB**: el filtro no se come el medio |
| LOWCUT / BASS+ | 60 Hz | **−12,8 / −5,1 dB**: lo que se va es el grave, y no hacia el silencio |
| EQ LOW SH −24 | 30 / 60 / 250 / 4000 Hz | −24,0 / −23,8 / −13,6 / −0,0 dB: monótono y sin sobrepasar 0 |
| EQ PEAK +12 | máximo de la curva | **+11,7 dB a 665 Hz**, 0 dB en los extremos |

Es decir: los cuatro defectos que el pedido nombraba como sospecha (pops, filtros que cortan audio, parámetros
sin efecto audible, medidores que no reaccionan) tienen, para Phaser / M-WIDE / EQ / LOWCUT, una medida que los
descarta o los acota. Delay y reverb tienen sus propias pruebas (`tests/dly_echo_test.c`, `tests/reverb_test.c`,
`tests/slicer_test.c`) y Punch-in las suyas (`tests/perform_test.c`).

### 1.2 El defecto real: dos sistemas de medición para el mismo dato

Los medidores de la página FX venían leyendo **los mismos** picos que HOME (`fx_vu[0..5]` para los buses,
`fx_vu[6..7]` para el master, `trk[].vu` para la parte) pero los escalaban con **su propia** balística —un
decremento de 1 px por frame escrita a mano en `ui_graph.c`— mientras `fx_meter_step()` (`core.h`), el helper
que la prueba `tests/fx_meter_test.c` fija con números (ataque inmediato, un cuarto de la caída por frame,
30 frames de retención), **no lo usaba ninguna pantalla**: sólo su prueba.

Corregido: la página FX llama a `fx_meter_step()` como cualquier otra barra del instrumento. Queda **una** ley
de balística en el firmware y **un** origen de datos por medidor:

| Pantalla | Qué dibuja | De dónde lo lee |
|---|---|---|
| HOME > MIXER | `L` / `R` del master | `fx_vu[6..7]` (**medido en host x86**, la misma envolvente que el resto) |
| HOME > OVERVIEW | el pico de cada parte | `trk[].peak` |
| HOME > ROUTING (nueva) | entrada y retorno de cada bus, y el master | `fx_vu[0..2]`, `fx_vu[3..5]`, `fx_vu[6..7]` |
| FX (página de la sección) | los cuatro canales | `trk[].vu`, `fx_vu[0..5]`, con `fx_meter_step()` |
| WAVES / TECH | master y retorno de reverb | `fx_vu[6..7]`, `fx_vu[5]` |

No quedó ningún medidor con su propia ley ni ninguna copia del dato: la duplicación que el pedido pedía borrar
era ésa, la de la ley, y está borrada.

---

## 2. Efectos nuevos (§2): evaluación y lo implementado

### 2.1 Criterio

Se eligió **por costo medido contra valor musical**, no por completar la lista. Lo que manda en el FM-1:
`cho_buf` (2 048 muestras), `dly_buf` (`DLY_LEN`), la reverb (~4 937 muestras) y el bloque de 32 muestras en
725 µs. Un efecto nuevo paga RAM (`.pool`), flash (código) y ns por muestra en el bus más cargado.

### 2.2 Implementado: FLANGER (tercer `CHORUS > TYPE`)

`CHORUS > TYPE` ya tenía dos modos (el ensemble de tres taps y el PHASER). El flanger es el tercero y usa **la
misma línea** del chorus con un retardo corto barrido y **realimentación 0,6** dentro de la línea:

- retardo **48..302 muestras** (1,1..6,8 ms a 44,1 kHz) según `DEPTH`, barrido por el LFO del chorus;
- notches en múltiplos impares de `1/(2d)`, que es lo que hace el «jet»;
- **0 B de RAM nueva** (usa `cho_buf`), **0 B de flash nuevo** (usa `cho_tap` y `osc_sine`), **dos productos por
  muestra**: el del tap y el de la realimentación. Es **más liviano que el ensemble**, que hace tres taps.

Medido en host x86 (`tests/fx_probe.c`):

| Medida | Valor |
|---|---|
| profundidad del peine (8 posiciones del LFO, 150 Hz..6 kHz) | **12,0 dB** |
| estabilidad con realimentación 0,6 | pico **7 033** (Q15), sin fuga |
| los notches se mueven con el barrido | null distinto entre LFO 0 y LFO 7, **≥ 20 %** |
| costo contra el ensemble (`fx_bench`, misma corrida, 6 partes, envíos iguales) | ensemble (esc. 1) **126,2 ns/muestra** · flanger (esc. 3) **119,1 ns/muestra** |

Persistencia: el `TYPE` viaja en el bloque `DT` del proyecto en **dos bits** (antes uno). Un proyecto guardado
antes de esta fase se lee igual (bit 1 solo ⇒ 0 ensemble / 1 phaser); un firmware anterior que lea un proyecto
con FLANGER lo ve como PHASER, no como silencio. Está dicho en el comentario del código y en la prueba de
proyecto. La pantalla `CHORUS` muestra `FLANGER` sin tocar el layout (el `F_ENUM` pasó de 2 a 3 valores y el
editor web tiene la lista nueva).

### 2.3 Los otros quince, con veredicto

| Efecto | Veredicto | Razón medida / de recursos |
|---|---|---|
| **Compressor** | **ya está** | el master tiene limitador con envolvente (`lim_env`, `master.c`) y `G_DUCK` hace ducking por bombo; un compresor de bus sería un tercero |
| **Limiter** | **ya está** | `master_out` + `G_CLIP` (clipper) + limitador; medido en `tests/master_test.c` |
| **Tremolo** | **ya está** | LFO de la matriz con destino AMP (`P_LD_AMP`, `MD_*`): es el mismo efecto, con forma y sync |
| **Vibrato** | **ya está** | LFO con destino PITCH (`P_LD_PIT`) |
| **Ring Modulator** | **no se hizo** | no hay anillo dedicado. Costaría poco, pero su valor musical es menor que el del flanger y el pedido pedía **elegir**, no sumar los dieciséis |
| **Bitcrusher** | **parcial** | `DIST > TYPE` = `FLUX` ya cuantiza (enmascara bits según el drive) y el motor LOFI baja resolución por voz; un crusher de bus pediría un cuantizador más por muestra en el bus más cargado |
| **Sample Rate Reducer** | **descartado** | pide otra línea (o un contador de retención) por bus; `LOAD ECO` y LOFI ya degradan a propósito |
| **Tape Saturation** | **ya está** | `DIST > TYPE` = `TAPE` (polinomio cúbico) |
| **Overdrive** | **ya está** | `DIST > TYPE` = `SOFT` (clip asimétrico, armónicos pares) y `HEARTH` (válvula + chispas de agudos + peso de sub) |
| **Fuzz** | **casi** | `HEARTH` y `FLUX` cubren el rango áspero; el enum tuvo también `RECT` (rectificación de onda completa, una octava arriba) — el comentario de `fx.c` lo recuerda, el enum vivo tiene cuatro tipos |
| **Wavefolder** | **no está** | el `FOLD` que el comentario de `fx.c` recuerda como parte de una versión anterior ya no existe en el enum. Agregarlo es barato (una no linealidad por muestra), pero suma un quinto `TYPE` y eso toca presets guardados: queda como deuda, no se hizo |
| **Frequency Shifter** | **descartado** | necesita una red de Hilbert (o un SSB con dos líneas y multiplicadores en cuadratura): ~2 líneas de 2 KB y 4 productos por muestra. No entra en el presupuesto de un bloque de 32 muestras sin medirlo en la placa, y no lo puedo medir aquí |
| **Reverse Delay** | **descartado** | pide una segunda línea del tamaño del delay (≈ 32 KB) o lectura inversa del `dly_buf`, que rompe el `GRAIN`/`PING-PONG` actual |
| **Shimmer Reverb** | **ya está** | `G_RTYPE` = `SHIMR` (con `AIR` en semitonos) |
| **Chorus** | **ya está** | el bus, más el flanger nuevo como tercer `TYPE` |

Resultado: de dieciséis, **once ya existían** (algunos con otro nombre), **uno se agregó** (flanger), **uno se
descartó por memoria** (reverse delay, frequency shifter) y **tres por redundancia medida** (bitcrusher, SRR,
fuzz/wavefolder). No se implementó ninguna lista completa sin medir su costo.

---

## 3. Ruteo (§3)

### 3.1 La cadena, tal como el DSP la ejecuta

```
PARTE (motor, NVOICE voces)
  → INSERT   track_dist(t)          P_DIST + P_DTYPE/P_DTONE   — sólo la parte seleccionada
  → SENDS    P_CHOR / P_DLY / P_REV (0..127, escalados ×258)
  → BUSES    fx_buses(): CHORUS (cho_buf) · DELAY (dly_buf) · REVERB (combs)  → fx_vu[0..2] medidos a la entrada,
              fx_vu[3..5] a la salida
  → MASTER   EQ (G_EQMODE/FREQ/GAIN/Q) · M-WIDTH · ducker · clipper · limitador → fx_vu[6..7]
  → OUT L/R
```

Hay además un camino alternativo real: `slicer_routes()` manda la parte al bus del **SLICER** en vez de al mix
seco (`G_SLMODE`, `G_SLT1..6`, `G_SLFX`).

### 3.2 La novena pantalla de HOME: ROUTING

`HV_ROUT`, al final de la lista de HOME (el pie ahora cuenta `/9` en vez de llevar `/8` escrito a mano). Dibuja
el recorrido de arriba abajo, en dos pasadas de 88 filas como el resto de HOME, y **cada cifra es la que el DSP
lee**:

- **PARTE**: el motor y las voces que tiene sonando ahora (`hv_voices`), como OVERVIEW;
- **INSERT**: `P_DIST` en % y su `TYPE`, o `BYPASS` cuando es 0 (que es lo que `track_dist` hace);
- **SENDS**: las tres barras con sus % reales;
- **BUSES**: dos barras por bus, **lo que se le envió** (`fx_vu[0..2]`) y **lo que devolvió** (`fx_vu[3..5]`), y a
  la derecha el modelo/tiempo de ese bus (`FLANGER`, `1/8`, `ROOM`…);
- **MASTER**: modo y ganancia del EQ, `W` (ancho M/S), `LIM`;
- **OUT**: los dos picos del master (`fx_vu[6..7]`), los mismos del MIXER.

Una barra oscura significa **que no pasó audio por ahí**, no que la pantalla lo esconda: es la misma distinción
que pedía el punto 3 del pedido, y hay una prueba que lo fija (abajo).

### 3.3 Que el audio realmente atraviesa ese ruteo

`tests/fx_probe.c` (medido en host x86) alimenta **sólo el bus de chorus** con un tono de 440 Hz durante 40
bloques y llama a `fx_buses()`, la función que corre el ISR:

```
in 11999 / 0 / 0 ; out 9060 / 0 / 0        (picos Q15)
```

- el envío alimentado levanta **su** medidor de entrada y ningún otro;
- ese bus devuelve audio;
- un bus al que nadie envía **no devuelve nada** (no hay fuga entre buses).

O sea: la pantalla ROUTING no es un diagrama, es la lectura de las mismas cifras que produce el ruteo.

---

## 4. Secuenciador y performance (§4): auditoría

Casi todo lo que el pedido enumera **ya existe** (el linaje JIANT/SLOOP lo trajo). Lo que faltaba era una
auditoría con evidencia; esto es lo que hay, dónde y con qué prueba, más lo que falta.

| Pedido | Estado | Dónde | Evidencia |
|---|---|---|---|
| Probabilidad por paso | **existe** | `seq.c` `step_chance`, `P_TCHAN` (CHANCE, 0 = 100 %), página `CHANCE` fuera del ciclo | `tests/ratchet_test.c`, `tests/perform_test.c` |
| Condiciones por paso | **existe** | `WHEN A/B` + `LEN` (página `WHEN`, `comp_slot`) | `tests/ui_test.c` (páginas COMP / COMP 2 / WHEN) |
| Ratchets | **existe** | `SF_RATCH`, `step_ratchet`, teclas sobre/sous la octava | `tests/ratchet_test.c` |
| Fills / variaciones | **parcial** | `MUTATE` (`P_MSND/P_MPAT/P_MAMT/P_MMODE`, `mutate_poll` en el compás) muta el patrón; no hay «fill» de un compás a demanda | `tests/project_test.c` (mutación), `tests/ui_test.c` (página MUTATE) |
| Parameter locks / automatización | **existe** | `AUTOMATION` (grabación de movimiento) y `AUTO LIST` (los locks y eventos, editables) | `tests/ui_test.c`, `tests/motion_test.c` |
| Escenas | **existe** | `song_chain.c`: cada fila de la canción lleva mutes, grupo de bombo, punch-in y `scene_pfx` | `tests/backup_test.c` (la escena de una canción va y vuelve en el índice), `tests/motion_test.c` |
| Macros | **existe** | matriz de modulación con macros `CLIP` / `PUNCH` (`mod.c`), página `MACRO MAP` | `tests/mod_test.c` |
| Performance / Punch-in | **existe** | `perform.c`: latches, slicer, punch-in MIDI por escena, black keys | `tests/perform_test.c` |
| Persistencia de patrones | **existe** | `project.c` (proyecto + 8 slots de usuario), `dx_snd`/`dx_lay` por pista | `tests/project_test.c`, `tests/fuzz_proj.c` |

Carencias reales, con su costo:

1. **Fills a demanda**: `MUTATE` muta solo, pero no hay «tocá este botón y el próximo compás varía una vez». Sería
   un flag por pista + una copia del patrón (64 pasos × 8 B = 512 B) o una semilla por compás en `mutate_poll`.
2. **Escenas sin nombre ni recall directo**: `song_chain.c` guarda la escena por fila, pero desde SEQ no hay un
   «recall escena n». Sería UI pura sobre datos que ya están.
3. **Repeat/one-shot de punch-in desde el teclado**: hoy `perform.c` tiene REPEAT 1/8, 1/16, 1/32, LPF, HPF; los
   punch-in MIDI por escena existen pero no se disparan a mano desde las teclas negras en todos los modos.

Ninguna de las tres se tocó en esta fase: las tres cambian el mapeo de teclas o el formato del proyecto, y el
pedido dice revisar bancos, asignaciones y latch mutes **antes** de modificarlos. Quedan planteadas, no hechas.

---

## 5. Herramientas musicales y MIDI (§5)

### 5.1 Lo que el instrumento ya tiene

En `FAM_SCL` y `FAM_SEQ`, todo conectado a la secuencia real:

| Página | Qué hace |
|---|---|
| `SCL` / `SCALE` | `ROOT`, `SCALE` (`N_SCALE`: CHR MAJ MIN DOR MIX PEN MPEN HARM…), `QUANT`, `TRANS` por pista |
| `CHORD` | `P_CHRD` (el acorde que dispara una tecla), `P_CVOI` (inversión/voicing), `P_CSTR` (strum) |
| `VOICING` | cómo se reparte el acorde (la misma armonía, otra disposición) |
| `INTERVAL` | intervalos hacia arriba y hacia abajo dentro de la escala |
| `PROG` | progresión: la escala y el acorde por grado |
| `KEYS` | el explorador de notas: qué tecla da qué nota con la escala y la cuantización puestas |
| `SCL 2` | `G_STRN`: toda secuencia arriba/abajo por la escala |

Y MIDI: `tests/midi_control_test.c` y `tests/midi_uart_test.c` (notas, CC, clock, MIDI LEARN/MAP según el
linaje JIANT), más `docs/` para el mapa.

### 5.2 Encuesta de los proyectos pedidos

Todos son **GPL-3.0** (o GPL-3.0-only), la misma familia que este firmware, así que las ideas se pueden tomar
citando origen y las partes de código se pueden portar conservando el aviso.

| Proyecto | Qué es | Idea útil para acá | Estado |
|---|---|---|---|
| **JIANT** (juanjiant-bit, GPL-3.0) | firmware completo del FM-1: 6 pistas, 11 motores, secuenciador de 64 pasos, `EUCLID`, ARRANGE, generador de canciones por género | el generador y `EUCLID` | **ya integrado** en gran parte: `gen.c`, `gen_reel.c`, `fugue.c`, `song_chain.c` citan SLOOP/JIANT por archivo |
| **X0X** (Charles Vestal, GPL-3.0) | máquina de ritmos 909: cada parte con su largo y su rate, cambios de patrón al final del compás | largo/rate por parte y cambio al compás | **parcial**: `P_SDIV`/`P_SLEN` por pista existen; el cambio «al final del compás» es lo que ya hace `song_chain` por fila |
| **Optimist** (w0ts, GPL-3.0) | plataforma modular: elegís motores y FX al compilar según el flash | selección por presupuesto | **no aplicable**: acá el firmware es uno solo; sí vale como criterio de la §2 (medir antes de sumar) |
| **Melodee** (keremimo, GPL-3.0) | 4 pistas, 64 pasos, 16 escalas occidentales, `AUTO` acordes (3–4 notas dentro de la escala), vista de acorde en círculo de quintas | `AUTO` acordes y la vista de círculo | **parcial**: `CHORD`/`VOICING` cubren el acorde dentro de la escala; falta el `AUTO` que elige la cantidad de notas solo |
| **Jangada** (zednaked, GPL-3.0) | sampler/grano con `GRAIN`, motores sampleados, ajustes del segundo núcleo | reparto de trabajo entre núcleos | **ya está** el segundo núcleo (`firmware/src/fx.c` `fx_run0`, `mc_join`) |

Lo descartado y por qué: los **70 scales** de Melodee (el campo de `N_SCALE` y las páginas están dimensionados
para 8; ampliarlo toca presets guardados), el **círculo de quintas** como pantalla nueva (hay `KEYS` y `PROG`,
que dan la misma información sin una pantalla más) y el **sampler** de Jangada (pide RAM de muestras, que el
FM-1 no tiene).

---

## 6. Verificación

| Qué | Cómo | Resultado |
|---|---|---|
| FX y ruteo | `tests/fx_probe.c` | PHASER ≥ 20 dB; FLANGER 12,0 dB con realimentación estable; M-WIDE mono/ancho/level; LOWCUT deja 1 kHz; EQ shelves y peak; **ruteo: in 11999/0/0 → out 9060/0/0** |
| Costo del flanger | `tests/fx_bench.c`, escena 1 (ensemble) contra escena 3 (flanger), dos corridas de 5 repeticiones cada una | **126,2 / 122,6 / 119,1 ns** por muestra (escena 1 / seca / escena 3): el flanger es **~7 ns más barato** que el ensemble, como corresponde a un tap en vez de tres. Es una medida de host x86 en un VM compartido: vale como comparación dentro de la misma corrida |
| Pantalla nueva | `tests/ui_render.c` (192 pantallas × 22 paletas, más el barrido de páginas y valores) | **0 hallazgos de lint**, 0 tintas sobre su propio color, **0 desviaciones en 3 164 009 medidas de alineación** |
| Recorrido de HOME | `tests/ui_test.c` | las nueve pantallas se recorren con un toque cada una y ROUTING no toca página, parámetro ni sonido |
| Tipo de chorus en el proyecto | `tests/project_test.c`, `tests/fuzz_proj.c` | ida y vuelta con `TYPE` de dos bits; los proyectos viejos se leen igual |
| Suite completa | `NO_PACKAGE=1 ./tests/run_tests.sh` | **`ALL HOST TESTS PASSED`** (§6.1) |
| Renders dorados | `tests/regress tests/golden.txt` | **230 renders, 0 cambiados, 0 perdidos** |
| Presets | auditoría de la suite | **202 presets, 0 silenciosos, 0 duplicados** |

### 6.1 Suite completa

Una corrida entera con `NO_PACKAGE=1 ./tests/run_tests.sh` sobre el árbol de esta fase:

```
ALL HOST TESTS PASSED
```

Con los números que importan para lo que se tocó: **192 pantallas × 22 paletas, 0 hallazgos de lint**,
**3 164 009 medidas de alineación, 0 desviaciones**; **230 renders dorados, 0 cambiados** (ningún sonido
anterior se movió: el flanger sólo existe cuando se elige `TYPE = FLANGER`); **202 presets, 0 silenciosos, 0
duplicados**; FX medidos (`fx probe passed`, con el peine del flanger y el ruteo nuevos); 0 fallos de salud,
de voces y de ruteo. La fase 3 cerró con 191 pantallas; la 192 es ROUTING.

Lo que la corrida **no** cubre: el emulador web (se saltea: no hay `emcc` en este VM) y el manifiesto de
compilación (`NO_PACKAGE=1`).

---

## 7. Límites de estas medidas (leer antes de citar cualquier número)

- **Todo lo de arriba es host x86** (clang/gcc sobre la DSP real del árbol). **No hay placa**: no hay toolchain
  AC791N en este entorno (sin `.bss` enlazado, sin flash real, sin desensamblado del bucle), no hay unidad
  física y no hay una sola escucha.
- Los ns por muestra salen de un VM compartido: sirven para comparar escenas de **la misma corrida**, no contra
  `tests/cpu_ns_baseline.txt`.
- Los contadores de instrucciones no están disponibles (el `regress` de este entorno informa `0 instr`).
- `tests/target_budget.py` y `tests/memory_map_test.py` se saltan sin toolchain: el `.bss` y el flash **no**
  están medidos en esta fase.
- **Encontrado y no arreglado** (de otra fase, no de ésta): `tests/mod_test.c` comprueba «DIST TYPE: SOFT HARD
  FOLD CRUSH RECT», cinco tipos que el firmware ya **no** tiene —el enum vivo es SOFT / TAPE / HEARTH / FLUX,
  cuatro—. El caso `k = 4` cae en el `default` (SOFT) y la prueba pasa igual, así que ese caso no prueba nada.
  Queda anotado para su propia pasada: no se tocó acá porque exige decidir si vuelven FOLD y CRUSH (que es lo
  que pide la §8.4) y rehacer sus hashes.

## 8. Deuda que queda, en orden de valor

1. **Fills a demanda** y **recall de escena desde SEQ** (§4): datos que ya existen, UI y un flag.
2. **`AUTO` acordes de Melodee** (§5.2): elegir 3 o 4 notas dentro de la escala automáticamente.
3. **Frequency shifter / reverse delay** (§2.3): sólo con una medición de `.bss` real en la placa, porque las dos
   piden memoria que acá no se puede verificar.
4. **`DIST > TYPE` = `FOLD`** (wavefolder): un quinto tipo, barato en DSP y caro en compatibilidad: toca presets
   guardados, así que va con migración escrita.
5. **Un `TYPE` de delay a la vez**: hoy `G_DTYPE` y `G_CTYPE` comparten byte en el proyecto; si en el futuro hace
   falta un cuarto tipo de chorus, el byte tiene lugar (bits 3..7 libres), pero conviene documentarlo en
   `project.c` antes de usarlo.
