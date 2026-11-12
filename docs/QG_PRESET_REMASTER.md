# Remasterización integral de la biblioteca de presets (6.0)

Informe de la fase de presets: qué había, qué se cambió, qué se añadió, con qué se midió y qué queda
pendiente. Todas las cifras de este documento salen de ejecutar las herramientas que se nombran; la clase
de cada medición está declarada (host x86 con el motor real, nunca una unidad física).

## 1. Punto de partida (medido, no estimado)

Antes de tocar nada se midió la biblioteca con las dos herramientas que ya existían más una nueva:

- `tests/preset_audit_test` — carga y renderiza **cada** preset con el motor real (host x86, 44 100 Hz):
  estructura, silencio, nivel, duplicados exactos y rango de parámetros.
- `tests/preset_levels --csv` — RMS y pico por preset (`tools/gen_preset_levels.py` los convierte en el
  nivel con el que cada sonido carga).
- `tools/preset_audit.py` (nueva) — lee la biblioteca como datos: reparto por motor, uso de los cuatro
  envíos, cuántos sonidos llevan movimiento (`x[]`: LFO, matriz, filtro, tipo de distorsión, voz),
  categoría primaria y parecidos razonables entre dos presets del mismo motor. Los datos salen del propio
  firmware: `tests/preset_audit_test --dump` enlaza `engines.c` y escribe un JSON por preset, así que el
  informe no puede desviarse de la tabla C (un `AW_SAW3` o un `QG_SRC_VEL` no se adivinan con una regex).

Estado inicial: **212 presets en 9 motores**, 0 silenciosos, 0 fuera de rango, 0 nombres repetidos, 0
duplicados exactos. El problema no era la estructura, era el contenido:

| Motor | Presets | Con movimiento | DIST | CHORUS | DELAY | REVERB |
|---|---:|---:|---:|---:|---:|---:|
| ANALOG | 56 | 21 | 14 | 33 | 31 | 47 |
| PHASE | 25 | 5 | 9 | 32 | 29 | 53 |
| LOFI | 25 | 1 | 8 | 13 | 33 | 35 |
| VOICE | 20 | 4 | 8 | 48 | 29 | 71 |
| WHEEL | 18 | 2 | 14 | 22 | 19 | 59 |
| DRUM | 12 | 0 | 13 | 5 | 18 | 25 |
| NOISE | 17 | 4 | 7 | 31 | 29 | 76 |
| FM6 | 29 | 3 | 1 | 33 | 30 | 60 |
| WTABLE | 10 | 0 | 10 | 26 | 23 | 45 |

Diagnóstico:

1. **Solo 40 de 212 (19 %) aprovechaban la modulación.** La LFO, la matriz de modulación y el filtro de
   parte existían en el firmware y en los presets modernos, pero la mayoría de la biblioteca era estática:
   el mismo timbre de la nota al silencio, sin respuesta a la velocidad.
2. **La reverb era el envío por defecto de todo.** Medias de 60–76 sobre 127 en VOICE/NOISE/FM6: un wash
   común que igualaba pads, bajos, campanas y percusión. Ningún sonido usaba el envío a cero.
3. **Casi ningún bajo llevaba carácter**: DIST medio de 1–14 sobre 127, cuando el motor tiene `DT_SOFT`,
   `TAPE`, `HEARTH` y `FLUX` y los presets nuevos ya los usaban.
4. **Las categorías cubrían la biblioteca** (PADS 49, KEYS 38, TEXTURES 34, DRUMS 18, BASS 17, ORGANS 14,
   BELLS 13, LEADS 12, PLUCKS 10, ARPS 7) y el navegador ya agrupaba; no hacía falta un formato nuevo.

## 2. Qué se hizo con los 212 existentes

Ninguno se retiró y ninguno se renombró. Los 212 se reescribieron uno a uno en su propia tabla, sin tocar
la identidad del sonido (forma de onda, envolvente, filtro, semitonos) salvo donde el ajuste era el
objetivo:

