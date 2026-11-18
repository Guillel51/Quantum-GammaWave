# El perf log: medir en la unidad, por USB serial

La ruta existe para una cosa: **medir en la FM-1**, no adivinarla. El firmware mantiene los contadores, la consola
los emite como CSV por el CDC-ACM, y un script del host los convierte en tres gráficos — por pantalla y por canción.

Nada de esto corre en el emulador ni en un test: lo que mide es la unidad. Un test host sólo prueba que el CSV se
lee y se dibuja.

## En la unidad

El CDC-ACM ya está: es la consola (`firmware/src/console.c`). No hay que compilar nada especial salvo que la build
se haya hecho con `FELUCCA_CDC=0`, en cuyo caso el dispositivo no presenta la función serial.

```
perf                una línea
perf on             una línea cada 250 ms
perf on 100         una línea cada 100 ms (mínimo 20: por debajo, la línea cuesta más de lo que muestra)
perf off            se detiene
```

La cadencia la marca el bucle principal (`cdc_task`), **nunca el ISR de audio**: un log que cambia lo que mide no es
una medición. Mientras el host no tenga DTR, no se emite nada; si el host deja de leer, la consola corta la línea en
vez de bloquear la UI.

### Los campos

```
perf,ms,halves,late,shed,t5_nested,max_us,last_us,cpu_q8,ui_frames,in_audio,stage,page,home,song
```

| campo | qué es |
|---|---|
| `ms` | `fm1_ms`, para el eje de tiempo |
| `halves` | mitades de audio renderizadas desde el arranque (su delta sobre el tiempo da 172 halves/s nominales) |
| `late` | mitades cuyo render pasó el deadline |
| `shed` | voces soltadas para entrar en el deadline (`audio.c` `shed_count`) |
| `t5_nested` | ticks de TIMER4 que TIMER5 pasó anidado dentro del render de la última mitad |
| `max_us` | la peor mitad desde el arranque (un número global, no de ventana: el ISR no mantiene ventana) |
| `last_us` | la última mitad |
| `cpu_q8` | fracción del half gastada en el render, Q8: 256 = una mitad entera |
| `ui_frames` | frames dibujados; su delta es el frame rate de la UI |
| `in_audio` | 1 mientras el render no volvió (una IRQ anidada) |
| `stage`/`page`/`home` | las migas de pan que el firmware ya lleva: la pantalla |
| `song` | `song_cur`: la canción cuyas secciones tiene `proj_slot` |

Los contadores son absolutos desde el arranque (`felucca_dbg` vive en `.noinit`), y por eso el host los diferencia:
así la unidad no tiene que llevar ninguna ventana.

## En el host

```
python3 tools/perf_log.py --port /dev/ttyACM0                  # en vivo
python3 tools/perf_log.py --file capture.log --json out/p.json # de una captura (tee de la consola)
python3 tools/perf_log.py - < capture.log                      # stdin
```

Escribe en `build/perf/`:

- `timeline.png` — CPU% del half en el tiempo, las pantallas como bandas, la canción como línea punteada, y abajo
  `late/min` y `shed/min` sobre un mismo eje.
- `by_screen.png` — por pantalla: media, p95 y pico de CPU, `late/min`, `shed/min`, fps.
- `by_song.png` — lo mismo por canción.

Y una tabla por stdout. `--screens "0/1/0=EDIT,0/2/0=FX"` y `--songs "0=MI TEMA"` ponen nombres a los números de las
migas de pan. `--port` necesita `pyserial`; desde un archivo no hace falta ninguna dependencia más que Pillow.

## Lo que este documento no puede decir

No hay unidad en este entorno: **nada de esto está medido en hardware**. La ruta compila (build AC791N), el parseo
y el dibujo están probados en host con una captura hecha a mano (`tests/perf_log_test.py`), y el costo en la imagen
está medido. Los números que produzca la unidad serán la primera medición real.
