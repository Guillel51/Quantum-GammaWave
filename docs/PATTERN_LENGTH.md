# Longitud de patrón por pista

La longitud del patrón **ya es de cada pista** y **ya viaja en el proyecto**: no hizo falta un formato nuevo ni
una migración, así que esta entrega no cambia comportamiento. Lo que faltaba era la prueba explícita, y eso es lo
que se añadió.

## Cómo está hecho

- **El parámetro**: `P_SLEN` (LEN, 1..64, por defecto 16) vive en `track_t.p[]`, no en `song.g[]`, o sea que hay
  uno por pista desde mucho antes de esta fase: es el id 30 de los 125 de `core.h`, dentro de los 53 que ya
  tenía el formato más viejo (`PROJ_NP_V2`).
- **El reloj**: `seq_tick()` avanza cada pista con su propio `seq_idx` y su propio `step_samples()` (DIV, swing,
  microtiming), y da la vuelta con `% len`, con `len = t->p[P_SLEN]`: no hay ningún contador de pasos compartido.
  El `bar_n` global existe, pero es el compás del transporte (MUTATE, cadena), no el largo del patrón.
- **El guardado**: `proj_pack()` escribe, por pista, solo los parámetros que difieren de su valor por defecto
  (mapa de presencia + bytes). `P_SLEN` está en ese conjunto desde el primer formato, así que 5, 7, 12 o 64 se
  guardan tal cual y se leen tal cual. Un proyecto que deja LEN en 16 no escribe nada y al cargarse vuelve a 16:
  eso es lo que hace que un proyecto viejo —o uno escrito por una versión que no tocaba LEN— siga sonando igual.
- **La migración**: `proj_import()` mapea los parámetros por cantidad (`params_by_count`); como el id de LEN (30)
  es menor que los 53 / 57 / 61 / 69 de los formatos FUN1..FUN5, todos lo llevan en su sitio. Los valores que un
  formato no traía caen a su default de tabla (`proj_pdef`), y LEN siempre estuvo.

## Evidencia (host x86)

- **`tests/pattern_len_test.c`** (nuevo, en `tests/run_tests.sh` y en el bucle ASan/UBSan):
  - dos pistas con LEN 4 y LEN 6, 60 pasos del secuenciador de verdad (`seq_tick`): cada una da la vuelta en su
    propio paso 0, **15 y 10 veces**;
  - un proyecto FUN6 con LEN 5 / 7 / 12 / 64 vuelve con esos cuatro largos, uno por pista;
  - el default es 16 y un proyecto con LEN sin tocar carga 16 en todas las pistas.
- **La canción de `tests/hostsim.c`** ya usa 16, 32 y 12 pasos en pistas distintas, y su render está fijado por
  los hashes de `tests/golden.txt`: si el secuenciador volviera a un largo compartido, esos hashes cambiarían.
- **Manual**: `docs/MANUAL.es.md` §6 ya lo describe («cada pista tiene su propio patrón de 64 pasos con su propio
  largo (LEN)…, así que patrones de distinto largo giran unos contra otros»).

## Dónde se edita

`SEQ > PATTERN`, KNOB 1 (LEN) de la pista seleccionada; el piano roll pagina los pasos 17..64 con las teclas
negras 10 / 11. El editor de escritorio y el Web Studio lo escriben como un parámetro más de la pista.

## Lo que no se hizo y por qué

- **No se tocó el formato del proyecto**: añadir un bloque para algo que ya se guarda por pista habría roto
  compatibilidad a cambio de nada.
- **No se cambió la UI**: la página PATTERN ya muestra y edita el LEN de la pista en la que estás.
- **Sin unidad física**: nada de esto está medido en el FM-1; todo es host x86 (`cc -O1`) con el código de
  producción.