- **Movimiento en 210 de los 212.** Cada sonido recibió el par que le corresponde por familia, nunca una
  plantilla única:
  - leads y bajos ácidos: LFO rápida al filtro + matriz `VEL → CUT` (la nota responde al toque) o `AT →
    VIB` (vibrato con aftertouch);
  - pads, drones y texturas: LFO lenta con onda `EVO` al filtro y a la amplitud, más `AT → CUT`;
  - teclas, campanas, mallets y plucks: `VEL → CUT`, que es lo que hace que un piano eléctrico o una
    campana suenen a instrumento y no a tono fijo;
  - bajos graves y subs: LFO muy leve o ninguna, sólo `VEL → CUT`; un sub que oscila deja de ser un sub;
  - percusión (DRUM): sin LFO, sólo `VEL → CUT`, porque un golpe no evoluciona;
  - órganos y cañas (WHEEL): movimiento de rotor/aire (`LD_AMP`) y `VEL → CUT`, sin vibrato donde el
    registro no lo tiene.
  Los dos kits `legacy` de DRUM (índices 10/11) se dejaron **byte a byte** como estaban: son el snapshot
  que un proyecto antiguo guardó y deben seguir cargando ese sonido exacto.
- **Reparto de envíos, sonido por sonido.** Se bajó la reverb donde emborronaba (bajos 20–50, percusión
  10–30, leads 24–40) y se subió el chorus donde el sonido lo pide (pads, cuerdas, órganos, coros), el
  delay donde hay ataque que repetir (plucks, arps, campanas) y la saturación donde el motor tiene drive
  (bajos, leads ácidos, órganos de rock). Resultado: ningún motor depende ya de la reverb y el envío a
  cero aparece en 9 sonidos que no necesitan espacio.
- **Sin FX inventados ni parámetros fuera de rango**: sólo los ids que `params.c` describe, y sólo los seis
  huecos de `x[6]`. Este límite se respetó a la primera comprobación estructural: un `x[]` con nueve
  entradas se trunca en silencio y deja, por ejemplo, un `VEL → CUT` sin cantidad (es decir, sin efecto).

## 3. Qué se añadió: 55 sonidos nuevos

La biblioteca pasa de **212 a 267** presets (251 navegables: WTABLE no entra en el generador de
categorías histórico, igual que antes, y los dos kits `legacy` siguen fuera del navegador). Los nuevos se
diseñaron por familia y repartidos entre motores, no concentrados en ANALOG/FM6:

| Familia | Nuevos | Dónde |
|---|---:|---|
| A. Leads, secuencias y arpegios | 7 | ANALOG (NEON RIDER, VELVET SCREAM, GLASS BLADE), LOFI (POCKET CONSOLE), WTABLE (SPECTRAL REED), PHASE (BRASS actualizado), arps ANALOG/LOFI (CLOCKWORK, CIUDAD DE PLAYA) |
| B. Pads, atmósferas, drones | 9 | ANALOG (PAPER LANTERN, SLOW TIDE), PHASE (MERCURY BLOOM), WTABLE (VELVET MORPH), VOICE (HALO CHORUS, ASHRAM), NOISE (SANDSTORM, COLD FUSION), WHEEL (CORAL ORGAN) |
| C. Campanas, pianos, mallets | 10 | FM6 (EMBER TINES, FROST MUSIC, DEEP MARIMBA, SUBMARINE BELL, TEMPLE CIRCUIT), PHASE (BELL FOUNDRY, CHORAL GLASS), ANALOG (AMBER GLASS, TIN ROOF), LOFI (CASIO DREAM) |
| D. Experimentales | 7 | PHASE (IRON FILAMENT, NULL POINT, STRAY SIGNAL), VOICE (TALK BOX WIRE, BROKEN HYMN), NOISE (BIT SWARM), WHEEL (BARKING PIPE) |
| E. Bajos | 4 | ANALOG (NIGHT SHIFT, MOLTEN CORE), LOFI (BRASS BUG), FM6 (ALLOY BASS) |
| F. Percusión y Drum-X | 7 | DRUM (CONCRETE KICK, GLASS SNARE, TIN HAT, TOMS OF MUD, RITUAL PERC, NEON CLAP), más NOISE (META DUST) como percusión metálica |
| G. Rock electrónico / música argentina | 7 | PUENTE DE LUZ (ANALOG, lead), SIESTA DORADA (ANALOG, pad), PIEDRA Y VIENTO (PHASE, textura), LUZ DE TUNEL (VOICE), CINTA MAGNETICA (FM6), BARRO Y LUZ (WTABLE), LLUVIA DE CENIZA (NOISE) |
| Teclas, órganos y plucks fuera de las familias | 4 | WHEEL (TONE CABINET, SALT REED), FM6 (PAPER ORGAN), WTABLE (CHROME PLUCK) |

