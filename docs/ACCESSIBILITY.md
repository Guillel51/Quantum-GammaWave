# Accesibilidad: FM-1 Screen Reader (issue #105)

Quantum GammaWave habla con el **FM-1 Screen Reader** de Charles Vestal
([charlesvestal/fm1-reader](https://github.com/charlesvestal/fm1-reader)) por el protocolo que ese proyecto
publica (`PROTOCOL.md`, versión 1). La implementación del firmware es propia
(`firmware/src/announce.c`), escrita contra esa especificación: no es una copia de `fm1-base/device/announce.c`
(MPL-2.0), así que el árbol sigue siendo GPL-3.0-only sin código de otra licencia.

## Qué manda el instrumento

SysEx por USB-MIDI: `F0 7D 53 52 <cmd> … F7` (`7D` = id de fabricante no comercial, `53 52` = "SR"). Todos
los bytes entre `F0` y `F7` son de 7 bits, así que cualquier otro equipo del mismo puerto lo ignora.

| Dirección | cmd | Significado |
|---|---|---|
| lector → equipo | 1 HELLO ver | empieza o sigue escuchando (se repite cada ~2 s; a los 5 s sin HELLO el equipo deja de hablar) |
| lector → equipo | 2 BYE | deja de escuchar |
| lector → equipo | 3 WHERE | di todo lo que hay en pantalla ahora |
| equipo → lector | 1 HELLO ver, nombre, 0, enlace | respuesta a cada HELLO (nombre: `Quantum GammaWave v1.4.17`) |
| equipo → lector | 16 SAY flags, texto | lo que hay que decir; flag 1 = corta lo que se está diciendo, 2 = aviso (se oye entero) |

El enlace a la lista de palabras (`ANN_WORDS_URL`) lleva, desde la v1.11.0, el archivo que este build publica
(`https://guillel51.github.io/Quantum-GammaWave/sr/w/<huella>.json`, ver arriba). Si el enlace faltara o el
lector estuviera sin red, aplica sus reglas comunes (números, `dB`, `%`, `C#3`, mayúsculas) y la lectura sigue
funcionando.

## Qué se anuncia hoy (v1.11.0)

- **La pantalla, al cambiar**: `HOME MAIN`, `VISUALS`, `OVERVIEW`, `MIXER`, `PERFORMANCE`, `MENU`, `CONFIRM`
  y el título propio de cada página (por ejemplo `ALGORITHM`, `PRESETS`, `FX`).
- **El valor de una tarjeta mientras se gira su perilla — en cualquier página** (v1.11.0). Hasta la 1.10 solo
  HOME tenía el concepto de "la tarjeta que se acaba de girar"; ahora la línea se pide con `page_desc()`, la misma
  llamada que dibujan las tarjetas y leen los encoders, así que EDIT, SEQ, FX, SCL, MOD y el resto dicen
  `CUTOFF 96` como HOME decía `LEVEL 96`. Se dice cuando el valor lleva ~120 ms quieto (y como mínimo cada
  ~480 ms si se sigue girando), que es lo que pide el protocolo.
- **La fila de una lista, con su lugar** (v1.11.0): `PATTERN 5 OF 40` en SEQ > PHRASES (la lista del cargador,
  fábrica + usuario) y `STEP 7 OF 16` en el cursor del secuenciador. Se dice cuando la selección se mueve, una vez
  por movimiento; WHERE la repite. El nombre del patrón se dice entero cuando se carga (`LOADED 03 NIGHT BASS`):
  repetirlo en la fila costaba 96 B de código de un slot que tenía 40.
- **El sonido cargado**: `ANALOG BRASS`, `FM6 TINE EP`, … al elegir preset (incluye el motor).
- **Los mensajes de la barra superior** (guardados, avisos, errores); los que llevan el icono de "sin archivo"
  se mandan como aviso (KEEP) para que no los corte el mensaje siguiente.
- **La fila de MENU, con su tab, su nombre y su valor** (v1.5.20): al moverse la selección, y su valor cuando el
  giro se asienta (250 ms), una vez por giro y no por paso.
- **WHERE**: al abrirse el lector se reanuncia la pantalla actual, la fila de MENU si está abierto, y la fila de la
  lista o el paso en las pantallas que la tienen.

## Las pronunciaciones, publicadas (v1.11.0)

`firmware/words.tsv` lista 60 abreviaturas que la pantalla muestra de verdad (comprobadas una a una contra los
`PD("...")` de `params.c`: `ATK` → attack, `FEEDBK` → feedback, `DLY` → delay, `CUT` → cutoff, …).
`tools/gen_words.py` las convierte en el JSON que pide el protocolo (`{"format":1,"words":[[from,to],…]}`), lo
nombra con la huella de su propio contenido y lo escribe en `site/sr/w/<huella>.json`, que se publica con el
sitio. El HELLO del instrumento lleva ese enlace exacto en `ANN_WORDS_URL`, y `gen_words.py --check` (en la suite)
falla si palabras, archivo publicado y enlace dejan de coincidir, o si el nombre + el enlace no caben en un
frame. Los números, `dB`, `%` y `C#3` no se listan: el lector aplica sus propias reglas.

## El emulador y Web Studio (v1.11.0)

La página del emulador compilaba su unidad **sin** `announce.c` (`FELUCCA_ANNOUNCE` quedaba en 0), así que el
dispositivo del navegador nunca hablaba: era la limitación 5 de esta lista. Ahora:

- `tests/hostsim.c` incluye `announce.c` antes de `usb.c`, en el mismo orden que `felucca.c`, y así el emulador
  tiene lector.
- `web/emu/felucca_web.c` exporta `web_sysex_byte(b)` (un byte de un frame del lector, con el framing del cable)
  y `web_ann_buf()` / `web_ann_take()`; `usb.c` pregunta `USB_WIRE_HOOK` antes de encolar, y el emulador toma el
  frame ahí porque no hay dispositivo USB que lo drene. En el firmware esa macro vale 0: el camino es el anillo
  de siempre.
- La página (`web/emu/index.html`) tiene un panel **Screen reader**: una casilla que manda HELLO cada 2 s y BYE al
  apagarse, un botón **Read the whole screen** (WHERE), y dos regiones `aria-live` — educada para las frases,
  asertiva para los avisos KEEP — además del registro de las últimas 200 líneas. El trabajo en el AudioWorklet
  (`worklet.js`) manda los frames al hilo de la página en cada `publish()`.
- `web/emu/emu_test.mjs` lo mide sin navegador: silencio antes de HELLO, la respuesta con nombre y enlace de
  palabras, `HOME MAIN` dicho, y WHERE dicho otra vez.

Web Studio (el editor web) no habla este protocolo: no es un lector, es un editor por SysEx propio. La
accesibilidad del emulador es la de su página.

## Lo que **no** se anuncia todavía (limitaciones reales)

1. **El nombre del patrón en la fila de la lista**: se dice la posición (`PATTERN 5 OF 40`) y el nombre al
   cargarlo, no en cada movimiento. Cuesta 96 B y el slot tenía 40 (v1.11.0).
2. **El aviso `QUEUE FULL`** cuando se llena la cola MIDI de entrada o el anillo SysEx de salida: medido en ~60 B,
   no cupo (quedaron 56). Los contadores existen (`midi_in_overflow`), y la carga de CPU ya se ve en la fila CPU
   LOAD de GLO > MONITOR; falta llevarlo a la barra (y por tanto al lector).
3. **Las páginas de acción sin valor propio** (LOAD, SAVE, CLEAR…) no dicen nada al girar su perilla porque no
   tienen número que decir; sí dicen su título.
4. **Nada de esto se probó en una FM-1 física ni con el lector real conectado**: ni la 1.3 ni la 1.11. El
   protocolo, el empaquetado USB-MIDI, la captura del SAY por el camino real (`ui_render --announce`) y el
   emulador (Node) están medidos; el cable y el altavoz, no.

## Qué cuesta

- Nada mientras no haya un lector: `ann_on()` es la primera comprobación de cada entrada y no se formatea ni se
  envía ninguna cadena. Un DAW en el mismo puerto no ve un solo byte.
- RAM: la estructura del protocolo (una línea en espera de 120 B, dos temporizadores, el estado) ≈ 150 B, más
  el anillo SysEx de salida (64 paquetes USB-MIDI = 256 B) que ya existía para M-UPGRADE. Ninguna reserva
  dinámica, ningún acceso a flash, ninguna espera: si el anillo no tiene sitio, el mensaje se descarta (el
  protocolo lo pide así).
- Audio: ni una llamada nueva desde el ISR. El envío ocurre en el bucle principal (`ep1_tx`, SysEx antes que
  cualquier nota) y el HELLO/BYE/WHERE se parsea donde ya se parseaba el SysEx de M-UPGRADE.

## Cómo probarlo

```bash
python3 -c "import sys; sys.path.insert(0,'tools'); import build; build.generate()"
cc -O1 -w -Ifirmware/src -o build/announce_test tests/announce_test.c && ./build/announce_test
```

`python3 tools/gen_words.py --check` comprueba el archivo de palabras (contenido, huella, enlace y tamaño del
frame). `node web/emu/emu_test.mjs build/emu/felucca.wasm` mide el camino del emulador: HELLO, la respuesta con
su enlace, la pantalla dicha, WHERE. `tests/announce_test.c` (host) mide el protocolo completo: silencio antes de HELLO, la respuesta con nombre y
versión, SAY/WHERE/BYE, el temporizador de 5 s, el corte a palabra completa y el **empaquetado USB-MIDI**
(CIN 4/5/6/7) que usa `usb.c`. En una FM-1 con esta versión: abrir
[charlesvestal.github.io/fm1-reader](https://charlesvestal.github.io/fm1-reader) en Chrome/Edge, conectar y
pulsar Connect; el lector debe anunciar `Quantum GammaWave v…` y la pantalla actual. **Eso no se pudo probar
aquí: no hubo unidad ni lector conectado.** La prueba con el lector real y con hardware queda pendiente y así
se registra en `docs/VALIDATION_MATRIX.md`.

## Alternativas autónomas estudiadas

- **Tonitor** y **Bigorna** llevan su propio lector dentro del firmware: hablan por la salida de auriculares y
  no necesitan computadora (se activan con una combinación de teclas). Dependen de que el firmware sintetice
  voz, lo que en la FM-1 significa ocupar el DAC y el motor de audio con habla, o añadir un sintetizador de
  voz. Para esta entrega se eligió el camino del Screen Reader (protocolo público, sin coste de audio y sin
  ocupar la salida); Tonitor/Bigorna quedan como alternativa documentada, no implementada.
