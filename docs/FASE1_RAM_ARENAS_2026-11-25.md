# Fase 1/5 — RAM recuperada por arquitectura: las arenas de los motores (2026-11-25)

Clase de cada cifra: **[HOST x86-64]** los `sizeof` y el `.bss` del objeto host (gcc 13.3, `-O2`); **[TARGET AC791N]**
el mapa real de `build/felucca.map`, que **en esta ronda no se pudo producir** (ver §5). No se presenta ninguna
cifra de target como medida.

## 1. Lo que cambió

Dos uniones etiquetadas por motor, una por **pista** y otra por **ranura del pool**:

| arena | quién la usa | por qué es exclusiva |
|---|---|---|
| `eng_arena[NTRK][1216]` | FM6 (`fm6_note_t[FM6_POLY]`, 4 × 300 B) o DRUM (`drum_lane_t[DV_NLANE]`, 8 × 112 B) | una pista corre **un** motor; `voice.c engine_block` cambia `t->engine` sólo después de atenuar y terminar todas las voces del motor viejo |
| `slot_arena[VPOOLN][96]` + `slot_eng[VPOOLN]` | WHEEL (`drw_vc_t`, 92 B), ADDITIVE (`ad_vc_t`, 68 B) o NOISE (`nz_slot_t`, 12 B) | una ranura del pool sostiene **una** voz de **un** motor |

El acceso sigue siendo el mismo que antes para cada motor (`drum_kit_row(t)`, `fm6_note_row(t)`, `drw_slot(vi)`,
`ad_slot(vi)`, `nz_slot_of(vi)`), así que ningún camino de audio cambia de forma: los motores leen y escriben sus
estructuras como siempre, sólo que ahora comparten los bytes en vez de sumarlos.

## 2. Inventario medido, antes y después

`sizeof` **[HOST x86-64]**; estas estructuras no tienen punteros ni campos de 64 bits, así que su tamaño no depende
del ABI (int32/int16/uint8 con alineación 4):

| estructura | antes | total antes | después |
|---|---|---|---|
| `fm6_note_t` (FM6, por nota) | 300 B | `fm6_note[6][4]` = **7 200 B** | 1 fila de arena por pista |
| `drum_lane_t` (DRUM, por lane) | 112 B | `drum_kit[6][8]` = **5 376 B** | la misma fila de arena |
| `eng_arena` | — | — | `6 × 1 216` = **7 296 B** |
| `drw_vc_t` (WHEEL, por ranura) | 92 B | **1 656 B** | — |
| `ad_vc_t` (ADDITIVE, por ranura) | 68 B | **1 224 B** | — |
| ruido (NOISE, 3 × int32 por ranura) | 12 B | **216 B** | — |
| `slot_arena` + `slot_eng` | — | — | `18 × 96 + 18` = **1 746 B** |
| **total estado de motor** | | **15 672 B** | **9 042 B** |

**6 630 B devueltos** por la aritmética de los `sizeof`. El objeto host completo (todo el firmware, no sólo estas
tablas) baja **6 752 B** de `.bss` (`size -A tests/hostsim.c` compilado antes y después: **297 264 → 290 512 B**);
los 122 B de diferencia son alineación de globales vecinos, no estado.

`voice_t` (92 B), `vpool[18]` (1 656 B), `vmod_t`, `track_t`, `WT_GEN`, `drum_kit`/`fm6_note` **no se reducen por
recortar polifonía, motores, pistas ni calidad**: las 12 voces, los 10 motores, los 6 tracks y el render no cambian.
No se movió nada a RAMTEXT ni se sacó nada de una ruta de audio: las arenas siguen en `.bss` (SRAM, misma latencia).

## 3. Por qué es seguro (vidas útiles)

1. **Cambio de motor.** `engine_block` ya no cambiaba de motor con voces vivas: pide `XF_ENG` (16 bloques), renderiza
   el motor viejo con sus propios valores, y en el bloque del cambio pone `active = gate = 0` y `env = env_out = 0`
   en todas las voces. Recién entonces escribe `t->engine`. La fase añade **una** línea en ese punto: limpia la fila
   de arena de la pista. Así el motor nuevo nunca lee bytes del viejo (ni un `owner` de DRUM apuntando a una voz
   FM6), y las notas que el secuenciador/UI tenían en la cola (`xp_note_p`) se re-disparan sobre la fila limpia.
2. **Cambio de motor por preset / proyecto / favorito.** Todos pasan por `set_engine_of` → `eng_req` diferido y por
   `engine_block`; no hay una ruta que escriba `t->engine` a mano en el firmware (las de `tools/video` y los tests
   son arneses). `main.c` convierte un `engine` inválido en el arranque, antes de que suenen voces.
3. **Ranura compartida entre WHEEL / ADDITIVE / NOISE.** `voice_start` (voice.c) compara `slot_eng[ranura]` con el
   motor de la pista antes de `note_on`; si difiere, limpia la fila de 96 B y pone la etiqueta. Mismo motor: no toca
   nada, así que las ruedas de WHEEL siguen libres y su seed (`FELUCCA_WSEED`) no cambia. El coste es **un compare
   por note-on** y un `memset` de 96 B sólo la primera vez que un motor toma esa ranura.
4. **Nota sostenida, release, robo, unísono.** El estado de una voz no se mueve de ranura: `voice_window_grow` sólo
   presta ranuras que **no** suenan (`!active && !stage`), y el robo reutiliza la misma ranura con el motor de la
   pista. El release sigue en su ranura hasta que la voz se apaga; `voice_live` cuenta las que suenan.