Sobre la familia G: son sonidos **originales**, diseñados desde cero con los motores del FM-1. Las
referencias (Cerati, Spinetta, rock alternativo y electrónica atmosférica) se usaron como orientación
estética —lead expresivo con vibrato de aftertouch y delay largo, pad envolvente con LFO lenta, campana
delicada con cola de reverb, textura cíclica— y no como copia de preset, muestra ni patch comercial. Los
nombres son propios y en español, sin nombrar a nadie.

Distribución final por motor y por categoría:

| Motor | 6.0 | antes |
|---|---:|---:|
| ANALOG | 68 | 56 |
| PHASE | 32 | 25 |
| LOFI | 29 | 25 |
| VOICE | 25 | 20 |
| WHEEL | 22 | 18 |
| DRUM | 18 | 12 |
| NOISE | 22 | 17 |
| FM6 | 37 | 29 |
| WTABLE | 14 | 10 |
| **Total** | **267** | **212** |

Categorías primarias (regla documentada en `docs/PRESET_CATEGORIES.md`, generada de las tablas): PADS 57,
KEYS 56, TEXTURES 49, DRUMS 24, BASS 18, ORGANS 18, BELLS 15, LEADS 12, PLUCKS 11, ARPS 7. Los drones y
las atmósferas viven en PADS y TEXTURES, y los sonidos experimentales en TEXTURES, porque la taxonomía es
la que el navegador y los índices web ya consumen y cambiarla habría roto esa compatibilidad sin ganancia
de navegación.

## 4. Parámetros y FX aprovechados

- **Modulación**: `P_LRATE`/`P_LWAVE` (SIN, TRI, SAW, SQR, S&H, RAMP, EVO) con `P_LFADE`; destinos
  `P_LD_FLT`, `P_LD_AMP`, `P_LD_PIT`, `P_LD_SHP`; matriz `P_M1SRC`/`P_M1DST`/`P_M1AMT` con `QG_SRC_VEL`,
  `QG_SRC_AT` y destinos `QG_DST_CUT`, `QG_DST_VIB`, `QG_DST_REV`; `P_GLIDE` y `P_VOICE` en bajos y leads.
- **Motores**: ANALOG (SAW, SQR, TRI, SIN, PWM, SYNC, RING, SAW3, TUBE, FOLD + drive y filtro trapezoidal),
  PHASE (DCW, ENV, DETUNE, LINE MIX/RING, SUB, FB), LOFI (5 chips, 7 formas, duty, crush, bend, mask),
  VOICE (dos vocales, TALK, SHIFT, BUZZ, BREATH, Q, RAND), WHEEL (16 registros, sub/body/top, percusión,
  click, drive, rotor), DRUM (morph, tune, tone, decay, noise, FM env, fold, drive), NOISE (ANLG, DUST,
  LFSR, META con color, freq, peak, trk, dense, drift, crush), FM6 (algoritmos, feedback, mod, ratio, mega,
  vmod, detune sobre los bancos ROM), WTABLE (morph, detune, uni, sub, cut, res, drive, harm).
- **FX**: three buses de fábrica — `DIST` (SOFT, TAPE, HEARTH, FLUX con `P_DTONE`), `CHORUS` (con FLANGER
  como `CHORUS > TYPE`), `DELAY` y `REVERB` — más el filtro de parte (`P_FTYPE` LP/HP/BP/COMB/VOWEL/SCREAM
  con `P_FCUT`/`P_FRES`) en los motores que no traen filtro propio.

## 5. Resultados de las pruebas

Clase: **host x86 con el motor real**. Ninguna de estas pruebas toca una unidad física.

| Prueba | Resultado |
|---|---|
| `build/host/preset_audit_test` | **267 presets**: 0 silenciosos, 0 «hot», 0 nombres repetidos, 0 duplicados exactos, 0 fuera de rango — *passed (structure and signal; nobody listened)* |
| `tools/preset_audit.py` | 267/267 con movimiento (los 2 `legacy` de DRUM quedan fuera a propósito), 0 problemas de biblioteca |
| `build/preset_levels` + `tools/gen_preset_levels.py` | RMS medido de los 267; nivel recalculado y `firmware/src/preset_level.c` regenerado |
| `tests/preset_library_test.py` | 251 sonidos navegables, reparto por motor, índices de DRUM y `legacy` 10/11 intactos, 245 nombres únicos |
| `tests/preset_categories_test.py` | 251/251 con una categoría primaria, sin duplicados, `web/preset_categories.json` igual a la tabla |
| `build/regress tests/golden.txt tests/cpu_baseline.txt` | **285 renders** de referencia (los 267 sonidos + 18 escenas), 0 cambios, 0 fallos de salud, 0 de voz y ruteo, 0 entradas de CPU fuera de presupuesto, 0 crashes |
| `web/test_web.mjs` | tablas del mock del editor == firmware (0 diferencias) y resto de pruebas web en verde |
| `NO_PACKAGE=1 ./tests/run_tests.sh` | **ALL HOST TESTS PASSED** (suite completa, host x86) |
| `tools/build.py` (AC791N) | build de destino en verde: `.data+.bss` **87 760 B de 98 304** (10 544 B de holgura), imagen 577 672 B, código XIP 399 068 B, paquete 610 959 B |

Niveles de salida (misma medición, antes y después de recalcular el nivel de carga):

- dispersión de RMS entre presets: **31,5 dB → 11,7 dB**;
- pico máximo de la biblioteca: **−2,8 dBFS** (ningún preset recorta);
- mediana −18,5 dBFS, 241 de los 267 sonidos quedan fijados por el fader;
- **26 presets** necesitan más ganancia de la que el fader puede dar (+13,5 dB sobre 100) y quedan en su
  máximo: son los más silenciosos de la casa (AURORA FM, STATIC HAZE, GEIGER CNT, ABYSSAL DRONE, entre
  otros). Subirlos es un cambio de sonido —más `DENSE`/`COLOR` en NOISE, más `MOD` en FM6—, no del número.

Recursos (host x86, misma ejecución de la suite): el motor más caro por parte sigue siendo ANALOG con
~303 ns por muestra, y 12 voces repartidas en tres motores con batería cuestan ~338 ns por muestra
(1,11× la parte más cara). Los presets nuevos sólo añaden parámetros de control (LFO y matriz ya
existentes en el motor), no voces ni estado por voz, así que no mueven el presupuesto de 725 µs por bloque
de 32 muestras ni la RAM.

## 6. Herramientas y ficheros