5. **Seis pistas a la vez.** Cada pista tiene su fila; FM6 y DRUM pueden sonar simultáneamente en pistas distintas
   (probado: FM6, DRUM, WHEEL, ADDITIVE, NOISE, FM6 con 12 voces), y el cambio de una pista no toca las filas de las
   otras (probado comparando el estado vivo de la pista 6 y la ranura de WHEEL de la pista 3).
6. **Proyectos, presets y datos anteriores.** No cambia ningún formato, ID, índice ni campo persistido: el estado de
   los motores es efímero y no se guarda. Los 311 renders de `tests/golden.txt` y los 61 s de `six_live_golden.txt`
   salen **idénticos**, que es la prueba de que el sonido no cambió.

Lo que **no** se hizo: no se unieron los estados de motores que pueden sonar a la vez (FM6 de una pista con DRUM de
otra, ni WHEEL/ADDITIVE/NOISE de ranuras distintas). La unión es por propietario exclusivo, no un solapamiento a
ciegas.

## 4. Pruebas

| prueba | resultado |
|---|---|
| `tests/regress.c` (311 goldens, salud, voces/routing) | **311 renders, 0 cambiados, 0 fallos de salud, 0 de voces/routing** |
| `tests/six_track_live_test.c` (60 s, seis pistas, 10 voces, cambios de preset, punch-in) | **61/61 s idénticos a `six_live_golden.txt`**, sin clicks ni overflow |
| `tests/engine_arena_test.c` (**nuevo**) | **todas pasan**: geometría, 6 motores a la vez, FM6→DRUM con notas sostenidas, DRUM→FM6 con un golpe sonando, vuelta a DRUM, aislamiento entre filas, robo con tope 8, UNISON con cambio de motor, release total |
| `tests/drumx_test.c` | todas pasan |
| `tests/fm6_test.c` | todas pasan |
| `tests/ui_test.c` (incluye el cambio de motor con nota sostenida) | todas pasan |
| `tests/ratchet_test.c`, `tests/perform_test.c` | todas pasan |
| `tests/poly_six_test.c`, `tests/poly_stress_test.c`, `tests/audio_test.c` | todas pasan |

El test nuevo cubre explícitamente lo que la fase pidió: notas sostenidas al cambiar de motor, seis pistas, acordes,
release, unísono y voice stealing. En los casos con notas retenidas comprueba que la fila contiene **sólo** los lanes
que las notas re-disparadas tocan (un kit viejo habría dejado `owner` en los otros), que las demás pistas conservan
su estado, y que el pool drena a cero tras el release.

## 5. Presupuestos de memoria: lo que se pudo medir y lo que no

**[HOST x86-64]** medido en esta ronda: `.bss` del objeto completo **−6 752 B**; estado de motor **−6 630 B** por
`sizeof`; `.data` sin cambio (672 B); `.data.rel.ro.local` sin cambio (31 352 B).

**[TARGET AC791N] BLOQUEADO.** No hay `JIELI_TOOLCHAIN` ni `AC79_SDK` en este entorno (ni `.tools/`), así que no hay
`build/felucca.map` de esta ronda. Por lo tanto:

- **no** hay cifra medida de `.data+.bss`, POOL, LOWBUF, RAMTEXT ni NOINIT después del cambio, y **no** se subió
  ningún piso de `tests/mem_budget.json` ni de `tools/resource_map.py`;
- la última medición de target publicada (v2.5.1: RAM 17 756 B libres, LOWBUF 3 512, POOL 8 032 de holgura, NOINIT
  124, RAMTEXT 11 808) **no se toca en este documento**; lo que se afirma es que el cambio sólo puede reducir
  `.data+.bss` (ninguna sección crece: las arenas reemplazan arreglos estáticos y no se añadió código fuera de
  `voice_start`/`engine_block`);
- la aritmética de 6 630 B **debería** trasladarse al mapa de target sin cambios porque ninguna de estas estructuras
  contiene punteros ni campos de 64 bits; aun así es una expectativa, no una medición, y así queda escrita.

Cuando haya toolchain, la verificación es la de siempre:

```sh
./build.sh                                     # escribe build/felucca.map
python3 tools/mem_budget.py --show
python3 tools/ram_report.py --region RAM
```

RAMTEXT no se usó: no se movió ni un byte de datos a la región del código que corre con la flash ocupada. Las
arenas son `.bss` puro, con la misma velocidad de acceso que las tablas que reemplazan.

## 6. Coste

- Render: **cero** instrucciones nuevas por muestra; los motores acceden a la misma dirección de antes, con una
  indirección menos de tabla (la fila se calcula una vez por llamada, como ya se hacía con `drum_kit[t - trk]`).
- Note-on: un `compare` de etiqueta; un `memset` de 96 B sólo cuando otro motor toma la ranura.
- Cambio de motor: un `memset` de 1 216 B en el bloque del cambio, que ya recorría y limpiaba las 12 voces de la
  pista y ya hacía un fade de 16 bloques. No está en el camino por muestra.
- No se cambió ninguna ruta de audio, ningún buffer de FX, ni se redujo polifonía, motores, pistas ni presets.

## 7. Lo que queda medido para la próxima ronda

Los kilobytes grandes que **no** se tocaron siguen siendo `rev_comb` (~9 874 B), `trk[6]` (10 968 B), `dsp`/`WT_GEN`
(6 192 B) y los buffers de FX (`dly_buf` 131 072, `cv_buf` 59 520, `sl_buf`/`cloud_buf` 32 768). Recortarlos es
calidad sonora o visual, o tocar la persistencia, y esta fase lo prohíbe. La única duplicación de estado de motor
que quedaba era la que este cambio eliminó.