- `tools/preset_audit.py` (nueva): auditoría de la biblioteca como datos.
- `tests/preset_audit_test.c`: nuevo modo `--dump` (un JSON por preset, desde las tablas del firmware).
- `firmware/src/eng_{analog,phase,lofi,formant,wheel,drum,noise,fm6,wavetable}.c`: las nueve tablas.
- `firmware/src/preset_level.c`: niveles regenerados.
- `docs/PRESET_CATEGORIES.md`, `web/preset_categories.json`: 251 entradas.
- `tools/gen_preset_categories.py`: el filtro de los kits `legacy` de DRUM ahora es por id (10/11) y no por
  posición, para que los kits nuevos no desaparecieran de la biblioteca navegable.
- `tools/generate_banner.py` (chip «9 ENGINES / 267 factory presets»), `web/emu/index.html`
  («267 Factory Presets»), `tests/run_tests.sh` (mensajes de conteo), `tests/preset_library_test.py` y
  `tests/preset_categories_test.py` (conteos esperados).

## 7. Lo que la biblioteca nueva obligó a arreglar fuera de las tablas

Ampliar la biblioteca destapó tres cosas que estaban apoyadas en el tamaño o en la suerte, no en una
garantía. Las tres se corrigieron en la causa:

1. **El navegador de presets** (`firmware/src/ui.c`): `LOFI_ORDER` fija el orden de audición de LOFI y lleva
   un `_Static_assert` contra `NELEM(LOFI_PRESETS)`. Con los cuatro sonidos nuevos la comprobación fallaba
   —el navegador no podía compilar—, así que el orden pasa a 29 entradas con cada sonido nuevo junto a su
   familia (lead, teclas, bajo, arp).
2. **El generador de piezas** (`firmware/src/gen.c`): `DICE ALL` promete que una tirada cambia al menos dos
   motores, y la prueba lo exigía en cada una de 30 tiradas. Nunca fue una garantía: los pesos de las
   recetas (55/30/15) pueden repetir motor, y **el árbol anterior a esta fase también falla la misma prueba
   con otras semillas** (2 de 60 tiradas, una de ellas sin ningún cambio de motor). Al mover la biblioteca
   cambió el flujo aleatorio y el fallo salió a la luz. Ahora `dice_all()` comprueba cuántos motores
   cambiaron y, si son menos de dos, cambia uno de los que no se movieron (nunca el kit de batería, nunca un
   motor retirado, FM6 sigue limitado a dos pistas). La prueba sigue exigiendo dos motores por tirada: ahora
   se cumple por construcción.
3. **Las pruebas que aíslan un parámetro del motor** (`tests/mod_test.c`, `tests/noise_test.c`): montaban su
   estado a partir de un preset de fábrica y después fijaban cada valor del motor a mano. Con todos los
   sonidos en movimiento, la LFO del preset seguía moviendo el filtro mientras la prueba medía dónde está
   («filter tracks the key» fallaba con 129/264/517 Hz en vez de 345/705/1416) o restaba profundidad al
   CUT («LOFI CUT darkens» quedaba en 0,52 en vez de <0,5). Ahora esas dos pruebas ponen a cero la LFO y la
   matriz después de cargar el preset, que es lo que ya hacían con los envíos: miden el parámetro, no el
   preset. Las pruebas de la matriz y de la LFO fijan sus propios slots y su propia velocidad después, así
   que no pierden nada.
4. **Un defecto real de un sonido nuevo**: BIT SWARM (NOISE, con `CRUSH` 60) tenía continua de 1,94 % de
   la escala —el recorte de bits sesga la media—. Se corrigió el sonido (menos `CRUSH`, más `DRIFT`): el
   peor caso de la biblioteca queda en 0,92 %.
5. **El golden de audio de seis pistas** (`tests/six_live_golden.txt`): son 61 hashes por segundo grabados
   con los presets anteriores, y su cometido es detectar cambios de DSP no buscados. Aquí el cambio de
   presets es el objetivo, así que se regrabó con `SIX_LIVE_ACCEPT=1` y se comprobó que una segunda
   ejecución da los mismos 61/61 hashes (reproducible, no una foto de una corrida).
6. **El registro de renders de referencia** (`tests/regress.c`, `tests/golden.txt`): el `regress` compila el
   ejecutable de la suite desde cero y su lista de trabajos no tenía tope (`J[512]` con 267 sonidos más las
   escenas). Se cambió a 1024 **con guarda**: si algún día no cabe, lo dice y sale con código 2 en vez de
   escribir fuera del array. Además ASan destapó que `chk_steal_fade` leía once notas de una tabla de nueve
   (`NVOICE` es 12 desde 3.8): la tabla pasa a once. Los 285 renders de referencia se regrabaron con
   `GOLDEN_UPDATE=1` —201 hashes cambian porque cada sonido tiene ahora movimiento y envíos distintos, y los
   55 nuevos añaden filas—, y una segunda pasada los reproduce con **0 cambios, 0 fallos de salud, 0
   crashes y 0 entradas de CPU fuera de presupuesto**.
7. **La salud de las campanas FM6 nuevas**: cinco de los ocho sonidos FM6 nuevos dejaban la voz ocupada
   doce segundos después del note-off (el chequeo de salud del `regress` lo cazó: «voices not free … peak
   12718»). La causa era la envolvente de parte larga (`SUS 0`, `REL 86–92`) sobre un motor cuyo decaimiento
   lo lleva el parche ROM: los timbres FM6 del banco usan la envolvente abierta (`0, 0, 127, 0`). Los cinco
   pasan a esa convención (los dos de teclado/órgano con un ataque corto y release de 25–40) y el chequeo
   de salud queda en 0 fallos.
8. **La tabla de espejo del editor** (`web/editor.html`): `web/test_web.mjs` compara los presets del mock
   del editor con las tablas del firmware (`build/host/desc.json`). Se añadieron los 57 registros que
   faltaban (55 nuevos más los dos kits `legacy`, que el mock nunca listó) desde el volcado del firmware,
   sin tocar ninguna fila existente —un primer intento casó nombres por texto y pisó `BRASS`/`BELL`/`ORGAN`
   de otros motores, que es justo lo que la prueba detectó—.
9. **Dos umbrales de prueba atados al timbre viejo**:
   - `tests/microstart_test.c` medía el primer sample del ataque en valor absoluto (2000) para detectar un
     click. El ataque de SAW LEAD ahora abre el filtro con la velocidad, así que el mismo sonido empieza
     legítimamente más brillante: la comprobación pasa a ser relativa al pico de la propia nota (el primer
     sample no supera el pico sostenido y el salto máximo no lo supera), que mide lo mismo —un click— sin
     medir el timbre.
   - `tests/fm6bank_test.c` cuenta los presets añadidos después del pool del generador: el guard pasa de 118
     a 169, con las comprobaciones por preset (valores extra, voz FM6, pico en rango) intactas.

## 8. Límites y pendientes

- **Nadie ha escuchado estos sonidos.** Las pruebas son de host: estructura, nivel, silencio, duplicados y
  el render del motor real. No hay juicio de mezcla ni de gusto, y ninguna prueba física.
- **26 presets por debajo de la mediana** que el fader no alcanza (ver arriba): subirlos exige retocar el
  sonido, no el nivel.
- **WTABLE sigue fuera de `tools/gen_preset_categories.py`** (y por tanto del JSON de categorías y de
  `docs/PRESET_CATEGORIES.md`): es anterior a esta fase y no se cambió para no alterar el índice que
  consume el editor; sus 14 sonidos sí aparecen en el firmware y en la auditoría.
- **Emulador y paquete**: los cambios son de tablas C. El emulador WebAssembly y el paquete de release se
  reconstruyen con `sh web/emu/build.sh` y el build de destino; este informe no afirma que la entrega
  publicada los incluya hasta que se reconstruya y se verifique el hash.
- **Sin pruebas en la unidad**: por instrucción expresa, el FM-1 físico no se tocó.
